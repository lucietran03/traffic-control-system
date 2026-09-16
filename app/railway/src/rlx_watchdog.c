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
        sleep(RLX_WATCHDOG_CHECK_INTERVAL_S);
        current = *args->tick_counter;
        if (current == last_seen) {
            fprintf(stderr, "RLx: WATCHDOG - no tick activity for %u s, reporting fault (PA-10)\n",
                    (unsigned)RLX_WATCHDOG_CHECK_INTERVAL_S);
            rlx_fsm_report_watchdog_trip(args->fsm); // Directly triggers a fault via watchdog if the FSM loop stalls.
        }
        last_seen = current;
    }
    return NULL;
}