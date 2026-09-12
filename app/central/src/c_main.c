#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "sys_types.h"
#include "ipc_msg.h"
#include "qnet_utils.h"
#include "c_mode_eng.h"
#include "c_server.h"
#include "c_hmi.h"
#include "c_logger.h"
#include "c_watchdog_mon.h"
#include "c_comm.h"
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
    /* Serialises this process's stdout/stdin: c_hmi_render()'s 1 Hz status
     * table, c_operator.c's interactive prompts, and the MSG_FAULT_REPORT
     * log line below all write to the same terminal from different
     * threads with zero coordination before this lock existed - during a
     * live demo the table would (and did) splice itself into the middle
     * of an operator prompt. ALWAYS acquired before mode_eng_lock, never
     * the reverse - see on_pulse() below; c_operator.c's reader-thread
     * switch takes it the same way, as the outer lock around each
     * handle_*() call. */
    pthread_mutex_t       console_io_lock;
} central_context_t;

/* PA-08: logs the reconnect edge c_server_record_status()/
 * record_crossing_status() detect (return value 1), under console_io_lock
 * - same lock-then-release-then-log-separately pattern used for
 * c_watchdog_mon_tick()'s newly_unavailable array (mode_eng_lock is never
 * held while console_io_lock is taken). No-op when reconnected==0. */
static void log_reconnect_if_needed(central_context_t *ctx, controller_id_t sender_id, int reconnected)
{
    if (!reconnected) {
        return;
    }
    pthread_mutex_lock(&ctx->console_io_lock);
    c_logger_log("Controller %d reconnected (PA-08)", (int)sender_id);
    pthread_mutex_unlock(&ctx->console_io_lock);
}

static void on_request(const ipc_request_t *req, ipc_reply_t *reply, void *ctx_ptr)
{
    central_context_t *ctx = (central_context_t *)ctx_ptr;
    int reconnected;

    switch ((msg_type_t)req->verb) {
    case MSG_STATUS:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        reconnected = c_server_record_status(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.status, req->timestamp_ms);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        log_reconnect_if_needed(ctx, (controller_id_t)req->sender_id, reconnected);
        /* UC-09 display refresh happens on the 1 Hz IPC_PULSE_HEARTBEAT_TICK
         * (see on_pulse() below), not per-request - no extra call needed here. */
        reply->result = RESULT_ACK;
        break;
    case MSG_HEARTBEAT:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        reconnected = c_server_record_status(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.heartbeat.summary, req->timestamp_ms);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        log_reconnect_if_needed(ctx, (controller_id_t)req->sender_id, reconnected);
        /* UC-09 display refresh happens on the 1 Hz IPC_PULSE_HEARTBEAT_TICK
         * (see on_pulse() below), not per-request - no extra call needed here. */
        reply->result = RESULT_ACK;
        break;
    case MSG_FAULT_REPORT:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        c_server_record_fault_report(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.fault_report);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        /* A fault report can arrive asynchronously at any time, from the
         * server thread - same terminal-splicing hazard c_hmi_render()'s
         * status table has, so this log line needs the same console_io_lock
         * (mode_eng_lock is already released by this point, so this is a
         * pure addition, not a lock-ordering change). */
        pthread_mutex_lock(&ctx->console_io_lock);
        c_logger_log("FAULT_REPORT from %d: fault_code=0x%08x severity=%u detail=\"%s\"",
                     (int)req->sender_id, (unsigned)req->payload.fault_report.fault_code,
                     (unsigned)req->payload.fault_report.severity, req->payload.fault_report.detail);
        pthread_mutex_unlock(&ctx->console_io_lock);
        /* UC-09 display refresh happens on the 1 Hz IPC_PULSE_HEARTBEAT_TICK
         * (see on_pulse() below), not per-request - no extra call needed here. */
        reply->result = RESULT_ACK;
        break;
    case MSG_CROSSING_STATUS:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        reconnected = c_server_record_crossing_status(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.crossing_status);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        log_reconnect_if_needed(ctx, (controller_id_t)req->sender_id, reconnected);
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
    case IPC_PULSE_HEARTBEAT_TICK: {
        uint8_t current_hour;
        operating_mode_t auto_mode = MODE_PEAK_FIXED;
        int mode_changed;
        controller_id_t newly_unavailable[9];
        int n_unavailable;
        int i;

        /* This node's own outgoing HEARTBEAT is sent by Lx/RLx, not C1 -
         * C1 is the HEARTBEAT server side, not a sender of it.
         * c_watchdog_mon_tick() deliberately does not log itself (it runs
         * under mode_eng_lock, and c_logger_log() needs console_io_lock -
         * see c_watchdog_mon.h) - this function logs each newly-stale
         * controller itself, below, after releasing mode_eng_lock. */
        pthread_mutex_lock(&ctx->mode_eng_lock);
        n_unavailable = c_watchdog_mon_tick(&ctx->mode_eng, newly_unavailable);
        pthread_mutex_unlock(&ctx->mode_eng_lock);

        if (n_unavailable > 0) {
            pthread_mutex_lock(&ctx->console_io_lock);
            for (i = 0; i < n_unavailable; i++) {
                c_logger_log("Controller %d marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)",
                             (int)newly_unavailable[i]);
            }
            pthread_mutex_unlock(&ctx->console_io_lock);
        }

        /* DP-01/DP-02 peak-hour auto-switch: reads the real wall clock
         * unless a demo hour is currently forced via c_operator.c's 'd'
         * command, in which case that forced hour stands in for "now"
         * (undone by 'a'). Broadcasts SET_MODE to every Lx only when the
         * schedule-implied mode actually changes since the last check
         * (c_mode_eng_auto_check()) - never every tick, and it does not
         * fight a manual 'm' command issued between boundary crossings,
         * matching the "defer to next safe boundary" pattern already used
         * for mode changes at the Lx level (lx_fsm.c). */
        pthread_mutex_lock(&ctx->mode_eng_lock);
        if (ctx->mode_eng.demo_hour_override_active) {
            current_hour = ctx->mode_eng.demo_hour;
        } else {
            time_t now = time(NULL);
            struct tm tm_now;

            localtime_r(&now, &tm_now);
            current_hour = (uint8_t)tm_now.tm_hour;
        }
        mode_changed = c_mode_eng_auto_check(&ctx->mode_eng, current_hour, &auto_mode);
        if (mode_changed) {
            c_mode_eng_mark_all_lx_commanded(&ctx->mode_eng, auto_mode);
        }
        pthread_mutex_unlock(&ctx->mode_eng_lock);

        if (mode_changed) {
            /* Never call a blocking/queueing send while mode_eng_lock is
             * held - same convention already used by every c_operator.c
             * handler. console_io_lock only guards the log line here (its
             * own splice hazard, same as the MSG_FAULT_REPORT case above);
             * c_comm_broadcast_set_mode() itself needs no lock. */
            pthread_mutex_lock(&ctx->console_io_lock);
            c_logger_log("Auto peak-hour switch: hour=%u -> mode=%s, broadcasting to all Lx",
                         (unsigned)current_hour,
                         (auto_mode == MODE_PEAK_FIXED) ? "PEAK_FIXED" : "OFF_PEAK_SENSOR");
            pthread_mutex_unlock(&ctx->console_io_lock);
            c_comm_broadcast_set_mode(ctx->client_queue, auto_mode);
        }

        /* console_io_lock is always acquired BEFORE mode_eng_lock, never
         * the reverse - c_operator.c's reader-thread switch takes
         * console_io_lock as its outer lock around each handle_*() call,
         * which internally takes mode_eng_lock; nesting them the other
         * way here would risk an AB-BA deadlock the first time a status
         * tick and an operator command race. */
        pthread_mutex_lock(&ctx->console_io_lock);
        pthread_mutex_lock(&ctx->mode_eng_lock);
        c_hmi_render(&ctx->mode_eng);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        pthread_mutex_unlock(&ctx->console_io_lock);
        break;
    }
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
    pthread_mutex_init(&ctx.console_io_lock, NULL);
    /* Must happen before the client thread starts below - see
     * c_comm_set_console_io_lock()'s doc comment in c_comm.h. */
    c_comm_set_console_io_lock(&ctx.console_io_lock);

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
    operator_args.console_io_lock = &ctx.console_io_lock;
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
