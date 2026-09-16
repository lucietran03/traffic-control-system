#ifndef LX_SIGNAL_H
#define LX_SIGNAL_H
#include "sys_types.h"

// Non-blocking, thread-safe stand-ins for actuating physical signal heads and pedestrian signs.
void lx_signal_show_phase(controller_id_t id, signal_phase_t phase);
void lx_signal_apply_fault_safe(controller_id_t id);
void lx_signal_show_override_clearance(controller_id_t id);

void lx_signal_show_walk(controller_id_t id, uint8_t side);
void lx_signal_show_flashing_dont_walk(controller_id_t id, uint8_t side);
void lx_signal_show_dont_walk(controller_id_t id, uint8_t side);

#endif /* LX_SIGNAL_H */