#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "sys_types.h"
#include "ipc_msg.h"
#include "qnet_utils.h"
#include "lx_fsm.h"
#include "lx_sensor.h"
#include "lx_comm.h"
#include "lx_watchdog.h"

/*
 * Lx entry point - one generic executable, deployed once per intersection
 * with its identity selected by argv[1] (1-6 -> L1-L6), per
 * docs/QNX_MOMENTICS_INTEGRATION.md's "Loads the specific ID configuration"
 * description of this file. Wires up the two-thread IPC pattern from
 * app/shared/README.md ("Threading pattern"), dispatching every verb/
 * pulse straight into lx_fsm.c, which owns phase sequencing and the
 * railway pre-emption/override supervisory overlay. Sensor debouncing,
 * signal-head actuation, and outgoing IPC belong in
 * lx_sensor.c/lx_signal.c/lx_comm.c respectively.
 */

typedef struct {
    controller_id_t     self_id;
    ipc_client_queue_t *client_queue;
    lx_fsm_t            fsm;
    /* PA-10: incremented by on_pulse() every IPC_PULSE_PHASE_TIMER tick;
     * polled by lx_watchdog.c (via a lx_watchdog_args_t pointing at this
     * field) to detect a stalled main loop. Belongs here, not in lx_fsm_t
     * - it is a liveness signal for lx_main.c's own server loop, not FSM
     * state. */
    volatile uint32_t    phase_tick_counter;
} intersection_context_t;

static void on_request(const ipc_request_t *req, ipc_reply_t *reply, void *ctx_ptr)
{
    intersection_context_t *ctx = (intersection_context_t *)ctx_ptr;

    switch ((msg_type_t)req->verb) {
    case MSG_SET_TIMING_PROFILE:
        lx_fsm_on_set_timing_profile(&ctx->fsm, &req->payload.timing_profile, reply);
        break;
    case MSG_SET_MODE:
        lx_fsm_on_set_mode(&ctx->fsm, &req->payload.mode, reply);
        break;
    case MSG_REQUEST_OVERRIDE:
        lx_fsm_on_request_override(&ctx->fsm, &req->payload.override_request, reply);
        break;
    case MSG_RENEW_OVERRIDE:
        lx_fsm_on_renew_override(&ctx->fsm, &req->payload.override_renew, reply);
        break;
    case MSG_CANCEL_OVERRIDE:
        /* No payload on the wire (envelope's target_id/sender_id fully
         * identify the active override) - see ipc_msg.h. */
        lx_fsm_on_cancel_override(&ctx->fsm, reply);
        break;
    case MSG_CROSSING_STATUS:
        /* Lx only ever reads this to feed the railway pre-emption
         * overlay (CC-02); it never replies with a command of its own
         * (RC-02: no Lx may command railway equipment). */
        lx_fsm_on_crossing_status(&ctx->fsm, &req->payload.crossing_status, reply);
        break;
    case MSG_REQUEST_FAULT_CLEAR:
        /* Test-plan finding: ipc_msg.h originally documented this verb as
         * C1->RLx only, leaving lx_fsm_local_fault_clear() as dead code
         * with no way to recover an Lx from FAULT_SAFE short of a process
         * restart. Wired up for symmetry with RLx's fault-clear path (no
         * payload on the wire, same as RLx's). */
        lx_fsm_on_request_fault_clear(&ctx->fsm, reply);
        break;
    default:
        reply->result = RESULT_ERROR;
        break;
    }
    reply->timestamp_ms = req->timestamp_ms;
}

static void on_pulse(int code, void *ctx_ptr)
{
    intersection_context_t *ctx = (intersection_context_t *)ctx_ptr;

    switch (code) {
    case IPC_PULSE_PHASE_TIMER:
        /* Single-step the phase sequencer - yellow/all-red clearance,
         * green extension recheck every 4 s (TL-01, TL-03), override
         * expiry. See lx_fsm.c's design-choice comment on this function
         * for why the timer stays fixed-period rather than being
         * re-armed per phase. */
        lx_fsm_on_phase_timer(&ctx->fsm);
        ctx->phase_tick_counter++;
        break;
    case IPC_PULSE_HEARTBEAT_TICK:
        lx_comm_send_heartbeat(ctx->self_id, &ctx->fsm, ctx->client_queue);
        break;
    default:
        break;
    }
}

static controller_id_t parse_self_id(int argc, char *argv[])
{
    int n;

    if (argc < 2) {
        return CTRL_UNKNOWN;
    }
    n = atoi(argv[1]);
    if (n < 1 || n > 6) {
        return CTRL_UNKNOWN;
    }
    return (controller_id_t)(CTRL_L1 + (n - 1));
}

int main(int argc, char *argv[])
{
    controller_id_t     self_id;
    int                 chid;
    pthread_t           client_tid;
    ipc_client_queue_t *client_queue;
    timer_t             phase_timer;
    timer_t             heartbeat_timer;
    intersection_context_t ctx;
    lx_watchdog_args_t  watchdog_args;

    self_id = parse_self_id(argc, argv);
    if (self_id == CTRL_UNKNOWN) {
        fprintf(stderr, "usage: %s <1-6>   (selects L1..L6)\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("%s (Intersection Controller) starting...\n", ipc_attach_name(self_id));

    chid = ipc_attach(self_id);
    if (chid == -1) {
        fprintf(stderr, "Lx: ipc_attach failed\n");
        return EXIT_FAILURE;
    }

    client_queue = ipc_client_queue_create();
    if (client_queue == NULL) {
        fprintf(stderr, "Lx: ipc_client_queue_create failed\n");
        return EXIT_FAILURE;
    }

    ctx.self_id           = self_id;
    lx_fsm_init(&ctx.fsm, self_id);
    ctx.client_queue      = client_queue;
    ctx.phase_tick_counter = 0;

    if (pthread_create(&client_tid, NULL, ipc_client_thread_main, client_queue) != 0) {
        fprintf(stderr, "Lx: failed to start client thread\n");
        return EXIT_FAILURE;
    }

    pthread_t sensor_tid;
    if (pthread_create(&sensor_tid, NULL, lx_sensor_reader_thread, &ctx.fsm) != 0) {
        fprintf(stderr, "Lx: failed to start sensor reader thread\n");
        return EXIT_FAILURE;
    }

    /* PA-10: local fail-safe watchdog thread. Needs both ->fsm and
     * ->phase_tick_counter, unlike lx_sensor_reader_thread above, so it
     * gets its own small args struct (watchdog_args) rather than &ctx.fsm
     * or &ctx directly - see lx_watchdog.h. watchdog_args lives in
     * main()'s stack frame, which stays alive for the process's whole
     * life (main() blocks forever in ipc_server_run() below). */
    watchdog_args.fsm = &ctx.fsm;
    watchdog_args.phase_tick_counter = &ctx.phase_tick_counter;
    pthread_t watchdog_tid;
    if (pthread_create(&watchdog_tid, NULL, lx_watchdog_thread, &watchdog_args) != 0) {
        fprintf(stderr, "Lx: failed to start watchdog thread\n");
        return EXIT_FAILURE;
    }

    /* 100 ms sequencer tick (spec-vi Muc 21: 100 ms +-50 ms jitter
     * budget) - deliberately left as a fixed period, never re-armed per
     * phase; lx_fsm_on_phase_timer() accumulates elapsed-ms itself and
     * compares against whichever threshold applies (see its design-
     * choice comment in lx_fsm.c for why). */
    if (ipc_timer_arm(chid, IPC_PULSE_PHASE_TIMER, 100, 100, &phase_timer) == -1) {
        fprintf(stderr, "Lx: failed to arm phase timer\n");
        return EXIT_FAILURE;
    }
    /* 1 s heartbeat cadence to C1 (PA-07). */
    if (ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, &heartbeat_timer) == -1) {
        fprintf(stderr, "Lx: failed to arm heartbeat timer\n");
        return EXIT_FAILURE;
    }

    printf("%s: attached on %s/%s, server loop starting.\n",
           ipc_attach_name(self_id), TRAFFIC_NAME_PREFIX, ipc_attach_name(self_id));

    /* Server thread: does not return in normal operation. Must stay the
     * ONLY code path in this process that calls MsgReceive()/MsgReply() -
     * every outgoing HEARTBEAT/STATUS goes through client_queue instead. */
    ipc_server_run(chid, on_request, on_pulse, &ctx);

    pthread_join(client_tid, NULL);
    pthread_join(sensor_tid, NULL);
    pthread_join(watchdog_tid, NULL);
    ipc_client_queue_destroy(client_queue);
    return EXIT_SUCCESS;
}
