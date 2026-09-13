#include <stdio.h>
#include <string.h>

#include "lx_comm.h"

/*
 * PA-07: ctx is the sending lx_fsm_t* (passed below), so this callback -
 * which runs on the CLIENT thread, never the server thread - can report
 * this heartbeat's real outcome to lx_fsm_on_heartbeat_result() instead of
 * only logging an outright transport failure. A real ACK is send_ok==1
 * AND reply->result==RESULT_ACK (c_main.c's on_request() always replies
 * RESULT_ACK to MSG_HEARTBEAT, so anything else - NACK/ERROR/no reply -
 * means this heartbeat did not land as a genuine proof-of-life). */
static void on_heartbeat_reply(controller_id_t target_id, const ipc_request_t *original_req,
                                const ipc_reply_t *reply, int send_ok, void *ctx)
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

void lx_comm_send_heartbeat(controller_id_t self_id, lx_fsm_t *fsm, ipc_client_queue_t *client_queue)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb = MSG_HEARTBEAT;
    req.sender_id = (uint32_t)self_id;
    req.target_id = (uint32_t)CTRL_C1;
    /* Known gap: no monotonic-clock helper exists anywhere in this
     * codebase yet (grep confirms). PA-07/PA-08 conceptually want a real
     * timestamp; 0 is the established placeholder convention until a
     * clock API is added. */
    req.timestamp_ms = 0;

    /* lx_fsm_fill_status() now copies the real, observed link_state (see
     * lx_fsm_on_heartbeat_result()) - no hardcoded override here anymore. */
    lx_fsm_fill_status(fsm, &req.payload.heartbeat.summary);

    /* Verifier-audit fix: ipc_client_post() returns -1 (queue full or
     * being torn down) WITHOUT ever invoking on_heartbeat_reply - that
     * callback only fires for a later name_open()/MsgSend() failure, so
     * a rejected enqueue was previously silent. Log it here instead, at
     * the only point that can observe it - and count it as a miss for
     * PA-07 the same as an unacknowledged send, since a heartbeat that
     * never even reached the outgoing queue is equally strong evidence
     * that C1 didn't hear from this controller. */
    if (ipc_client_post(client_queue, CTRL_C1, &req, on_heartbeat_reply, fsm) != 0) {
        fprintf(stderr, "Lx: HEARTBEAT to C1 dropped - outgoing queue full or stopping\n");
        (void)lx_fsm_on_heartbeat_result(fsm, 0);
    }
}
