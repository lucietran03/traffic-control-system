#ifndef RLX_SIGNAL_H
#define RLX_SIGNAL_H
#include <stdint.h>
#include "sys_types.h"

/*
 * Pure printf wrappers for train-signal/flasher/fault output (STATE_CHARTS.md
 * SC-04A/SC-04B; RC-03, RC-04, RC-06, RC-10). No locking, no state - safe to
 * call while rlx_fsm.c's fsm->lock is already held, which is how every
 * caller in rlx_fsm.c uses these. No real train-signal-head hardware exists
 * for this PoC, so these calls are the wire-visible-in-the-console
 * equivalent of an actuator command.
 */

void rlx_signal_show_flashers_on(uint32_t direction);
void rlx_signal_show_flashers_off(void);
void rlx_signal_show_train_proceed(uint32_t direction);
void rlx_signal_show_train_stop(void);
void rlx_signal_show_reclosing(void);
void rlx_signal_show_fault(fault_flags_t fault_bit);

#endif /* RLX_SIGNAL_H */
