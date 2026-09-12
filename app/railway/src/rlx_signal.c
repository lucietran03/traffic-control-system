#include <stdio.h>
#include "rlx_signal.h"

static const char *fault_bit_name(fault_flags_t bit)
{
    switch (bit) {
    case FAULT_GATE_CONFIRM_MISSING:
        return "GATE_CONFIRM_MISSING";
    case FAULT_TRAIN_SENSOR_STUCK:
        return "TRAIN_SENSOR_STUCK";
    case FAULT_PED_BUTTON_STUCK:
        return "PED_BUTTON_STUCK";
    case FAULT_VEHICLE_SENSOR_STUCK:
        return "VEHICLE_SENSOR_STUCK";
    case FAULT_WATCHDOG_TRIP:
        return "WATCHDOG_TRIP";
    default:
        return "UNKNOWN_FAULT";
    }
}

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
    printf("RLx: FAULT latched (%s, bit 0x%x) - holding STOP on all train signals, commanding gates DOWN\n",
           fault_bit_name(fault_bit), (unsigned)fault_bit);
}
