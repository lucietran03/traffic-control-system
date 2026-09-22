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

#include "rlx_watchdog.h"

// Isolated watchdog thread monitoring the tick counter to trigger faults if the main loop stalls.

// Conservative 3-second diagnostic delay safely absorbing ordinary scheduling jitter.
#define RLX_WATCHDOG_CHECK_INTERVAL_S 3u   

// Dedicated thread function that monitors the tick counter and reports a watchdog trip if no activity is detected within the defined interval.
void *rlx_watchdog_thread(void *arg)
{
    rlx_watchdog_args_t *args = (rlx_watchdog_args_t *)arg;
    uint32_t last_seen = 0;
    uint32_t current;

    for (;;) {
        // #1 Sleep for the check interval; this thread runs independently of the server thread.
        sleep(RLX_WATCHDOG_CHECK_INTERVAL_S);
        // #2 Counter unchanged since last check means the server thread is stalled.
        current = *args->tick_counter;
        if (current == last_seen) {
            fprintf(stderr, "RLx: WATCHDOG - no tick activity for %u s, reporting fault (PA-10)\n",
                    (unsigned)RLX_WATCHDOG_CHECK_INTERVAL_S);
            rlx_fsm_report_watchdog_trip(args->fsm); // Directly triggers a fault via watchdog if the FSM loop stalls.
        }
        // #3 Record this tick count as the new baseline for the next interval.
        last_seen = current;
    }
    return NULL;
}