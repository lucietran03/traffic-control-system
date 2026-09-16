#include <stdio.h>
#include <string.h>

#include "lx_comm.h"

// Client-thread callback to verify heartbeat ACKs and report connection state transitions back to the FSM.
static void on_heartbeat_reply(controller_id_t target_id, const ipc_request_t *original_req, const ipc_reply_t *reply, int send_ok, void *ctx)
{
    lx_fsm_t *fsm = (lx_fsm_t *)ctx;
    int acked = send_ok && reply != NULL && (msg_result_t)reply->result == RESULT_ACK;
    int transition;

    (void)original_req;

    if (!send_ok) {
        fprintf(stderr, "Lx: HEARTBEAT to %d failed to send\n", (int)target_id);
    }

    transition = lx_fsm_on_heartbeat_result(fsm, acked);
    if (transition == 1) {
        fprintf(stderr, "Lx: 3 consecutive HEARTBEATs unacknowledged - entering DEGRADED_LOCAL (PA-07)\n");
    } else if (transition == 2) {
        fprintf(stderr, "Lx: HEARTBEAT acknowledged by C1 - reconnected, resuming CENTRAL_CONNECTED (PA-08)\n");
    }
}

// Sends a HEARTBEAT request to C1 with the current status report, and registers a callback to handle the reply. If the send fails (queue full or stopping), it immediately reports a failed heartbeat to the FSM.
void lx_comm_send_heartbeat(controller_id_t self_id, lx_fsm_t *fsm, ipc_client_queue_t *client_queue)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb = MSG_HEARTBEAT;
    req.sender_id = (uint32_t)self_id;
    req.target_id = (uint32_t)CTRL_C1;
    req.timestamp_ms = 0;

    lx_fsm_fill_status(fsm, &req.payload.heartbeat.summary);

    if (ipc_client_post(client_queue, CTRL_C1, &req, on_heartbeat_reply, fsm) != 0) {
        fprintf(stderr, "Lx: HEARTBEAT to C1 dropped - outgoing queue full or stopping\n");
        (void)lx_fsm_on_heartbeat_result(fsm, 0);
    }
}