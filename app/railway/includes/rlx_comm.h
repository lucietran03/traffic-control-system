#ifndef RLX_COMM_H
#define RLX_COMM_H

#include "rlx_fsm.h"
#include "qnet_utils.h"

// Builds and posts a MSG_HEARTBEAT with an FSM status snapshot to the Central controller.
void rlx_comm_send_heartbeat(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue);

// Builds and independently posts a MSG_FAULT_REPORT to Central based on current FSM fault flags.
void rlx_comm_send_fault_report(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue);

// Broadcasts a MSG_CROSSING_STATUS to adjacent intersections and Central if the state changed.
void rlx_comm_broadcast_crossing_status_if_changed(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue);

#endif /* RLX_COMM_H */