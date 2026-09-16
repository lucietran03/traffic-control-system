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
#include "rlx_fsm.h"
#include "rlx_sensor.h"
#include "rlx_gate.h"
#include "rlx_comm.h"
#include "rlx_watchdog.h"

// Main entry point for the Railway Controller executing IPC patterns while delegating domain logic to FSM modules.

typedef struct {
    controller_id_t     self_id;
    ipc_client_queue_t *client_queue;
    rlx_fsm_t           fsm;
    // Watchdog counter tracking FSM ticks for stall detection.
    volatile uint32_t   tick_counter;   
} railway_context_t;

// Handles incoming IPC requests, delegating to the FSM for fault clear requests and returning errors for unsupported verbs.
static void on_request(const ipc_request_t *req, ipc_reply_t *reply, void *ctx_ptr)
{
    railway_context_t *ctx = (railway_context_t *)ctx_ptr;

    switch ((msg_type_t)req->verb) {
    case MSG_REQUEST_FAULT_CLEAR:
        rlx_fsm_on_fault_clear(&ctx->fsm, reply);
        break;
    default:
        reply->result = RESULT_ERROR;
        break;
    }
    reply->timestamp_ms = req->timestamp_ms;
}

// Pulse handler for the Railway Controller, processing warning ticks, occupancy ticks, and heartbeat ticks to drive FSM state transitions and communication.
static void on_pulse(int code, void *ctx_ptr)
{
    railway_context_t *ctx = (railway_context_t *)ctx_ptr;

    switch (code) {
    case IPC_PULSE_RAILWAY_WARNING:
        // Leverages the warning tick pulse to drive both the warning sequence and the occupancy window countdowns.
        rlx_fsm_on_tick(&ctx->fsm);
        ctx->tick_counter++;
        if (rlx_fsm_take_fault_report_pending(&ctx->fsm)) {
            rlx_comm_send_fault_report(ctx->self_id, &ctx->fsm, ctx->client_queue);
        }
        rlx_comm_broadcast_crossing_status_if_changed(ctx->self_id, &ctx->fsm, ctx->client_queue);
        break;
    case IPC_PULSE_RAILWAY_OCCUPANCY:
        // Occupancy pulse remains unarmed since the warning pulse already handles occupancy evaluation.
        break;
    case IPC_PULSE_HEARTBEAT_TICK:
        rlx_comm_send_heartbeat(ctx->self_id, &ctx->fsm, ctx->client_queue);
        break;
    default:
        break;
    }
}

// Parses the command-line argument to determine the railway controller ID, returning CTRL_UNKNOWN for invalid inputs.
static controller_id_t parse_self_id(int argc, char *argv[])
{
    int n;

    if (argc < 2) {
        return CTRL_UNKNOWN;
    }
    n = atoi(argv[1]);
    if (n < 1 || n > 3) {
        return CTRL_UNKNOWN;
    }
    return (controller_id_t)(CTRL_RL1 + (n - 1));
}

// Main function initializing the Railway Controller, setting up IPC, threads, timers, and entering the server loop to handle requests and pulses.
int main(int argc, char *argv[])
{
    controller_id_t     self_id;
    int                 chid;
    pthread_t           client_tid;
    pthread_t           sensor_tid;
    pthread_t           watchdog_tid;
    ipc_client_queue_t *client_queue;
    timer_t             heartbeat_timer;
    timer_t             railway_tick_timer;
    railway_context_t   ctx;
    rlx_watchdog_args_t watchdog_args;

    self_id = parse_self_id(argc, argv);
    if (self_id == CTRL_UNKNOWN) {
        fprintf(stderr, "usage: %s <1-3>   (selects RL1..RL3)\n", argv[0]);
        return EXIT_FAILURE;
    }

    printf("%s (Railway Controller) starting...\n", ipc_attach_name(self_id));

    chid = ipc_attach(self_id);
    if (chid == -1) {
        fprintf(stderr, "RLx: ipc_attach failed\n");
        return EXIT_FAILURE;
    }

    client_queue = ipc_client_queue_create();
    if (client_queue == NULL) {
        fprintf(stderr, "RLx: ipc_client_queue_create failed\n");
        return EXIT_FAILURE;
    }

    ctx.self_id      = self_id;
    ctx.client_queue = client_queue;
    ctx.tick_counter = 0;
    rlx_fsm_init(&ctx.fsm, self_id);
    rlx_gate_init();

    if (pthread_create(&client_tid, NULL, ipc_client_thread_main, client_queue) != 0) {
        fprintf(stderr, "RLx: failed to start client thread\n");
        return EXIT_FAILURE;
    }

    if (pthread_create(&sensor_tid, NULL, rlx_sensor_reader_thread, &ctx.fsm) != 0) {
        fprintf(stderr, "RLx: failed to start sensor reader thread\n");
        return EXIT_FAILURE;
    }

    watchdog_args.fsm = &ctx.fsm;
    watchdog_args.tick_counter = &ctx.tick_counter;
    if (pthread_create(&watchdog_tid, NULL, rlx_watchdog_thread, &watchdog_args) != 0) {
        fprintf(stderr, "RLx: failed to start watchdog thread\n");
        return EXIT_FAILURE;
    }

    // Arms the 1-second heartbeat timer mapped to Central.
    if (ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, &heartbeat_timer) == -1) {
        fprintf(stderr, "RLx: failed to arm heartbeat timer\n");
        return EXIT_FAILURE;
    }

    // Arms the single 1-second warning timer to advance all crossing FSM budgets.
    if (ipc_timer_arm(chid, IPC_PULSE_RAILWAY_WARNING, 1000, 1000, &railway_tick_timer) == -1) {
        fprintf(stderr, "RLx: failed to arm railway FSM tick timer\n");
        return EXIT_FAILURE;
    }

    printf("%s: attached on %s/%s, server loop starting.\n",
           ipc_attach_name(self_id), TRAFFIC_NAME_PREFIX, ipc_attach_name(self_id));

    // Prevents main thread return to guarantee the server loop handles incoming communications.
    ipc_server_run(chid, on_request, on_pulse, &ctx);

    pthread_join(client_tid, NULL); // Waits for the client thread to finish.
    pthread_join(sensor_tid, NULL); // Waits for the sensor reader thread to finish.
    pthread_join(watchdog_tid, NULL); // Waits for the watchdog thread to finish.
    ipc_client_queue_destroy(client_queue); // Cleans up the client queue resources.
    return EXIT_SUCCESS;
}