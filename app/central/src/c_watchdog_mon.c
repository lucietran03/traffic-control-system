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

#include "c_watchdog_mon.h"

// Increments missed heartbeats and marks controllers unavailable after three consecutive misses.
int c_watchdog_mon_tick(c_mode_eng_t *eng, controller_id_t *out_newly_unavailable)
{
    int i;
    int n = 0;

    for (i = 0; i < 9; i++) {
        eng->controllers[i].missed_heartbeat_ticks++;

        if (eng->controllers[i].missed_heartbeat_ticks == 3 &&
            eng->controllers[i].marked_unavailable == 0) {
            eng->controllers[i].marked_unavailable = 1;
            out_newly_unavailable[n++] = eng->controllers[i].id;
        }
    }

    return n;
}