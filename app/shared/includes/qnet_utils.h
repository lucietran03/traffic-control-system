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

#ifndef QNET_UTILS_H
#define QNET_UTILS_H

#include <stdint.h>
#include <time.h>
#include <sys/neutrino.h>
#include "sys_types.h"
#include "ipc_msg.h"

// Qnet attach-point naming convention, resolved dynamically via TRAFFIC_NODE_MAP.
#define TRAFFIC_NAME_PREFIX "traffic"

// Returns the lowercase attach-point suffix for a given controller ID.
const char *ipc_attach_name(controller_id_t id);

// Attaches the node in the GLOBAL Qnet namespace and returns the channel ID.
int ipc_attach(controller_id_t self_id);

// Multiplexed pulse codes for dedicated node timers and heartbeats.
enum {
    IPC_PULSE_PHASE_TIMER = _PULSE_CODE_MINAVAIL, // Lx: yellow/all-red/green step, 4 s demand-extension recheck
    IPC_PULSE_HEARTBEAT_TICK,    // Every node: 1 s heartbeat cadence
    IPC_PULSE_RAILWAY_WARNING,   // RLx: 45 s warning-to-arrival budget (RC-03)
    IPC_PULSE_RAILWAY_OCCUPANCY  // RLx: 20 s per-direction occupancy window 
};

// Arms a timer to deliver the specified pulse code to the given channel.
int ipc_timer_arm(int chid, int pulse_code, uint32_t initial_ms, uint32_t period_ms, timer_t *out_timer_id);

/* --- server thread --- */

// Request handler callback for the non-blocking server thread.
typedef void (*ipc_request_handler_t)(const ipc_request_t *req, ipc_reply_t *reply, void *ctx);

// Pulse handler callback for the non-blocking server thread.
typedef void (*ipc_pulse_handler_t)(int code, void *ctx);

// Indefinite server thread loop for processing messages and pulses.
int ipc_server_run(int chid, ipc_request_handler_t on_request, ipc_pulse_handler_t on_pulse, void *ctx);

/* --- client thread / outgoing queue --- */

typedef struct ipc_client_queue ipc_client_queue_t;

// Callback for client thread when a send action completes or fails.
typedef void (*ipc_reply_handler_t)(
    controller_id_t target_id, 
    const ipc_request_t *original_req, 
    const ipc_reply_t *reply, 
    int send_ok, 
    void *ctx
);

// Creates a new client IPC queue.
ipc_client_queue_t *ipc_client_queue_create(void);

// Destroys the IPC queue once drained.
void ipc_client_queue_destroy(ipc_client_queue_t *q);

// Non-blocking: pushes a formatted request onto the target queue.
int ipc_client_post(ipc_client_queue_t *q, controller_id_t target_id, const ipc_request_t *req, ipc_reply_handler_t on_reply, void *ctx);

// Main client loop for consuming the message queue and invoking blocking MsgSends.
void *ipc_client_thread_main(void *queue);

#endif /* QNET_UTILS_H */
