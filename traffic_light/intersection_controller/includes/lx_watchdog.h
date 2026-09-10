#ifndef LX_WATCHDOG_H
#define LX_WATCHDOG_H

#include <stdint.h>

#include "lx_fsm.h"

/* Small struct the watchdog thread actually needs, decoupled from
 * lx_main.c's full intersection_context_t (which isn't declared in any
 * header) - lx_main.c populates one of these and passes its address to
 * pthread_create() instead of passing &ctx directly. */
typedef struct {
    lx_fsm_t          *fsm;
    volatile uint32_t *phase_tick_counter;
} lx_watchdog_args_t;

/* PA-10 dead-man's-switch thread. arg must point to an lx_watchdog_args_t
 * (needs both ->phase_tick_counter and ->fsm) - this is the one thread
 * that needs more than just the fsm, unlike lx_sensor_reader_thread.
 * Wakes up periodically, and if phase_tick_counter hasn't advanced since
 * the last check (the phase timer should fire every 100 ms, so several
 * seconds of silence is unambiguous), reports a trip via
 * lx_fsm_report_watchdog_trip() and keeps watching (does not exit - a
 * hung main loop recovering later, e.g. after FAULT_SAFE is cleared,
 * should resume being monitored normally). */
void *lx_watchdog_thread(void *arg);

#endif /* LX_WATCHDOG_H */
