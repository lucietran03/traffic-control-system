#ifndef LX_COMM_H
#define LX_COMM_H

#include "lx_fsm.h"
#include "qnet_utils.h"

/* Builds a MSG_HEARTBEAT (PA-07) with an lx_fsm_fill_status() snapshot as
 * its payload and posts it to CTRL_C1 via the client queue. Called once
 * per IPC_PULSE_HEARTBEAT_TICK from lx_main.c's on_pulse() - non-blocking
 * (ipc_client_post() only enqueues), safe to call from the server thread. */
void lx_comm_send_heartbeat(controller_id_t self_id, lx_fsm_t *fsm, ipc_client_queue_t *client_queue);

#endif /* LX_COMM_H */
