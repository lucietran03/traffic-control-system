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

#include <stdio.h>
#include <unistd.h>

#include "lx_watchdog.h"

// Defines a 2-second check interval for detecting stalled main loops and triggering fault states.
#define LX_WATCHDOG_CHECK_INTERVAL_S 2u

// Thread function that monitors the phase-timer tick counter and reports a watchdog trip to the FSM if no activity is detected within the defined interval.
void *lx_watchdog_thread(void *arg)
{
    lx_watchdog_args_t *args = (lx_watchdog_args_t *)arg;
    uint32_t             last_seen = 0;
    uint32_t             current;

    for (;;) {
        sleep(LX_WATCHDOG_CHECK_INTERVAL_S);

        current = *args->phase_tick_counter;
        if (current == last_seen) {
            fprintf(stderr, "Lx: WATCHDOG - no phase-timer activity for %u s, reporting fault (PA-10)\n",
                    (unsigned)LX_WATCHDOG_CHECK_INTERVAL_S);
            lx_fsm_report_watchdog_trip(args->fsm);
        }
        last_seen = current;
    }

    return NULL; 
}