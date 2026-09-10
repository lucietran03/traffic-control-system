#include <stdio.h>
#include <string.h>

#include "lx_comm.h"

static void on_heartbeat_reply(controller_id_t target_id, const ipc_request_t *original_req,
                                const ipc_reply_t *reply, int send_ok, void *ctx)
{
    (void)original_req;
    (void)reply;
    (void)ctx;
    if (!send_ok) {
        fprintf(stderr, "Lx: HEARTBEAT to %d failed to send\n", (int)target_id);
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

    lx_fsm_fill_status(fsm, &req.payload.heartbeat.summary);
    /* lx_fsm_fill_status() deliberately leaves link_state untouched (see
     * its own doc comment in lx_fsm.h) - no connectivity-tracking logic
     * exists yet to report anything else honestly, so this is a
     * placeholder, not a real observation. */
    req.payload.heartbeat.summary.link_state = (uint32_t)LINK_CENTRAL_CONNECTED;

    /* Verifier-audit fix: ipc_client_post() returns -1 (queue full or
     * being torn down) WITHOUT ever invoking on_heartbeat_reply - that
     * callback only fires for a later name_open()/MsgSend() failure, so
     * a rejected enqueue was previously silent. Log it here instead, at
     * the only point that can observe it. */
    if (ipc_client_post(client_queue, CTRL_C1, &req, on_heartbeat_reply, NULL) != 0) {
        fprintf(stderr, "Lx: HEARTBEAT to C1 dropped - outgoing queue full or stopping\n");
    }
}
