#ifndef LX_COMM_H
#define LX_COMM_H

#include "lx_fsm.h"
#include "qnet_utils.h"

// Enqueues a non-blocking heartbeat message containing the current FSM status snapshot to the Central controller.
void lx_comm_send_heartbeat(controller_id_t self_id, lx_fsm_t *fsm, ipc_client_queue_t *client_queue);

#endif /* LX_COMM_H */