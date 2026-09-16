/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Team QNX
    Member: Tran Dong Nghi - s3914633
			Le Hung - s4061665
			Hoang Minh Thang - s3999925
    Assessment: 2 - Project Implementation 
    Due date: 18/09/2026
*/
#include <stdio.h>
#include "rlx_signal.h"

// Translates internal fault bitmasks into human-readable signal representations.
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

// Displays the flashers ON state for a specific train approach direction.
void rlx_signal_show_flashers_on(uint32_t direction)
{
    printf("RLx: flashers ON (train approaching, direction %u)\n", direction);
}

// Displays the flashers OFF state when gates are confirmed open.
void rlx_signal_show_flashers_off(void)
{
    printf("RLx: flashers OFF (gates confirmed open)\n");
}

// Displays the PROCEED state for a specific train approach direction when gates are confirmed closed.
void rlx_signal_show_train_proceed(uint32_t direction)
{
    printf("RLx: train signal PROCEED for direction %u (gates confirmed closed)\n", direction);
}

// Displays the STOP state for all train signals when the crossing is occupied or faulted.
void rlx_signal_show_train_stop(void)
{
    printf("RLx: all train signals -> STOP (crossing reopening)\n");
}

// Displays the REOPENING state when the crossing is transitioning from occupied to open.
void rlx_signal_show_reclosing(void)
{
    printf("RLx: reclosing - aborting gate-open motion, flashers remain active\n");
}

// Displays the FAULT state with the specific fault bit that has been latched, indicating that all train signals are held at STOP and gates are commanded down.
void rlx_signal_show_fault(fault_flags_t fault_bit)
{
    printf("RLx: FAULT latched (%s, bit 0x%x) - holding STOP on all train signals, commanding gates DOWN\n",
           fault_bit_name(fault_bit), (unsigned)fault_bit);
}