#ifndef RLX_COMM_H
#define RLX_COMM_H

#include "rlx_fsm.h"
#include "qnet_utils.h"

/* Builds a MSG_HEARTBEAT (PA-07) with an rlx_fsm_fill_status() snapshot
 * and posts it to CTRL_C1. Called once per IPC_PULSE_HEARTBEAT_TICK. */
void rlx_comm_send_heartbeat(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue);

/* Builds a MSG_FAULT_REPORT (RC-10) from the FSM's current fault flags
 * and posts it to CTRL_C1. Called when rlx_fsm_take_fault_report_pending()
 * returns 1, independently of and in parallel with the local safe-state
 * response the FSM already applies itself (RC-10: never sequentially
 * gated on Central). */
void rlx_comm_send_fault_report(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue);

/* Sends MSG_CROSSING_STATUS to BOTH adjacent Lx controllers and CTRL_C1
 * (three recipients total per app/shared/README.md) whenever the
 * crossing's externally-visible state has changed since the last call.
 * Tracks its own last-broadcast state internally (does not modify
 * rlx_fsm.h/.c) - a no-op when nothing changed. Called once per tick from
 * rlx_main.c's on_pulse(). */
void rlx_comm_broadcast_crossing_status_if_changed(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t *client_queue);

#endif /* RLX_COMM_H */
