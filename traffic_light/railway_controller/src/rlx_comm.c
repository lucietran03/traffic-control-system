#include <stdio.h>
#include <string.h>

#include "rlx_comm.h"

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

void rlx_comm_send_heartbeat(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb = MSG_HEARTBEAT;
    req.sender_id = (uint32_t)self_id;
    req.target_id = (uint32_t)CTRL_C1;
    /* Known gap: no monotonic-clock helper exists anywhere in this
     * codebase yet - 0 is the established placeholder convention (see
     * lx_comm.c) until a clock API is added. */
    req.timestamp_ms = 0;

    rlx_fsm_fill_status(fsm, &req.payload.heartbeat.summary);
    /* rlx_fsm_fill_status() only fills role/crossing_state/faults for
     * ROLE_RAILWAY - mode/signal_phase/supervisory_state are not
     * meaningful here and stay 0. link_state has the same placeholder gap
     * as lx_comm.c's heartbeat: no connectivity-tracking logic exists yet
     * to report anything else honestly. */
    req.payload.heartbeat.summary.link_state = (uint32_t)LINK_CENTRAL_CONNECTED;

    /* Verifier-audit fix: ipc_client_post() returns -1 (queue full or
     * stopping) WITHOUT ever invoking on_reply_log_failure - log the
     * drop here, the only point that can observe it. */
    if (ipc_client_post(client_queue, CTRL_C1, &req, on_reply_log_failure, NULL) != 0) {
        fprintf(stderr, "RLx: HEARTBEAT to C1 dropped - outgoing queue full or stopping\n");
    }
}

void rlx_comm_send_fault_report(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue)
{
    ipc_request_t req;
    status_report_payload_t tmp_status;

    memset(&req, 0, sizeof(req));
    req.verb = MSG_FAULT_REPORT;
    req.sender_id = (uint32_t)self_id;
    req.target_id = (uint32_t)CTRL_C1;
    req.timestamp_ms = 0;

    rlx_fsm_fill_status(fsm, &tmp_status); /* to read the current fault_flags_t out */
    req.payload.fault_report.fault_code = (uint32_t)tmp_status.faults;
    req.payload.fault_report.severity = 1; /* fixed placeholder - no severity-classification scheme is designed anywhere */
    strncpy(req.payload.fault_report.detail, "RLx fault - see fault_code bitmask",
            sizeof(req.payload.fault_report.detail) - 1);
    req.payload.fault_report.detail[sizeof(req.payload.fault_report.detail) - 1] = '\0';

    /* RC-10: this send is independent of/in-parallel with the FSM's own
     * local safe-state response, which has already been applied by the
     * time this is called - a dropped enqueue here must still be logged
     * (safety-relevant), not silently lost. */
    if (ipc_client_post(client_queue, CTRL_C1, &req, on_reply_log_failure, NULL) != 0) {
        fprintf(stderr, "RLx: FAULT_REPORT to C1 dropped - outgoing queue full or stopping\n");
    }
}

/* RC-07: RL1-RL3 each independently manage RC1-RC3; adjacency to Lx per
 * SYSTEM_DIAGRAMS.md Diagram 4 / lecture_clarification.md topology
 * (RC1 between I1/I2, RC2 between I3/I4, RC3 between I5/I6). */
typedef struct {
    controller_id_t rlx_id;
    controller_id_t adjacent_lx[2];
} rlx_adjacency_t;

static const rlx_adjacency_t ADJACENCY[] = {
    { CTRL_RL1, { CTRL_L1, CTRL_L2 } },
    { CTRL_RL2, { CTRL_L3, CTRL_L4 } },
    { CTRL_RL3, { CTRL_L5, CTRL_L6 } },
};

static const rlx_adjacency_t *find_adjacency(controller_id_t self_id)
{
    size_t i;
    for (i = 0; i < sizeof(ADJACENCY) / sizeof(ADJACENCY[0]); i++) {
        if (ADJACENCY[i].rlx_id == self_id) {
            return &ADJACENCY[i];
        }
    }
    return NULL; /* defensive: should never happen given parse_self_id()'s 1-3 range in rlx_main.c */
}

static void send_crossing_status(controller_id_t self_id, controller_id_t target_id,
                                  crossing_state_t state, ipc_client_queue_t *client_queue)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb = MSG_CROSSING_STATUS;
    req.sender_id = (uint32_t)self_id;
    req.target_id = (uint32_t)target_id;
    req.timestamp_ms = 0;
    req.payload.crossing_status.state = (uint32_t)state;

    /* RC-02: this is the one-way status feed adjacent Lx controllers rely
     * on to suppress a toward-crossing movement (CC-02) - a silently
     * dropped enqueue here is safety-relevant, must be logged. */
    if (ipc_client_post(client_queue, target_id, &req, on_reply_log_failure, NULL) != 0) {
        fprintf(stderr, "RLx: CROSSING_STATUS to %d dropped - outgoing queue full or stopping\n", (int)target_id);
    }
}

void rlx_comm_broadcast_crossing_status_if_changed(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue)
{
    /* Sentinel outside crossing_state_t's valid range so the very first
     * call always sends (there is no previous state to compare against
     * yet). One process = one fixed self_id for its whole lifetime (see
     * rlx_main.c's parse_self_id()), so a single static is sufficient -
     * no need to index by self_id. */
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

    send_crossing_status(self_id, adj->adjacent_lx[0], current, client_queue);
    send_crossing_status(self_id, adj->adjacent_lx[1], current, client_queue);
    send_crossing_status(self_id, CTRL_C1, current, client_queue);
}
