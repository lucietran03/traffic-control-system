#ifndef LX_WATCHDOG_H
#define LX_WATCHDOG_H

#include <stdint.h>
#include "lx_fsm.h"

// Decoupled thread arguments strictly needed by the watchdog to monitor the FSM phase timer.
typedef struct {
    lx_fsm_t          *fsm;
    volatile uint32_t *phase_tick_counter;
} lx_watchdog_args_t;

// Periodically checks the phase timer tick counter and triggers a fault-safe state if the main loop stalls.
void *lx_watchdog_thread(void *arg);

#endif /* LX_WATCHDOG_H */