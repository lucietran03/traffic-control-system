#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "sys_types.h"
#include "ipc_msg.h"
#include "qnet_utils.h"
#include "c_mode_eng.h"
#include "c_server.h"
#include "c_hmi.h"
#include "c_logger.h"
#include "c_watchdog_mon.h"
#include "c_operator.h"

/*
 * C1 entry point. Wires up the two-thread IPC pattern documented in
 * app/shared/README.md ("Threading pattern") and nothing else: the
 * server thread here never calls MsgSend(), the client thread here
 * never touches MsgReceive()/MsgReply(). Business logic (mode
 * selection, missed-heartbeat bookkeeping, override validation, HMI
 * rendering, persistent logging) belongs in c_mode_eng.c/c_watchdog_mon.c/
 * c_hmi.c/c_logger.c - all four are now implemented (c_mode_eng.c is a
 * pure decision layer with no IPC of its own; c_watchdog_mon.c/c_hmi.c/
 * c_logger.c are driven from here, from the 1 Hz IPC_PULSE_HEARTBEAT_TICK
 * and from the MSG_FAULT_REPORT request handler below).
 *
 * A third thread now exists too: c_operator.c's operator-console thread,
 * which originates every outgoing command (UC-03/UC-06 alt 7.1/UC-07/
 * UC-08 via c_comm.c) that this node sends. It never touches
 * MsgReceive()/MsgReply()/MsgSend() directly either - it only ever
 * enqueues onto client_queue via c_comm.c's senders, exactly like the
 * server/client threads already do.
 */

typedef struct {
    ipc_client_queue_t *client_queue;
    c_mode_eng_t         mode_eng;
    /* Was a TODO ("shared network-view state ... protected by a mutex,
     * once c_server.c/c_watchdog_mon.c exist") until the operator-console
     * thread (c_operator.c) was added: mode_eng was previously touched by
     * exactly one thread (the server thread, sequentially through
     * ipc_server_run()'s MsgReceive() loop), so no lock was load-bearing
     * yet. The operator thread is a second thread that reads/writes the
     * same eng->controllers[] slots (see c_operator.c's per-command
     * handlers), so this lock is now real, not aspirational. Taken here
     * only around the direct c_server_record_*()/c_watchdog_mon_tick()/
     * c_hmi_render() calls below - short and uncontended, per
     * ipc_server_run()'s non-blocking contract for on_request/on_pulse
     * (Lecture02 message-passing hazards). c_operator.c takes the same
     * lock via mode_eng_lock in its own c_operator_args_t. */
    pthread_mutex_t       mode_eng_lock;
} central_context_t;

static void on_request(const ipc_request_t *req, ipc_reply_t *reply, void *ctx_ptr)
{
    central_context_t *ctx = (central_context_t *)ctx_ptr;

    switch ((msg_type_t)req->verb) {
    case MSG_STATUS:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        c_server_record_status(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.status, req->timestamp_ms);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        /* UC-09 display refresh happens on the 1 Hz IPC_PULSE_HEARTBEAT_TICK
         * (see on_pulse() below), not per-request - no extra call needed here. */
        reply->result = RESULT_ACK;
        break;
    case MSG_HEARTBEAT:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        c_server_record_status(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.heartbeat.summary, req->timestamp_ms);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        /* UC-09 display refresh happens on the 1 Hz IPC_PULSE_HEARTBEAT_TICK
         * (see on_pulse() below), not per-request - no extra call needed here. */
        reply->result = RESULT_ACK;
        break;
    case MSG_FAULT_REPORT:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        c_server_record_fault_report(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.fault_report);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        c_logger_log("FAULT_REPORT from %d: fault_code=0x%08x severity=%u detail=\"%s\"",
                     (int)req->sender_id, (unsigned)req->payload.fault_report.fault_code,
                     (unsigned)req->payload.fault_report.severity, req->payload.fault_report.detail);
        /* UC-09 display refresh happens on the 1 Hz IPC_PULSE_HEARTBEAT_TICK
         * (see on_pulse() below), not per-request - no extra call needed here. */
        reply->result = RESULT_ACK;
        break;
    case MSG_CROSSING_STATUS:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        c_server_record_crossing_status(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.crossing_status);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        /* UC-09 display refresh happens on the 1 Hz IPC_PULSE_HEARTBEAT_TICK
         * (see on_pulse() below), not per-request - no extra call needed here.
         * Operator-initiated commands (SET_MODE/SET_TIMING_PROFILE/
         * REQUEST_OVERRIDE/RENEW_OVERRIDE/CANCEL_OVERRIDE/
         * REQUEST_FAULT_CLEAR) no longer belong here - they now originate
         * from c_operator.c's dedicated thread via c_comm.c's senders,
         * not from this inbound-request dispatch table (see main() below). */
        reply->result = RESULT_ACK;
        break;
    default:
        reply->result = RESULT_ERROR;
        break;
    }
    reply->timestamp_ms = req->timestamp_ms;
}

static void on_pulse(int code, void *ctx_ptr)
{
    central_context_t *ctx = (central_context_t *)ctx_ptr;

    switch (code) {
    case IPC_PULSE_HEARTBEAT_TICK:
        /* This node's own outgoing HEARTBEAT is sent by Lx/RLx, not C1 -
         * C1 is the HEARTBEAT server side, not a sender of it. */
        pthread_mutex_lock(&ctx->mode_eng_lock);
        c_watchdog_mon_tick(&ctx->mode_eng);
        c_hmi_render(&ctx->mode_eng);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        break;
    default:
        /* Includes the kernel's _PULSE_CODE_DISCONNECT - no action
         * needed; ipc_server_run() never replies to a pulse either way. */
        break;
    }
}

int main(void)
{
    int chid;
    pthread_t client_tid;
    pthread_t operator_tid;
    ipc_client_queue_t *client_queue;
    timer_t heartbeat_check_timer;
    central_context_t ctx;
    c_operator_args_t operator_args;

    printf("C1 (Central Controller) starting...\n");
    c_logger_init();

    chid = ipc_attach(CTRL_C1);
    if (chid == -1) {
        fprintf(stderr, "C1: ipc_attach failed\n");
        return EXIT_FAILURE;
    }

    client_queue = ipc_client_queue_create();
    if (client_queue == NULL) {
        fprintf(stderr, "C1: ipc_client_queue_create failed\n");
        return EXIT_FAILURE;
    }
    ctx.client_queue = client_queue;
    c_mode_eng_init(&ctx.mode_eng);
    pthread_mutex_init(&ctx.mode_eng_lock, NULL);

    if (pthread_create(&client_tid, NULL, ipc_client_thread_main, client_queue) != 0) {
        fprintf(stderr, "C1: failed to start client thread\n");
        return EXIT_FAILURE;
    }

    /* Operator-console thread (UC-03/UC-06 alt 7.1/UC-07/UC-08): reads
     * blocking stdin, same reason lx_main.c/rlx_main.c give their own
     * sensor-reader threads a dedicated thread rather than sharing the
     * server or client thread. operator_args lives in this stack frame,
     * which stays alive for the process's whole life (main() blocks
     * forever in ipc_server_run() below) - same pattern as lx_main.c's
     * watchdog_args. */
    operator_args.client_queue = client_queue;
    operator_args.mode_eng     = &ctx.mode_eng;
    operator_args.mode_eng_lock = &ctx.mode_eng_lock;
    if (pthread_create(&operator_tid, NULL, c_operator_reader_thread, &operator_args) != 0) {
        fprintf(stderr, "C1: failed to start operator console thread\n");
        return EXIT_FAILURE;
    }

    /* 1 s tick to drive missed-heartbeat bookkeeping (PA-07). */
    if (ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, &heartbeat_check_timer) == -1) {
        fprintf(stderr, "C1: failed to arm heartbeat-check timer\n");
        return EXIT_FAILURE;
    }

    printf("C1: attached on %s/%s, server loop starting.\n", TRAFFIC_NAME_PREFIX, ipc_attach_name(CTRL_C1));

    /* Server thread: does not return in normal operation. This must stay
     * the ONLY code path in this process that calls MsgReceive()/
     * MsgReply() - every outgoing command goes through client_queue
     * instead (see app/shared/README.md "Threading pattern"). */
    ipc_server_run(chid, on_request, on_pulse, &ctx);

    pthread_join(client_tid, NULL);
    pthread_join(operator_tid, NULL);
    ipc_client_queue_destroy(client_queue);
    return EXIT_SUCCESS;
}
