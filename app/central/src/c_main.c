/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Team QNX
    Member: Tran Dong Nghi - s3914633
			Le Hung - s4061665
			Hoang Minh Thang - s3999925
    Assessment: 2 - Project Implementation 
    Due date: 18/09/2026
*/

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

// C1 entry point: initializes the central controller and wires up the multi-thread IPC pattern.

typedef struct {
    ipc_client_queue_t *client_queue;
    c_mode_eng_t         mode_eng;
    // Centralized context linking the client queue, mode engine, and synchronization locks.
    pthread_mutex_t       mode_eng_lock;
    // Serializes standard IO access to prevent HMI render logic from interrupting the operator prompt.
    pthread_mutex_t       console_io_lock;
} central_context_t;

// Safely logs a controller reconnection event using the console IO lock.
static void log_reconnect_if_needed(central_context_t *ctx, controller_id_t sender_id, int reconnected)
{
    if (!reconnected) {
        return;
    }
    pthread_mutex_lock(&ctx->console_io_lock);
    c_logger_log("Controller %d reconnected (PA-08)", (int)sender_id);
    pthread_mutex_unlock(&ctx->console_io_lock);
}

// Synchronously processes incoming status, heartbeat, fault, and crossing reports.
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
        reply->result = RESULT_ACK;
        break;
    case MSG_HEARTBEAT:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        reconnected = c_server_record_status(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.heartbeat.summary, req->timestamp_ms);
        pthread_mutex_unlock(&ctx->mode_eng_lock);

        log_reconnect_if_needed(ctx, (controller_id_t)req->sender_id, reconnected);
        reply->result = RESULT_ACK;
        break;
    case MSG_FAULT_REPORT:

        pthread_mutex_lock(&ctx->mode_eng_lock);
        c_server_record_fault_report(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.fault_report);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        
        pthread_mutex_lock(&ctx->console_io_lock);
        c_logger_log("FAULT_REPORT from %d: fault_code=0x%08x severity=%u detail=\"%s\"",
                     (int)req->sender_id, (unsigned)req->payload.fault_report.fault_code,
                     (unsigned)req->payload.fault_report.severity, req->payload.fault_report.detail);
        pthread_mutex_unlock(&ctx->console_io_lock);

        reply->result = RESULT_ACK;
        break;
    case MSG_CROSSING_STATUS:
        pthread_mutex_lock(&ctx->mode_eng_lock);
        reconnected = c_server_record_crossing_status(&ctx->mode_eng, (controller_id_t)req->sender_id, &req->payload.crossing_status);
        pthread_mutex_unlock(&ctx->mode_eng_lock);

        log_reconnect_if_needed(ctx, (controller_id_t)req->sender_id, reconnected);
        reply->result = RESULT_ACK;
        break;
    default:
        reply->result = RESULT_ERROR;
        break;
    }
    reply->timestamp_ms = req->timestamp_ms;
}

// Drives periodic heartbeat checks, automated peak-hour mode switching, and HMI rendering.
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
            pthread_mutex_lock(&ctx->console_io_lock);
            c_logger_log("Auto peak-hour switch: hour=%u -> mode=%s, broadcasting to all Lx",
                         (unsigned)current_hour,
                         (auto_mode == MODE_PEAK_FIXED) ? "PEAK_FIXED" : "OFF_PEAK_SENSOR");
            c_comm_broadcast_set_mode(ctx->client_queue, auto_mode);
            pthread_mutex_unlock(&ctx->console_io_lock);
        }

        pthread_mutex_lock(&ctx->console_io_lock);
        pthread_mutex_lock(&ctx->mode_eng_lock);
        c_hmi_render(&ctx->mode_eng);
        pthread_mutex_unlock(&ctx->mode_eng_lock);
        pthread_mutex_unlock(&ctx->console_io_lock);
        break;
    }
    default:
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
    
    c_comm_set_console_io_lock(&ctx.console_io_lock);

    if (pthread_create(&client_tid, NULL, ipc_client_thread_main, client_queue) != 0) {
        fprintf(stderr, "C1: failed to start client thread\n");
        return EXIT_FAILURE;
    }

    operator_args.client_queue = client_queue;
    operator_args.mode_eng     = &ctx.mode_eng;
    operator_args.mode_eng_lock = &ctx.mode_eng_lock;
    operator_args.console_io_lock = &ctx.console_io_lock;
    if (pthread_create(&operator_tid, NULL, c_operator_reader_thread, &operator_args) != 0) {
        fprintf(stderr, "C1: failed to start operator console thread\n");
        return EXIT_FAILURE;
    }

    if (ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, &heartbeat_check_timer) == -1) {
        fprintf(stderr, "C1: failed to arm heartbeat-check timer\n");
        return EXIT_FAILURE;
    }

    printf("C1: attached on %s/%s, server loop starting.\n", TRAFFIC_NAME_PREFIX, ipc_attach_name(CTRL_C1));

    ipc_server_run(chid, on_request, on_pulse, &ctx);

    pthread_join(client_tid, NULL); // Wait for the client thread to finish 
    pthread_join(operator_tid, NULL); // Wait for the operator thread to finish
    ipc_client_queue_destroy(client_queue); // Clean up the client queue
    return EXIT_SUCCESS;
}