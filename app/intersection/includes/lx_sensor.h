#ifndef LX_SENSOR_H
#define LX_SENSOR_H

#include "lx_fsm.h"

// A dedicated blocking thread that continuously reads simulated keyboard inputs and updates the local FSM sensors.
void *lx_sensor_reader_thread(void *arg);

#endif /* LX_SENSOR_H */