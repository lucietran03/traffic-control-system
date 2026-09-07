#include <stdio.h>
#include "rlx_signal.h"

void rlx_signal_show_flashers_on(uint32_t direction)
{
    printf("RLx: flashers ON (train approaching, direction %u)\n", direction);
}

void rlx_signal_show_flashers_off(void)
{
    printf("RLx: flashers OFF (gates confirmed open)\n");
}

void rlx_signal_show_train_proceed(uint32_t direction)
{
    printf("RLx: train signal PROCEED for direction %u (gates confirmed closed)\n", direction);
}

void rlx_signal_show_train_stop(void)
{
    printf("RLx: all train signals -> STOP (crossing reopening)\n");
}

void rlx_signal_show_reclosing(void)
{
    printf("RLx: reclosing - aborting gate-open motion, flashers remain active\n");
}

void rlx_signal_show_fault(fault_flags_t fault_bit)
{
    printf("RLx: FAULT latched (fault bit 0x%x) - holding STOP on all train signals, commanding gates DOWN\n",
           (unsigned)fault_bit);
}
