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

#ifndef RLX_SENSOR_H
#define RLX_SENSOR_H

#include "rlx_fsm.h"

// Dedicated blocking thread that continually reads simulated keyboard sensor inputs.
void *rlx_sensor_reader_thread(void *arg);

#endif /* RLX_SENSOR_H */