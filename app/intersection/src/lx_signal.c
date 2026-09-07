#include <stdio.h>

#include "sys_types.h"
#include "lx_signal.h"

/*
 * Lx signal-head actuation - implementation.
 *
 * PoC stand-in for the actual GPIO/relay drive: every function here is a
 * pure printf wrapper with no state and no locking of its own, so it is
 * always safe to call while a caller (lx_fsm.c) already holds fsm->lock -
 * these functions never block and never take a lock themselves (see
 * lx_fsm.h's threading contract).
 */

/* --- internal helpers --------------------------------------------------- */

static const char *lx_signal_phase_name(signal_phase_t phase)
{
    switch (phase) {
    case PHASE_ARTERIAL_GREEN:
        return "ARTERIAL GREEN";
    case PHASE_ARTERIAL_YELLOW:
        return "ARTERIAL YELLOW";
    case PHASE_ALL_RED_A_TO_B:
        return "ALL RED (A to B)";
    case PHASE_CONNECTOR_GREEN:
        return "CONNECTOR GREEN";
    case PHASE_CONNECTOR_YELLOW:
        return "CONNECTOR YELLOW";
    case PHASE_ALL_RED_B_TO_A:
        return "ALL RED (B to A)";
    default:
        return "UNKNOWN";
    }
}

/* --- public API ---------------------------------------------------------- */

void lx_signal_show_phase(controller_id_t id, signal_phase_t phase)
{
    printf("Lx %d: SIGNAL -> %s\n", (int)id, lx_signal_phase_name(phase));
}

void lx_signal_apply_fault_safe(controller_id_t id)
{
    printf("Lx %d: FAULT_SAFE - holding safe outputs (all-red/dark)\n", (int)id);
}

void lx_signal_show_override_clearance(controller_id_t id)
{
    printf("Lx %d: override cleared/expired - running safe clearance sequence\n", (int)id);
}

void lx_signal_show_walk(controller_id_t id, uint8_t side)
{
    printf("Lx %d: PED SIGNAL side %u -> WALK\n", (int)id, (unsigned)side);
}

void lx_signal_show_flashing_dont_walk(controller_id_t id, uint8_t side)
{
    printf("Lx %d: PED SIGNAL side %u -> FLASHING_DONT_WALK\n", (int)id, (unsigned)side);
}

void lx_signal_show_dont_walk(controller_id_t id, uint8_t side)
{
    printf("Lx %d: PED SIGNAL side %u -> DONT_WALK\n", (int)id, (unsigned)side);
}
