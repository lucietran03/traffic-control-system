/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Team QNX
    Member: Tran Dong Nghi - s3914633
			Le Hung - s4061665
			Hoang Minh Thang - s3999925
    Assessment: 2 - Project Implementation 
    Due date: 18/09/2026
*/

#ifndef LX_SENSOR_H
#define LX_SENSOR_H

#include "lx_fsm.h"

// A dedicated blocking thread that continuously reads simulated keyboard inputs and updates the local FSM sensors.
void *lx_sensor_reader_thread(void *arg);

#endif /* LX_SENSOR_H */