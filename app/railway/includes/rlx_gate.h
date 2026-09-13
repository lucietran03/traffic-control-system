#ifndef RLX_GATE_H
#define RLX_GATE_H

#include <stdint.h>

/*
 * Accepted Core simplification, not currently documented as a limitation
 * elsewhere: RC-01 specifies two independent boom gates and two gate-
 * position sensors per crossing (so a real asymmetric failure - one gate
 * closes, the other doesn't - is representable). This module instead
 * tracks a single aggregate gate_motion_t/confirmed-closed/confirmed-open
 * state for the whole crossing (see rlx_gate.c) - RC-06's "sensor-
 * confirmed closed before PROCEED" invariant is still enforced, just
 * against one combined position, not two independent ones. Splitting this
 * into a real two-gate model would need corresponding two-gate fields in
 * crossing_status_payload_t (ipc_msg.h) and is a larger change than a
 * pre-demo fix; flagged here rather than silently left unstated.
 */

/* Judgment call / conservative default: no doc specifies exact gate
 * travel time. 3000 ms is physically plausible and well inside RC-03's
 * 10s gate-closing allowance (a budget for the whole closing step, of
 * which travel is only part), short enough not to stall a demo. */
#define RLX_GATE_MOTION_MS 3000u

void rlx_gate_init(void);            /* call once from rlx_main.c's main() */
void rlx_gate_on_tick(void);         /* call exactly once per rlx_fsm_on_tick() invocation, unconditionally */
void rlx_gate_command_close(void);   /* reused for both CLOSING and RECLOSING */
void rlx_gate_command_open(void);
uint8_t rlx_gate_poll_closed(void);  /* read-only, safe to call any number of times per tick */
uint8_t rlx_gate_poll_open(void);
void rlx_gate_arm_demo_fault(void);  /* one-shot: the NEXT gate motion never confirms (RC-06 demo path) */

/*
 * Test-plan finding, confirmed independently by 3 test-design passes:
 * nothing in rlx_fsm.c ever calls rlx_gate_command_open() while
 * state==RLX_FAULT (every fault-entry path only ever closes the gate), so
 * gates_confirmed_open() can never become true on its own once faulted -
 * meaning MSG_REQUEST_FAULT_CLEAR's RESULT_ACK branch was unreachable by
 * any demo key. This simulates a technician physically repairing/
 * confirming the gate mechanism is open again - the demo-side counterpart
 * to rlx_gate_arm_demo_fault() (which simulates the opposite: a physical
 * failure). Immediately sets the gate to a fully-open, idle, confirmed
 * state, independent of any in-progress motion. */
void rlx_gate_force_confirmed_open(void);

#endif /* RLX_GATE_H */
