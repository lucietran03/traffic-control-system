#include <stdio.h>
#include <string.h>

#include "rlx_comm.h"

// Shared callback that logs transmission failures for fault and crossing status reports.
static void on_reply_log_failure(controller_id_t target_id, const ipc_request_t *original_req,
                                  const ipc_reply_t *reply, int send_ok, void *ctx)
{
    (void)original_req;
    (void)reply;
    (void)ctx;
    if (!send_ok) {
        fprintf(stderr, "RLx: message to %d failed to send\n", (int)target_id);
    }
}

// Client-thread callback to track heartbeat ACKs and report connection states to the FSM.
static void on_heartbeat_reply(controller_id_t target_id, const ipc_request_t *original_req,
                                const ipc_reply_t *reply, int send_ok, void *ctx)
{
    rlx_fsm_t *fsm = (rlx_fsm_t *)ctx;
    int acked = send_ok && reply != NULL && (msg_result_t)reply->result == RESULT_ACK;
    int transition;

    (void)original_req;

    if (!send_ok) {
        fprintf(stderr, "RLx: HEARTBEAT to %d failed to send\n", (int)target_id);
    }

    transition = rlx_fsm_on_heartbeat_result(fsm, acked);
    if (transition == 1) {
        fprintf(stderr, "RLx: 3 consecutive HEARTBEATs unacknowledged - entering DEGRADED_LOCAL (PA-07)\n");
    } else if (transition == 2) {
        fprintf(stderr, "RLx: HEARTBEAT acknowledged by C1 - reconnected, resuming CENTRAL_CONNECTED (PA-08)\n");
    }
}

// Sends a heartbeat message to C1 with the current railway status, logging any enqueue failures.
void rlx_comm_send_heartbeat(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb = MSG_HEARTBEAT;
    req.sender_id = (uint32_t)self_id;
    req.target_id = (uint32_t)CTRL_C1;
    // Known gap: placeholder timestamp of 0 used until a monotonic-clock helper is implemented.
    req.timestamp_ms = 0;

    // Fills role-specific status details without modifying non-railway fields.
    rlx_fsm_fill_status(fsm, &req.payload.heartbeat.summary);

    // Logs unacknowledged sends as heartbeat drops if the queue is full or stopped.
    if (ipc_client_post(client_queue, CTRL_C1, &req, on_heartbeat_reply, fsm) != 0) {
        fprintf(stderr, "RLx: HEARTBEAT to C1 dropped - outgoing queue full or stopping\n");
        (void)rlx_fsm_on_heartbeat_result(fsm, 0);
    }
}

// Sends a fault report to C1 with the current railway status, logging any enqueue failures.
void rlx_comm_send_fault_report(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue)
{
    ipc_request_t req;
    status_report_payload_t tmp_status;

    memset(&req, 0, sizeof(req));
    req.verb = MSG_FAULT_REPORT;
    req.sender_id = (uint32_t)self_id;
    req.target_id = (uint32_t)CTRL_C1;
    req.timestamp_ms = 0;

    rlx_fsm_fill_status(fsm, &tmp_status); 
    req.payload.fault_report.fault_code = (uint32_t)tmp_status.faults;
    // Placeholder severity level; no classification scheme is currently designed.
    req.payload.fault_report.severity = 1; 
    strncpy(req.payload.fault_report.detail, "RLx fault - see fault_code bitmask",
            sizeof(req.payload.fault_report.detail) - 1);
    req.payload.fault_report.detail[sizeof(req.payload.fault_report.detail) - 1] = '\0';

    // Posts fault reports independent of local safe-state responses, logging enqueue drops.
    if (ipc_client_post(client_queue, CTRL_C1, &req, on_reply_log_failure, NULL) != 0) {
        fprintf(stderr, "RLx: FAULT_REPORT to C1 dropped - outgoing queue full or stopping\n");
    }
}

// Defines physical adjacency mapping between railway crossings and intersection controllers.
typedef struct {
    controller_id_t rlx_id;
    controller_id_t adjacent_lx[2];
} rlx_adjacency_t;

static const rlx_adjacency_t ADJACENCY[] = {
    { CTRL_RL1, { CTRL_L1, CTRL_L2 } },
    { CTRL_RL2, { CTRL_L3, CTRL_L4 } },
    { CTRL_RL3, { CTRL_L5, CTRL_L6 } },
};

// Finds the adjacency mapping for the given railway controller ID, returning NULL if not found.
static const rlx_adjacency_t *find_adjacency(controller_id_t self_id)
{
    size_t i;
    for (i = 0; i < sizeof(ADJACENCY) / sizeof(ADJACENCY[0]); i++) {
        if (ADJACENCY[i].rlx_id == self_id) {
            return &ADJACENCY[i];
        }
    }
    // Defensively handles invalid self_ids.
    return NULL; 
}

// Sends a crossing status message to a specific target controller, logging any enqueue failures.
static void send_crossing_status(controller_id_t self_id, controller_id_t target_id, crossing_state_t state, ipc_client_queue_t *client_queue)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb = MSG_CROSSING_STATUS;
    req.sender_id = (uint32_t)self_id;
    req.target_id = (uint32_t)target_id;
    req.timestamp_ms = 0;
    req.payload.crossing_status.state = (uint32_t)state;

    // Relays crossing status to adjacent intersections to suppress toward-crossing movements.
    if (ipc_client_post(client_queue, target_id, &req, on_reply_log_failure, NULL) != 0) {
        fprintf(stderr, "RLx: CROSSING_STATUS to %d dropped - outgoing queue full or stopping\n", (int)target_id);
    }
}

// Broadcasts crossing status to adjacent intersections and C1 only when the status changes, avoiding redundant messages.
void rlx_comm_broadcast_crossing_status_if_changed(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue)
{
    // Static state tracker ensuring broadcasts only occur upon crossing status changes.
    static int last_broadcast_state = -1;
    const rlx_adjacency_t *adj;
    crossing_state_t current;

    current = rlx_fsm_get_crossing_state(fsm);
    if ((int)current == last_broadcast_state) {
        return;
    }
    last_broadcast_state = (int)current;

    adj = find_adjacency(self_id);
    if (adj == NULL) {
        return;
    }

    // Sends the updated crossing status to both adjacent intersections and C1.
    send_crossing_status(self_id, adj->adjacent_lx[0], current, client_queue);
    send_crossing_status(self_id, adj->adjacent_lx[1], current, client_queue);
    send_crossing_status(self_id, CTRL_C1, current, client_queue);
}