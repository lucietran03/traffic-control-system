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

#include "sys_types.h"
#include "ipc_msg.h"
#include "qnet_utils.h"
#include "lx_fsm.h"
#include "lx_sensor.h"
#include "lx_comm.h"
#include "lx_watchdog.h"

// Intersection entry point mapping incoming Qnet requests and timer pulses directly to the underlying FSM logic.

typedef struct {
    controller_id_t     self_id;
    ipc_client_queue_t *client_queue;
    lx_fsm_t            fsm;
    // Shared counter polled by the watchdog thread to assert main loop liveness.
    volatile uint32_t    phase_tick_counter;
} intersection_context_t;

// Invoked sequentially by the server thread to handle incoming IPC messages.
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
        lx_fsm_on_cancel_override(&ctx->fsm, reply);
        break;
    case MSG_CROSSING_STATUS:
        lx_fsm_on_crossing_status(&ctx->fsm, &req->payload.crossing_status, reply);
        break;
    case MSG_REQUEST_FAULT_CLEAR:
        lx_fsm_on_request_fault_clear(&ctx->fsm, reply);
        break;
    default:
        reply->result = RESULT_ERROR;
        break;
    }
    reply->timestamp_ms = req->timestamp_ms;
}

// Drives fixed-rate tick updates for the phase timer and heartbeats.
static void on_pulse(int code, void *ctx_ptr)
{
    intersection_context_t *ctx = (intersection_context_t *)ctx_ptr;

    switch (code) {
    case IPC_PULSE_PHASE_TIMER:
        lx_fsm_on_phase_timer(&ctx->fsm);
        ctx->phase_tick_counter++;
        break;
    case IPC_PULSE_HEARTBEAT_TICK:
        lx_comm_send_heartbeat(ctx->self_id, &ctx->fsm, ctx->client_queue);
        lx_fsm_local_clock_mode_check(&ctx->fsm);
        break;
    default:
        break;
    }
}

// Identifies the instance identity from runtime arguments.
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

// Main entry point for the intersection controller.
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

    watchdog_args.fsm = &ctx.fsm;
    watchdog_args.phase_tick_counter = &ctx.phase_tick_counter;
    pthread_t watchdog_tid;
    if (pthread_create(&watchdog_tid, NULL, lx_watchdog_thread, &watchdog_args) != 0) {
        fprintf(stderr, "Lx: failed to start watchdog thread\n");
        return EXIT_FAILURE;
    }

    if (ipc_timer_arm(chid, IPC_PULSE_PHASE_TIMER, 100, 100, &phase_timer) == -1) {
        fprintf(stderr, "Lx: failed to arm phase timer\n");
        return EXIT_FAILURE;
    }
    
    if (ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, &heartbeat_timer) == -1) {
        fprintf(stderr, "Lx: failed to arm heartbeat timer\n");
        return EXIT_FAILURE;
    }

    printf("%s: attached on %s/%s, server loop starting.\n",
           ipc_attach_name(self_id), TRAFFIC_NAME_PREFIX, ipc_attach_name(self_id));

    ipc_server_run(chid, on_request, on_pulse, &ctx);

    pthread_join(client_tid, NULL); // Wait for the client thread to finish
    pthread_join(sensor_tid, NULL); // Wait for the sensor reader thread to finish
    pthread_join(watchdog_tid, NULL); // Wait for the watchdog thread to finish
    ipc_client_queue_destroy(client_queue); // Clean up the client queue
    return EXIT_SUCCESS;
}