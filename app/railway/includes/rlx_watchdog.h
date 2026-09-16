#ifndef RLX_WATCHDOG_H
#define RLX_WATCHDOG_H

#include <stdint.h>
#include "rlx_fsm.h"

// Watchdog thread arguments providing the required FSM and tick counter references.
typedef struct {
    rlx_fsm_t         *fsm;
    volatile uint32_t *tick_counter;
} rlx_watchdog_args_t;

// Dedicated dead-man's-switch thread that periodically monitors the main loop tick to catch and report stalls.
void *rlx_watchdog_thread(void *arg);

#endif /* RLX_WATCHDOG_H */