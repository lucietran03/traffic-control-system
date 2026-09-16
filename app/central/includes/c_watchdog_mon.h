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

#ifndef C_WATCHDOG_MON_H
#define C_WATCHDOG_MON_H

#include "c_mode_eng.h"

// Ticks the network watchdog to track missed heartbeats and flag unavailable controllers.
int c_watchdog_mon_tick(c_mode_eng_t *eng, controller_id_t *out_newly_unavailable);

#endif /* C_WATCHDOG_MON_H */