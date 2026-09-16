#ifndef RLX_GATE_H
#define RLX_GATE_H

#include <stdint.h>

// Tracks a simplified aggregate gate motion state for the entire crossing instead of independent gates.

// Conservative 3-second gate travel time allowance.
#define RLX_GATE_MOTION_MS 3000u

// Initializes the gate module once at startup.
void rlx_gate_init(void);

// Conditionally advances gate motion every FSM tick.
void rlx_gate_on_tick(void);

// Issues a close or reclose command to the gate.
void rlx_gate_command_close(void);

// Issues an open command to the gate.
void rlx_gate_command_open(void);

// Polled to check if the gate is confirmed closed.
uint8_t rlx_gate_poll_closed(void);

// Polled to check if the gate is confirmed open.
uint8_t rlx_gate_poll_open(void);

// Simulates a physical failure where the next gate motion never confirms.
void rlx_gate_arm_demo_fault(void);

// Simulates a physical gate repair by forcing the gate to a confirmed idle open state.
void rlx_gate_force_confirmed_open(void);

#endif /* RLX_GATE_H */