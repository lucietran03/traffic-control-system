#ifndef RLX_WATCHDOG_H
#define RLX_WATCHDOG_H

#include <stdint.h>

#include "rlx_fsm.h"

/*
 * PA-10: local fail-safe watchdog. A 4th thread per process (alongside the
 * server/client/sensor threads) that periodically checks whether the main
 * server thread's tick pulse is still firing, and if not, forces a fault
 * into the FSM so the existing FAULT_SAFE/enter_fault machinery already in
 * rlx_fsm.c takes over. This watchdog does NOT implement its own safe-
 * output logic - it only detects the hang and reports it.
 */

/* Small struct the watchdog thread needs, decoupled from rlx_main.c's
 * full railway_context_t (not declared in any header) - rlx_main.c
 * populates one of these (as a main()-scoped local so it outlives the
 * thread) and passes its address to pthread_create(). */
typedef struct {
    rlx_fsm_t         *fsm;
    volatile uint32_t *tick_counter;
} rlx_watchdog_args_t;

/* PA-10 dead-man's-switch thread. Wakes up periodically, and if
 * tick_counter hasn't advanced since the last check (the tick should
 * fire every 1000 ms, so several seconds of silence is unambiguous),
 * reports a trip via rlx_fsm_report_watchdog_trip() and keeps watching
 * (does not exit). */
void *rlx_watchdog_thread(void *arg);

#endif /* RLX_WATCHDOG_H */
