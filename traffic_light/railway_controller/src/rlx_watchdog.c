#include <stdio.h>
#include <unistd.h>

#include "rlx_watchdog.h"

/*
 * PA-10 dead-man's-switch thread - see rlx_watchdog.h for the design
 * rationale. Runs as its own dedicated thread (alongside the server,
 * client, and sensor threads) so it keeps checking liveness even if the
 * server thread's tick has stalled.
 */

#define RLX_WATCHDOG_CHECK_INTERVAL_S 3u   /* conservative default: the tick fires every 1000 ms, so 3 s of total silence (3 missed ticks) is unambiguous, not ordinary scheduling jitter */

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
            rlx_fsm_report_watchdog_trip(args->fsm);
        }
        last_seen = current;
    }
    return NULL;
}
