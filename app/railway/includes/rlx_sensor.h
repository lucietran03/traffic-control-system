#ifndef RLX_SENSOR_H
#define RLX_SENSOR_H

#include "rlx_fsm.h"

// Dedicated blocking thread that continually reads simulated keyboard sensor inputs.
void *rlx_sensor_reader_thread(void *arg);

#endif /* RLX_SENSOR_H */