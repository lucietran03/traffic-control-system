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

#include "sys_types.h"
#include "lx_signal.h"

// Non-blocking console stubs mocking the signal-head and crosswalk hardware outputs.

// --- internal helpers --------------------------------------------------- //

static const char *lx_signal_phase_name(signal_phase_t phase)
{
    switch (phase) {
    case PHASE_ARTERIAL_GREEN: // Arterial approach green
        return "ARTERIAL GREEN";
    case PHASE_ARTERIAL_YELLOW: // Arterial approach yellow
        return "ARTERIAL YELLOW";
    case PHASE_ALL_RED_A_TO_B: // All-red clearance from arterial to connector
        return "ALL RED (A to B)";
    case PHASE_CONNECTOR_GREEN: // Connector approach green
        return "CONNECTOR GREEN";
    case PHASE_CONNECTOR_YELLOW: // Connector approach yellow
        return "CONNECTOR YELLOW";
    case PHASE_ALL_RED_B_TO_A: // All-red clearance from connector to arterial
        return "ALL RED (B to A)";
    default:
        return "UNKNOWN";
    }
}

// --- public API ---------------------------------------------------------- //

// Displays the current signal phase for the specified controller ID.
void lx_signal_show_phase(controller_id_t id, signal_phase_t phase)
{
    printf("Lx %d: signal phase now %s\n", (int)id, lx_signal_phase_name(phase));
}

// Displays the current pedestrian signal state for the specified controller ID and side.
void lx_signal_apply_fault_safe(controller_id_t id)
{
    printf("Lx %d: entering FAULT_SAFE mode - holding safe outputs (all-red/dark)\n", (int)id);
}

// Displays the current override clearance state for the specified controller ID.
void lx_signal_show_override_clearance(controller_id_t id)
{
    printf("Lx %d: override cleared/expired - running safe clearance sequence\n", (int)id);
}

// Displays the current override active state for the specified controller ID.
void lx_signal_show_walk(controller_id_t id, uint8_t side)
{
    printf("Lx %d: PED SIGNAL side %u -> WALK\n", (int)id, (unsigned)side);
}

// Displays the current pedestrian signal state for the specified controller ID and side.
void lx_signal_show_flashing_dont_walk(controller_id_t id, uint8_t side)
{
    printf("Lx %d: PED SIGNAL side %u -> FLASHING_DONT_WALK\n", (int)id, (unsigned)side);
}

// Displays the current pedestrian signal state for the specified controller ID and side.
void lx_signal_show_dont_walk(controller_id_t id, uint8_t side)
{
    printf("Lx %d: PED SIGNAL side %u -> DONT_WALK\n", (int)id, (unsigned)side);
}