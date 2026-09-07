#ifndef LX_SIGNAL_H
#define LX_SIGNAL_H
#include "sys_types.h"

void lx_signal_show_phase(controller_id_t id, signal_phase_t phase);
void lx_signal_apply_fault_safe(controller_id_t id);
void lx_signal_show_override_clearance(controller_id_t id);

/* SC-02/TL-05/UC-02: one pedestrian crossing side's signal head, side in
 * [0,3] (NU-03). Printf-based stand-ins, same PoC pattern as every other
 * function in this file - never block, never take a lock, safe to call
 * with fsm->lock already held. */
void lx_signal_show_walk(controller_id_t id, uint8_t side);
void lx_signal_show_flashing_dont_walk(controller_id_t id, uint8_t side);
void lx_signal_show_dont_walk(controller_id_t id, uint8_t side);

#endif /* LX_SIGNAL_H */
