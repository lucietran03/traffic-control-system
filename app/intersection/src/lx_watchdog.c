#include <stdio.h>
#include <unistd.h>

#include "lx_watchdog.h"

/*
 * Lx local fail-safe watchdog - implementation.
 *
 * PA-10 dead-man's switch: does not implement any safe-output logic of
 * its own - it only detects that the server thread's phase-timer pulse
 * (IPC_PULSE_PHASE_TIMER, armed in lx_main.c at a 100 ms period) has
 * stopped advancing lx_watchdog_args_t::phase_tick_counter, and reports
 * that via lx_fsm_report_watchdog_trip(). The existing, already-audited
 * lx_fsm_check_fault_locked() machinery takes it from there (SC-03A).
 */

/* Conservative default: the phase timer ticks every 100 ms, so 2 s of
 * total silence (20 missed ticks) is unambiguous, not a false positive
 * from ordinary scheduling jitter. */
#define LX_WATCHDOG_CHECK_INTERVAL_S 2u

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

    return NULL; /* unreachable - matches the existing server/client thread
                  * pattern of an infinite loop. */
}
