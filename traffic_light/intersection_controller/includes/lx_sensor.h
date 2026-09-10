#ifndef LX_SENSOR_H
#define LX_SENSOR_H

#include "lx_fsm.h"

/* Reads simulated sensor events from stdin (keyboard) in a loop and calls
 * the corresponding lx_fsm_* setter. Runs as its own dedicated thread -
 * see app/shared/README.md "Threading pattern": reading stdin blocks, so
 * it cannot share the server thread (must never block on MsgReceive) or
 * the client thread (dedicated to outgoing MsgSend). arg must point to
 * the lx_fsm_t this reader should update (NOT the whole
 * intersection_context_t - this file has no reason to know about
 * client_queue/self_id beyond what's already in the fsm). Entry point
 * signature matches pthread_create()'s expected void *(*)(void*). */
void *lx_sensor_reader_thread(void *arg);

#endif /* LX_SENSOR_H */
