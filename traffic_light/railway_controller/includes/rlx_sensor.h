#ifndef RLX_SENSOR_H
#define RLX_SENSOR_H

#include "rlx_fsm.h"

/* Reads simulated sensor events from stdin in a loop - see
 * app/shared/README.md "Threading pattern" and lx_sensor.h's identical
 * rationale: reading stdin blocks, so this runs as its own dedicated
 * thread, never sharing the server or client thread. arg must point to
 * the rlx_fsm_t this reader updates. */
void *rlx_sensor_reader_thread(void *arg);

#endif /* RLX_SENSOR_H */
