#ifndef RLX_SIGNAL_H
#define RLX_SIGNAL_H
#include <stdint.h>
#include "sys_types.h"

// Non-blocking, thread-safe console printf wrappers simulating train signal and flasher actuations.

void rlx_signal_show_flashers_on(uint32_t direction);
void rlx_signal_show_flashers_off(void);
void rlx_signal_show_train_proceed(uint32_t direction);
void rlx_signal_show_train_stop(void);
void rlx_signal_show_reclosing(void);
void rlx_signal_show_fault(fault_flags_t fault_bit);

#endif /* RLX_SIGNAL_H */