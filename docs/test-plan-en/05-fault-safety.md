# Test Plan 05 - Fault Safety

Scope of this document: the QNX traffic-control system's safety/fault
handling mechanisms - the internal watchdog (PA-10), `FAULT_SAFE`
supervision at intersections (Lx), the "gate fails to confirm
close/open" fault flow at railway crossings (RC-06), fault reporting +
clearing (RC-09/RC-10), and the known limitation around "stuck" sensors.

All test cases below were written by reading the current source code
directly (no guessing):
- `app/intersection/src/lx_watchdog.c`, `app/intersection/src/lx_fsm.c`
- `app/railway/src/rlx_watchdog.c`, `app/railway/src/rlx_gate.c`,
  `app/railway/src/rlx_fsm.c`, `app/railway/src/rlx_sensor.c`
- `app/central/src/c_operator.c`, `app/central/src/c_comm.c`,
  `app/central/src/c_server.c`, `app/central/src/c_main.c`

Guiding principle throughout: **honesty over case count**. If a
scenario has no real way to trigger it via keyboard/IPC in the current
build, the test case says so explicitly and proposes the closest
available alternative (code review, debugger, etc.) instead of
inventing a key or flow that doesn't exist.

## Environment convention (A/B/C)

Per `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`, section "Deployment Topologies":

- **(A) Single node**: runs only ONE process (`rlx_main` or `lx_main`)
  standalone on a machine/VM, no `c_main` needed. Observe behavior via
  that process's own console/stderr (e.g. lines printed by
  `rlx_gate.c`, `rlx_fsm.c`, `rlx_watchdog.c`) and via the sensor
  simulation keys in `rlx_sensor.c`/`lx_sensor.c`. Used for tests that
  don't need Central to issue commands.
- **(B) Multiple nodes on the same QNX machine**: runs `c_main` +
  `rlx_main` (+ `lx_main` if needed) on the **same** VM/machine (same
  Qnet node, no `TRAFFIC_NODE_MAP` needed since `name_open()` treats
  them as "same node"). Used for tests that need Central to send real
  IPC commands (`MSG_REQUEST_FAULT_CLEAR`, `MSG_SET_MODE`, ...) to
  Lx/RLx.
- **(C) Multiple machines/VMs over a real network**: like (B) but
  `c_main` and `lx_main`/`rlx_main` run on different physical
  machines/VMs, connected over real Qnet (Case 1/2/3 in
  QNX_DEPLOYMENT_RUN_GUIDE.md), requiring `TRAFFIC_NODE_MAP` to be
  exported correctly per the guide. Used to reconfirm that safety
  behavior is unchanged under real network/Qnet latency, after already
  PASSing in environment (B).

Recommendation: run the full suite in (A)/(B) first; only repeat a
representative subset (RC-06 positive case, REQUEST_FAULT_CLEAR
NACK/ACK, watchdog) in (C) to confirm no difference from real network
conditions.

## Relevant timing constants (from code, not guessed)

| Constant | Value | Source |
|---|---|---|
| `RLX_WARNING_TO_CLOSING_MS` | 5 s | `rlx_timer.h` |
| `RLX_GATE_MOTION_MS` | 3 s | `rlx_gate.h` |
| `RLX_CLOSING_DEADLINE_MS` | 15 s | `rlx_timer.h` |
| `RLX_OPENING_DEADLINE_MS` | 15 s | `rlx_timer.h` |
| `RLX_EXPECTED_ARRIVAL_MS` | 20 s | `rlx_timer.h` |
| `RLX_OCCUPANCY_WINDOW_MS` | 20 s | `rlx_timer.h` |
| RLx tick period (`IPC_PULSE_RAILWAY_WARNING`) | 1 s | `rlx_main.c` |
| RLx watchdog check period | 3 s (`RLX_WATCHDOG_CHECK_INTERVAL_S`) | `rlx_watchdog.c` |
| Lx tick period (`IPC_PULSE_PHASE_TIMER`) | 100 ms | `lx_main.c` |
| Lx watchdog check period | 2 s (`LX_WATCHDOG_CHECK_INTERVAL_S`) | `lx_watchdog.c` |

Note on watchdog detection latency: the watchdog loop is not
synchronized with the moment the "hang" begins (it just `sleep()`s
then compares the counter to its previous value). So actual detection
latency falls within 1-2 check periods, i.e. roughly 2-4 s for Lx and
3-6 s for RLx, not exactly 2 s/3 s.

---

## Group 1 - RC-06: Gate fails to confirm close/open

Built-in demo mechanism: `rlx_gate_arm_demo_fault()` (`rlx_gate.c`)
sets a one-shot flag `g_demo_fault_armed`; the **next** gate motion
(close or open, whichever is commanded first) will never confirm
(`g_confirmed_closed`/`g_confirmed_open` stays 0), forcing
`rlx_fsm.c` to detect it itself via deadline
(`RLX_CLOSING_DEADLINE_MS`/`RLX_OPENING_DEADLINE_MS`) and call
`enter_fault(FAULT_GATE_CONFIRM_MISSING)`. Trigger key: `x` in
`rlx_sensor.c`.

### TC-FAULT-01: RC-06 - gate fails to confirm close on train arrival -> FAULT, never PROCEED
- **Type**: Positive
- **Related**: RC-06, RC-03
- **Environment**: (A) or (B)
- **Setup**: Start `rlx_main 1` (RL1), crossing at rest in
  `RLX_OPEN` (default at startup).
- **Steps**:
  1. Press `x` (arm demo fault for the next gate motion).
  2. Press `0` (TRAIN_APPROACHING direction 0) -> RLx enters
     `RLX_WARNING`, flashers on (`rlx_signal_show_flashers_on`).
  3. Wait 5 s (`RLX_WARNING_TO_CLOSING_MS`) -> RLx auto-transitions to
     `RLX_CLOSING`, calls `rlx_gate_command_close()` (log
     "commanding gates DOWN..."). Since armed in step 1, this motion
     is flagged `g_fail_this_motion = 1`.
  4. After 3 s of simulated motion (`RLX_GATE_MOTION_MS`), observe log
     "gate FAILED TO CONFIRM (simulated fault) ..." instead of a
     confirmed close.
  5. Keep waiting until total time in `RLX_CLOSING` reaches
     `RLX_CLOSING_DEADLINE_MS` = 15 s from entering CLOSING (about
     10 s after step 4).
- **Expected result**:
  - As soon as `state_elapsed_ms >= 15000` while
    `gates_confirmed_closed()` is still 0,
    `check_closing_or_reclosing_complete()` calls
    `enter_fault(FAULT_GATE_CONFIRM_MISSING)`.
  - Log prints a re-close command (`enter_fault()` unconditionally
    calls `rlx_gate_command_close()`) and
    `rlx_signal_show_fault(FAULT_GATE_CONFIRM_MISSING)`.
  - `fsm->state == RLX_FAULT`, `fsm->faults` has bit
    `FAULT_GATE_CONFIRM_MISSING` set, `fault_report_pending = 1`.
  - **Never** a log line `rlx_signal_show_train_proceed(...)` (PROCEED
    is only called inside `check_closing_or_reclosing_complete()` on a
    confirmed close - that branch never runs here).
  - In environment (B), RL1 sends `MSG_FAULT_REPORT` in the same tick
    (`rlx_main.c` calls `rlx_comm_send_fault_report()` right after
    `rlx_fsm_on_tick()`); Central logs
    `FAULT_REPORT from <RL1>: fault_code=... severity=... detail="..."`.

### TC-FAULT-02: Baseline (control) - normal gate close with no fault -> correct PROCEED
- **Type**: Negative (actually a control/sanity check)
- **Related**: RC-06 (control to validate that TC-FAULT-01's measurement is meaningful)
- **Environment**: (A) or (B)
- **Setup**: `rlx_main 1`, do **not** press `x`.
- **Steps**: Press `0` -> wait 5 s -> CLOSING -> wait 3 s for the gate
  to confirm close normally.
- **Expected result**: After ~3 s (not 15 s), gate confirms closed,
  `fsm->state -> RLX_CLOSED`, log
  `rlx_signal_show_train_proceed(direction=0)` is called, NO fault is
  set. This proves the 15 s deadline violation in TC-FAULT-01 was
  caused solely by the demo-fault flag, not by a bug in normal timing
  logic.

### TC-FAULT-03: RC-06 edge case - gate fails to reconfirm close when a 2nd train arrives while OPENING (RECLOSING)
- **Type**: Edge case
- **Related**: RC-06, RC-04
- **Environment**: (A) or (B)
- **Setup**: Bring RL1 through one normal WARNING->CLOSING->CLOSED->
  TRAIN_PRESENT->OPENING cycle (press `0`, wait long enough for the
  train to "pass" through the full 20 s occupancy window, RLx
  auto-enters `RLX_OPENING` and calls `rlx_gate_command_open()`).
- **Steps**:
  1. Right after RLx enters `RLX_OPENING` (gate mid-open, not yet
     confirmed), press `x` to arm demo fault for the NEXT gate motion.
  2. Immediately press `1` (TRAIN_APPROACHING direction 1) while still
     in `RLX_OPENING`.
  3. Per `rlx_fsm_simulate_train_approaching()`, the `RLX_OPENING` case
     calls `enter_reclosing()`, immediately issuing a NEW
     `rlx_gate_command_close()` motion - and since armed in step 1,
     this close motion never confirms.
  4. Wait the full `RLX_CLOSING_DEADLINE_MS` = 15 s from entering
     `RLX_RECLOSING`.
- **Expected result**: `check_closing_or_reclosing_complete()` (shared
  by both CLOSING and RECLOSING) detects the deadline exceeded ->
  `enter_fault(FAULT_GATE_CONFIRM_MISSING)`, identical to the CLOSING
  branch. While waiting, the externally reported state
  (`map_to_crossing_state`) must be `CROSSING_WARNING` for
  `RLX_RECLOSING` too (per the compliance-fix comment in
  `map_to_crossing_state()`), not jump straight to `CROSSING_CLOSED`
  before the close is actually confirmed.

---

## Group 2 - Fault forces an immediate gate re-close (bugfix just applied in `enter_fault()`)

`enter_fault()` now **unconditionally** calls
`rlx_gate_command_close()` before setting `fsm->state = RLX_FAULT`, so
the gate is never left open/mid-open when a fault occurs (before the
fix, a fault only set a flag and printed "gates held as-is", leaving
the crossing exposed if the fault hit while OPEN/OPENING).

### TC-FAULT-04: Fault occurring while gate is OPENING -> must be commanded closed immediately
- **Type**: Positive (regression test for the bugfix)
- **Related**: RC-06, RC-10, PA-10 (fail-safe output)
- **Environment**: (A) or (B)
- **Setup**: Bring RL1 into `RLX_TRAIN_PRESENT` with exactly one
  occupancy window running (press `0`, let WARNING(5s)->CLOSING(3s, do
  NOT arm fault here so CLOSING succeeds normally)->CLOSED->
  TRAIN_PRESENT happen naturally).
- **Steps**:
  1. While in `RLX_TRAIN_PRESENT` (gate confirmed closed, no open
     command sent yet), press `x` to arm demo fault for the NEXT
     motion (the upcoming gate open).
  2. Wait until the 20 s occupancy window (`RLX_OCCUPANCY_WINDOW_MS`)
     expires -> `active_window_count == 0` -> RLx auto-calls
     `enter_opening()`, i.e. `rlx_gate_command_open()` (log
     "commanding gates UP...").
  3. Since armed in step 1, this open motion never confirms. Wait the
     full `RLX_OPENING_DEADLINE_MS` = 15 s.
- **Expected result**:
  - `check_opening_complete()` detects the deadline exceeded while
    `fsm->state == RLX_OPENING` -> calls
    `enter_fault(FAULT_GATE_CONFIRM_MISSING)`.
  - Observe log: a NEW "commanding gates DOWN (simulated motion,
    3000 ms)" line appears right at the moment FAULT is entered - this
    is exactly the bugfix behavior (before the fix, no re-close would
    be issued, leaving the gate abandoned open/half-open).
  - Since `rlx_gate_command_close()` resets `g_confirmed_open = 0` and
    starts a new close motion (not armed this time, unless the tester
    presses `x` again), after another 3 s the gate will confirm closed
    (`g_confirmed_closed = 1`) even though the FSM is stuck in
    `RLX_FAULT` (no more tick logic runs except `rlx_gate_on_tick()`,
    which runs unconditionally every tick).
  - Expected total wait for the whole scenario: about
    5 + 3 + 20 + 15 ~= 43 s from pressing `0` to entering FAULT.

---

## Group 3 - Central REQUEST_FAULT_CLEAR (RC-09/RC-10: never bypass live verification)

`rlx_fsm_on_fault_clear()` only ACKs when `gates_confirmed_open()`
returns true **at the moment the request is processed**, read directly
from `rlx_gate.c` (no cached value used) - this is exactly what RC-10
requires ("never bypass live verification").

**Key finding from reading the code**: once the FSM is in
`RLX_FAULT`, `enter_fault()` always commands the gate CLOSED (never
open), and the `RLX_FAULT` branch in `rlx_fsm_on_tick()` does nothing
(no open command is ever issued while in fault). `rlx_gate_command_open()`
is, across the entire current codebase, **only** called from
`enter_opening()` (`rlx_fsm.c`), and that function is **only** called
from the `RLX_TRAIN_PRESENT` branch of `rlx_fsm_on_tick()` - never
while `fsm->state == RLX_FAULT`. In other words: **no key or logic
path in the current app can bring the gate to "confirmed open" on its
own while RLx is in RLX_FAULT.** This is not an RC-10 bug (quite the
opposite - it's an inevitable consequence of RC-10: "the gate must
always be re-verified by hand/reality before being considered safe to
reopen") but it means the ACK branch of
`rlx_fsm_on_fault_clear()` **cannot be reproduced via keyboard** in
the current build. TC-FAULT-07 below honestly records this limitation
and proposes the closest feasible workaround (using a debugger) to
still exercise that branch on real QNX hardware.

### TC-FAULT-05: REQUEST_FAULT_CLEAR while gate NOT confirmed open -> NACK (RC-10 core)
- **Type**: Negative
- **Related**: RC-09, RC-10
- **Environment**: (B) - needs real Central to send `MSG_REQUEST_FAULT_CLEAR`
- **Setup**: Bring RL1 into `RLX_FAULT` via TC-FAULT-01 (or any Group
  1/2 scenario). The gate was already commanded closed by
  `enter_fault()` - after 3 s it will confirm CLOSED (not OPEN).
- **Steps**: At the Central console (`c_main`), press `f` ->
  `RLx number (1-3): 1` (enter `1` for RL1) -> Enter.
- **Expected result**:
  - RL1 receives `MSG_REQUEST_FAULT_CLEAR`, calls
    `rlx_fsm_on_fault_clear()`.
  - Since `fsm->state == RLX_FAULT` but `gates_confirmed_open() == 0`
    (gate is closed, not open), returns
    `RESULT_NACK` / `NACK_REASON_FAULT_ACTIVE`.
  - Central log: `C1: REQUEST_FAULT_CLEAR to <RL1> -> NACK reason=FAULT_ACTIVE`
    (per `on_command_reply()` in `c_comm.c`).
  - `fsm->state` remains `RLX_FAULT`, `fsm->faults` unchanged.

### TC-FAULT-06: REQUEST_FAULT_CLEAR when RLx is NOT in fault -> NACK UNKNOWN_TARGET
- **Type**: Negative (edge case)
- **Related**: RC-09
- **Environment**: (B)
- **Setup**: RL1 in normal state (`RLX_OPEN`, no fault).
- **Steps**: From Central, press `f` -> enter RLx = 1.
- **Expected result**: `rlx_fsm_on_fault_clear()` sees
  `fsm->state != RLX_FAULT` -> `RESULT_NACK` /
  `NACK_REASON_UNKNOWN_TARGET`. Central logs
  `... -> NACK reason=UNKNOWN_TARGET`. No state change.

### TC-FAULT-07: REQUEST_FAULT_CLEAR when gate is genuinely confirmed open -> ACK, exits FAULT
- **Type**: Positive - **requires a debug tool, no corresponding key in the current build (see Group 3 intro analysis)**
- **Related**: RC-09, RC-10
- **Environment**: (B), plus a debugger (gdb/QNX Momentics debugger,
  or `pdebug` + `qnx-gdb` from host) attached to the running
  `rlx_main` process on the QNX target. Build `rlx_main` with debug
  info (not stripped) so functions can still be `call`ed by name.
- **Setup**: Bring RL1 into `RLX_FAULT` (TC-FAULT-01/04).
- **Steps**:
  1. Attach the debugger to the `rlx_main` process (no need to stop it long).
  2. Call the public function `rlx_gate_command_open()` directly via
     the debugger, e.g. in gdb: `call rlx_gate_command_open()`. This
     sets `g_motion = GATE_MOVING_OPEN`, `g_remaining_ms = 3000`,
     `g_fail_this_motion = g_demo_fault_armed` (make sure `x` was NOT
     pressed beforehand, so `g_demo_fault_armed == 0` and this motion
     succeeds).
  3. `continue`/resume the process. `rlx_gate_on_tick()` runs
     unconditionally every tick (even when `fsm->state == RLX_FAULT`,
     since that call sits BEFORE the switch on `fsm->state` in
     `rlx_fsm_on_tick()`), so the open motion progresses on its own
     and after 3 s (`RLX_GATE_MOTION_MS`) `g_confirmed_open` becomes 1,
     even though the FSM is still stuck in `RLX_FAULT`.
  4. From Central, press `f` -> enter RLx = 1.
- **Expected result**:
  - `rlx_fsm_on_fault_clear()` sees `fsm->state == RLX_FAULT` AND
    `gates_confirmed_open() == 1` (read live, not cached) -> returns
    `RESULT_ACK`.
  - `fsm->state -> RLX_OPEN`, `fsm->state_elapsed_ms = 0`,
    `fsm->faults = FAULT_NONE`, `active_window_count = 0`, both
    `windows[]` slots reset (`active = 0`) - matching the code that
    clears "stale occupancy bookkeeping" on fault clear.
  - Central log: `C1: REQUEST_FAULT_CLEAR to <RL1> -> ACK`.
  - **If no debugger is available/cannot attach to the QNX target**:
    skip the runtime execution of this test case; verify only by code
    review that the ACK branch in `rlx_fsm_on_fault_clear()` reads
    `gates_confirmed_open()` (not a cached flag) at the moment the
    request is processed - record in the test log as "verified by
    code review only, ACK branch not reproducible via current UI".

### TC-FAULT-08: Repeated back-to-back REQUEST_FAULT_CLEAR while conditions still unmet -> always NACK, no side effects
- **Type**: Negative / idempotency edge case
- **Related**: RC-09, RC-10
- **Environment**: (B)
- **Setup**: Same as TC-FAULT-05 (RL1 in `RLX_FAULT`, gate closed).
- **Steps**: From Central, press `f` -> RLx=1 three times in a row
  (no long wait between).
- **Expected result**: All 3 receive `NACK reason=FAULT_ACTIVE`.
  `fsm->faults`, `fsm->state`, `active_window_count` are not
  changed/corrupted by the repeated calls (no partial state write - the
  NACK branch touches no field other than `reply`).

### TC-FAULT-09: Local `f` key in `rlx_sensor.c` still honors RC-10 (not a "backdoor" bypassing verification)
- **Type**: Negative (clarifying a potential misunderstanding)
- **Related**: RC-09, RC-10
- **Environment**: (A) or (B) - Central not required, since this is a
  local trigger on the RLx process itself.
- **Setup**: RL1 in `RLX_FAULT`, gate closed (not confirmed open) - as
  in TC-FAULT-05.
- **Steps**: At the console of `rlx_main 1` itself, press `f`.
  Per the comment in `rlx_sensor.c`: this is a "DEMO-ONLY local
  fault-clear trigger (bypasses the real MSG_REQUEST_FAULT_CLEAR
  path)" - i.e. it only bypasses the **IPC transport from Central**,
  simulating a technician physically present at the crossing control
  cabinet, NOT bypassing the safety-verification step.
- **Expected result**: The local `f` key calls
  `rlx_fsm_on_fault_clear(fsm, &reply)` directly - **the same function,
  same live verification logic** as when Central calls it via IPC.
  Since the gate isn't confirmed open, the result is still
  `RESULT_NACK`/`NACK_REASON_FAULT_ACTIVE`, printing
  `[rlx_sensor] fault-clear result=... reason=...`. This proves RC-10
  is applied uniformly regardless of whether the request comes from
  Central or a local action - "local" only differs in the request's
  path, not in a separate safety shortcut.

### TC-FAULT-10: Local `f` key ACKs when gate is genuinely open (positive control for TC-FAULT-09)
- **Type**: Positive - requires a debugger like TC-FAULT-07
- **Related**: RC-09, RC-10
- **Environment**: (A), plus a debugger as in TC-FAULT-07
- **Steps**: Repeat steps 1-3 of TC-FAULT-07 (use the debugger to force
  the gate to confirm open while still `RLX_FAULT`), then press `f` at
  the `rlx_main` console instead of via Central.
- **Expected result**: `RESULT_ACK`, `fsm->state -> RLX_OPEN`, identical
  to TC-FAULT-07's result but via the local path - confirming both
  paths (Central IPC and local key) share the exact same safety
  execution point (`rlx_fsm_on_fault_clear()`), with no divergent
  duplicate logic.

---

## Group 4 - FAULT_SAFE at intersections (Lx)

**Finding from reading the code**: `app/intersection/src/lx_sensor.c`
only has keys `a/A/c/C/1/2/3/4/w/W/h/?/q` - none of them call any
function that raises a fault at Lx. The ONLY existing path into
`SUPERVISORY_FAULT_SAFE` is via `lx_fsm_report_watchdog_trip()`,
called from `lx_watchdog_thread()` when the watchdog actually trips
(see Group 5).

**Update (test-plan finding resolved)**: this section previously noted
that `lx_fsm_local_fault_clear()` existed but was never called anywhere
- meaning Lx had no way out of `FAULT_SAFE` other than restarting the
process. That's no longer true: `MSG_REQUEST_FAULT_CLEAR` is now
handled by `lx_main.c`'s `on_request()`, calling
`lx_fsm_on_request_fault_clear()` (`lx_fsm.c`/`lx_fsm.h`), and
`c_operator.c`'s `f` key asks for `node type` (0=Lx, 1=RLx) before
asking for the node number, so a specific Lx can be targeted directly
from the C1 console. Unlike RLx's `rlx_fsm_on_fault_clear()` (which
requires `gates_confirmed_open()==1`), the Lx-side function has no
physical condition to re-verify - it's unconditional/idempotent:
always ACKs, clears `fsm->faults`, and only changes `supervisory` if
actually `FAULT_SAFE`. A related safety fix (re-audit finding, see
`last_crossing_state` in `lx_fsm.h`): this clear correctly resumes
`RAILWAY_PREEMPTION` (not always `NORMAL_OPERATION`) if the adjacent
crossing is still not `CROSSING_OPEN` at the time of the clear - see
TC-FAULT-14b/14c below.

### TC-FAULT-11: Confirm there is no direct fault-trigger key for Lx
- **Type**: Edge case (structural) - **code review only, no runtime
  steps to execute via keyboard**
- **Related**: PA-10, SC-03A
- **Environment**: N/A (code review)
- **Steps**: Read the entire `switch (input)` in
  `lx_sensor_reader_thread()` (`lx_sensor.c`) and all of `lx_fsm.h`/
  `lx_fsm.c` looking for any call to
  `lx_fsm_report_watchdog_trip()` or any setter that sets
  `fsm->faults`.
- **Expected result**: The only place that sets `fsm->faults` is
  `lx_fsm_report_watchdog_trip()` (line `fsm->faults |= FAULT_WATCHDOG_TRIP;`
  in `lx_fsm.c`), and that function has exactly one caller,
  `lx_watchdog_thread()`. Conclusion for the test log: **"Lx has no
  keyboard-triggerable way into FAULT_SAFE in the current build - only
  verifiable via code review, not triggerable via the UI"**, matching
  this document's honesty requirement.

### TC-FAULT-12: FAULT_SAFE always wins, NACKs every new command with FAULT_ACTIVE (review + conditional runtime)
- **Type**: Positive, conditional
- **Related**: SC-03A, PA-09
- **Environment**: (B), depends on successfully tripping the watchdog
  (Group 5, TC-FAULT-16/17)
- **Setup**: If TC-FAULT-16 (forcing an Lx watchdog trip via debugger)
  succeeds, use that same Lx. Otherwise, **skip the runtime part,
  verify by code review only**.
- **Steps (if trip succeeds)**: After Lx enters
  `SUPERVISORY_FAULT_SAFE`, from Central send `m` (SET_MODE),
  `t` (SET_TIMING_PROFILE), `o` (REQUEST_OVERRIDE) to that Lx, one at a
  time.
- **Expected result**: Every command gets `RESULT_NACK`/
  `NACK_REASON_FAULT_ACTIVE` - since every `lx_fsm_on_*()` function
  calls `lx_fsm_check_fault_locked(fsm)` first and checks
  `fsm->supervisory == SUPERVISORY_FAULT_SAFE` before processing the
  verb. **Code review (always doable, unconditional)**: confirm all 5
  handlers (`lx_fsm_on_set_timing_profile`, `lx_fsm_on_set_mode`,
  `lx_fsm_on_request_override`, `lx_fsm_on_crossing_status` - this verb
  is special, always ACKs even during fault since Lx only observes,
  never commands) have the `if (fsm->supervisory == SUPERVISORY_FAULT_SAFE)`
  branch in the right place.

### TC-FAULT-13: FAULT_SAFE forcibly terminates a running override (SC-03A)
- **Type**: Positive, conditional - **interaction matches scenario 6 of the assignment**
- **Related**: SC-03A, PA-10
- **Environment**: (B), depends on TC-FAULT-16
- **Setup**: From Central, issue a `REQUEST_OVERRIDE` (key `o`) on the
  target Lx with a sufficiently long `duration_ms` (e.g. 60000),
  confirm `override_active = 1` in the STATUS/HEARTBEAT shown at the
  Central console (`c_hmi.c`'s status table, `OVERRIDE` column).
- **Steps**: While the override is `OVR_ACTIVE`, force a watchdog trip
  on that Lx (see TC-FAULT-16).
- **Expected result**: `lx_fsm_report_watchdog_trip()` sees
  `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE` -> calls
  `lx_fsm_terminate_override_locked(fsm)` (resetting
  `override_substate` to `OVR_NONE`, `override_remaining_ms = 0`)
  BEFORE setting `fsm->supervisory = SUPERVISORY_FAULT_SAFE`. In the
  next STATUS/HEARTBEAT sent to Central, the `OVERRIDE` column must
  drop to 0 and `SUPERVISORY` must show `FAULT_SAFE` - the override
  never "auto-resumes" after fault clear (per "never automatically
  resumes").
  **If the watchdog can't be tripped via runtime**: verify by code
  review, `lx_fsm.c` lines 470-475 (`lx_fsm_report_watchdog_trip()`)
  calling `lx_fsm_terminate_override_locked()` when
  `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE`, noting "verified
  by code review only".

### TC-FAULT-14: FAULT_SAFE doesn't affect the response to `MSG_CROSSING_STATUS` (Lx only observes, never blocks)
- **Type**: Edge case / mild Negative
- **Related**: RC-02, SC-03A
- **Environment**: N/A (code review; runtime would require both an Lx
  watchdog trip AND a real RLx sending crossing status - a complex
  combination, not required)
- **Steps**: Read `lx_fsm_on_crossing_status()` in `lx_fsm.c`.
- **Expected result**: The function always returns `RESULT_ACK`
  regardless of `fsm->supervisory` (including `FAULT_SAFE`) - matching
  the comment "RC-02: Lx only ever observes crossing status, it never
  rejects it" - but while `FAULT_SAFE`, that update does NOT change
  `fsm->supervisory` (doesn't exit FAULT_SAFE, doesn't enter
  RAILWAY_PREEMPTION). This is correct by design, not a bug -
  confirmed via code review.

### TC-FAULT-14b: REQUEST_FAULT_CLEAR moves Lx out of FAULT_SAFE into NORMAL_OPERATION (fixed - no longer a known gap)
- **Type**: Positive
- **Related**: SC-03A, `lx_fsm_on_request_fault_clear()` (`lx_fsm.c`)
- **Environment**: (B), depends on TC-FAULT-16/17 to trip the Lx
  watchdog first.
- **Setup**: L1 is in `SUPERVISORY_FAULT_SAFE` (watchdog tripped via
  Group 5), and no adjacent RLx is currently pre-empting
  (`last_crossing_state == CROSSING_OPEN`, e.g. never received a
  non-OPEN `CROSSING_STATUS`, or RL1 already reported back `OPEN`
  before the trip).
- **Steps**: At C1: `f` -> node type `0` (Lx) -> Lx number `1`.
- **Expected result**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 1 ->
  ACK`. `fsm->faults` back to `FAULT_NONE`, SUPERVISORY on L1 changes
  `FAULT_SAFE -> NORMAL_OPERATION`. There is no NACK branch for this
  verb on the Lx side (unlike RLx) - always ACK, even called again
  after the fault is already cleared (idempotent).

### TC-FAULT-14c: REQUEST_FAULT_CLEAR while the adjacent crossing is still closed - resumes RAILWAY_PREEMPTION, not NORMAL_OPERATION (safety re-audit fix)
- **Type**: Positive (safety-relevant regression case)
- **Related**: SC-03A, CC-02, `last_crossing_state` (`lx_fsm.h`) -
  before this patch, `lx_fsm_on_request_fault_clear()` always resumed
  `NORMAL_OPERATION` unconditionally, which could allow green toward a
  crossing that's still closed if the fault occurred (or was still
  active) while railway pre-emption was suppressing that intersection.
- **Environment**: (B), needs an adjacent `rlx_main` and a tripped Lx
  watchdog (depends on Group 5).
- **Setup**: Put L1 into real `RAILWAY_PREEMPTION` (adjacent RL1 in
  WARNING/CLOSED, sending `CROSSING_STATUS` other than `CROSSING_OPEN`),
  then trip the Lx watchdog while still pre-empting (SUPERVISORY goes
  straight `RAILWAY_PREEMPTION -> FAULT_SAFE`;
  `lx_fsm_on_crossing_status()` still updates
  `fsm->last_crossing_state` unconditionally even while `FAULT_SAFE` -
  see TC-FAULT-14 above).
- **Steps**: At C1: `f` -> `0` -> `1`, **before** RL1 reports back
  `OPEN`.
- **Expected result**: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`, but
  SUPERVISORY on L1 afterward must be `RAILWAY_PREEMPTION`, **not**
  `NORMAL_OPERATION` - CONNECTOR_GREEN (toward the crossing) remains
  suppressed until L1 actually receives `CROSSING_STATUS(OPEN)` from
  RL1.
  **If the watchdog can't be tripped via runtime**: verify by code
  review, `lx_fsm_on_request_fault_clear()` in `lx_fsm.c` (branch
  `fsm->last_crossing_state != CROSSING_OPEN ? SUPERVISORY_RAILWAY_
  PREEMPTION : SUPERVISORY_NORMAL_OPERATION`), noting "verified by code
  review only".

---

## Group 5 - Watchdog trip (PA-10)

This is the hardest group in the whole document to test at runtime,
because the watchdog is inherently a dead-man's switch: to trigger it
for real, the **main tick-processing thread** (server thread) of the
process must stop responding while the **separate watchdog thread**
(an independent thread in the same process) keeps running its
`sleep()`/counter-comparison loop. Both threads run in the SAME
process, so any way of "freezing the whole process" (including
`kill -STOP`) also stops the watchdog thread itself, so it never gets
a chance to detect anything.

### TC-FAULT-15: Confirm watchdog logic via code review (always doable, used as baseline)
- **Type**: Positive - code review, not runtime
- **Related**: PA-10
- **Environment**: N/A (reading code)
- **Steps**: Read `lx_watchdog_thread()` (`lx_watchdog.c`) and
  `rlx_watchdog_thread()` (`rlx_watchdog.c`).
- **Expected result**:
  - Both run a `for(;;) { sleep(N); compare counter; }` loop with
    N = 2 s (Lx) / 3 s (RLx), independently, on their own thread (not
    sharing a thread with server/client - matching the comment "PA-10
    dead man's switch thread ... alongside the server, client, and
    sensor threads").
  - When the counter is unchanged between two consecutive checks, it
    calls `lx_fsm_report_watchdog_trip(args->fsm)` /
    `rlx_fsm_report_watchdog_trip(args->fsm)` - these functions take
    the lock and set FAULT state themselves, right inside the watchdog
    thread, **independent of whether the server thread is still
    alive** (matching the comment in `lx_fsm.c`: "this function is
    called FROM the watchdog thread itself ... independent of whether
    the server thread ever runs another event again").
  - This is the only test case in Group 5 that **always PASSes,
    without needing a QNX machine** - used as the minimum required
    evidence before attempting the runtime tests below.

### TC-FAULT-16: Counter-proof - `kill -STOP` on the whole process does NOT trigger the watchdog
- **Type**: Negative (deliberately disproving a seemingly reasonable
  approach so the team doesn't waste time retrying it)
- **Related**: PA-10
- **Environment**: (A), real QNX machine, need the PID of
  `rlx_main`/`lx_main` (e.g. via `pidin` on QNX)
- **Steps**:
  1. Run `rlx_main 1`, note the PID (`pidin | grep rlx_main`).
  2. `kill -STOP <pid>`, wait 10 s (much longer than the 3 s check
     interval), then `kill -CONT <pid>`.
- **Expected result**: **NO** fault is raised, no
  "WATCHDOG - no tick activity..." log. Reason (record for
  explanation, not a test "failure"): POSIX `SIGSTOP`/`SIGCONT`
  semantics act on the whole process (every thread), not one thread.
  The watchdog thread is stopped at the same time, so when both
  threads wake up together, `tick_counter` still looks "just
  incremented" as far as the watchdog expects (the elapsed STOP time
  is never perceived as a "gap" by either thread). Conclusion:
  **`kill -STOP` is not a valid way to test PA-10** in this
  single-process multi-thread architecture - a way to block only the
  server thread is needed (see TC-FAULT-17/18).

### TC-FAULT-17: Best-effort - force an Lx watchdog trip via debugger (blocking only the server thread)
- **Type**: Positive - best effort, tool-dependent, may not reproduce
  100% on every setup
- **Related**: PA-10
- **Environment**: (A) or (B), needs gdb/QNX Momentics debugger with
  per-thread control (non-stop mode or equivalent) attached to
  `lx_main`
- **Setup**: Run `lx_main 1` (L1), built with debug symbols.
- **Steps**:
  1. Attach the debugger to the `lx_main` process.
  2. Set a breakpoint inside the server thread's main pulse handler
     (e.g. start of `lx_fsm_on_phase_timer()` in `lx_fsm.c`, or in
     `lx_main.c`'s `on_pulse()`/dispatch loop).
  3. When the breakpoint hits, **hold only the server thread**; if the
     debugger supports it, use "non-stop"/"scheduler-locking off for
     other threads" mode so the other threads (especially the one
     running `lx_watchdog_thread()`) keep running normally.
  4. Hold that state for at least 4 s (2 check periods of 2 s) before
     `continue`ing.
- **Expected result**: After resuming, `lx_main`'s stderr prints
  `Lx: WATCHDOG - no phase-timer activity for 2 s, reporting fault (PA-10)`
  and `lx_fsm_report_watchdog_trip()` is called ->
  `fsm->supervisory == SUPERVISORY_FAULT_SAFE`,
  `fsm->faults & FAULT_WATCHDOG_TRIP` != 0.
- **Honesty note**: many gdb/IDE builds stop ALL threads by default
  when a breakpoint hits (including the watchdog thread), the same
  problem as TC-FAULT-16. If the debug tooling available to the team
  does NOT support holding one thread while others run, this test case
  **cannot be reliably executed at runtime** - in that case, only
  TC-FAULT-15 (code review) is recorded as substitute evidence, rather
  than forcing an uncertain runtime result.

### TC-FAULT-18: Best-effort - force an RLx watchdog trip via debugger (same as TC-FAULT-17, 3 s threshold)
- **Type**: Positive - best effort, same tooling caveat as TC-FAULT-17
- **Related**: PA-10, RC-10 (gate must close on fault)
- **Environment**: (A) or (B), same debugger requirement as TC-FAULT-17
- **Steps**: Same as TC-FAULT-17 but attach to `rlx_main`, breakpoint
  in `rlx_fsm_on_tick()`, hold for at least 6 s (2 periods of 3 s),
  freeze only the server thread, let `rlx_watchdog_thread()` keep
  running.
- **Expected result**: stderr prints
  `RLx: WATCHDOG - no tick activity for 3 s, reporting fault (PA-10)`,
  `rlx_fsm_report_watchdog_trip()` calls `enter_fault(FAULT_WATCHDOG_TRIP)`
  (since `fsm->state != RLX_FAULT` beforehand) -> gate is commanded
  closed immediately (`rlx_gate_command_close()`) even though it may
  have been at rest in `RLX_OPEN` with no motion running at all - the
  key difference from TC-FAULT-04 (where the fault hit mid-way through
  a running OPENING motion): here the fault hits while the crossing is
  completely idle at `RLX_OPEN`, proving `enter_fault()` proactively
  closes the gate rather than just "fixing" an in-progress motion.
- **Honesty note**: same debugger caveat as TC-FAULT-17 applies. If
  infeasible, use TC-FAULT-15 as substitute evidence.

---

## Group 6 - Stuck sensors (known, accepted limitation - not a bug)

Per comments in the source code itself, detecting a sensor "stuck
active" (pedestrian button held continuously, stuck vehicle sensor,
stuck train sensor) is **not implemented** in the current PoC - this
is a previously accepted limitation (documented future work), not a
bug to fix. The purpose of the test cases below is to **confirm
current behavior matches the documented limitation** (i.e. "not
detected" as already known), to avoid mistaking "not detected" for a
new bug during full-system testing.

### TC-FAULT-19: "Stuck" pedestrian button (held/pressed continuously) doesn't set FAULT_PED_BUTTON_STUCK
- **Type**: Negative (confirming an accepted limitation)
- **Related**: PA-03
- **Environment**: (A)
- **Setup**: `lx_main 1`.
- **Steps**: Press key `1` (pedestrian button, side 0) repeatedly over
  a long period (e.g. every 1 s for 2 minutes, simulating a physically
  stuck/held button).
- **Expected result**: `fsm->ped_latched[0]` is still set/held
  correctly per the normal coalescing logic of
  `lx_fsm_latch_pedestrian_request()` (no crash, no infinite loop),
  BUT `fsm->faults` **never** gets bit `FAULT_PED_BUTTON_STUCK` set, no
  matter how long it's held - matching the comment "there is no 'stuck
  active beyond a diagnostic timeout' detection here, so
  FAULT_PED_BUTTON_STUCK ... is never set by this file" in `lx_fsm.c`.
  This is a **passing** result, not a bug to report.

### TC-FAULT-20: "Stuck" vehicle sensor (demand held continuously) doesn't set FAULT_VEHICLE_SENSOR_STUCK
- **Type**: Negative (confirming an accepted limitation)
- **Related**: PA-06
- **Environment**: (A)
- **Steps**: Press `a` (arterial vehicle present) and never press `A`
  (clear) over a long period (several minutes, across multiple phase
  cycles).
- **Expected result**: `fsm->arterial_vehicle_demand` stays 1
  (correctly simulating a genuinely stuck sensor), the system still
  operates normally (in `MODE_OFF_PEAK_SENSOR`, the arterial lane will
  keep being seen as having demand, possibly extending green longer
  than warranted but never exceeding `LX_MAX_GREEN_MS`), but
  `fsm->faults` has no `FAULT_VEHICLE_SENSOR_STUCK` bit - matching the
  current design (no mechanism to measure "how long has this demand
  been active"). Recorded as an **accepted limitation**, not a bug.

### TC-FAULT-21: "Stuck" train sensor (continuous TRAIN_APPROACHING) doesn't set FAULT_TRAIN_SENSOR_STUCK
- **Type**: Negative (confirming an accepted limitation)
- **Related**: RC-11
- **Environment**: (A)
- **Steps**: While RL1 is in `RLX_WARNING`, press `0` repeatedly before
  the 5 s (`RLX_WARNING_TO_CLOSING_MS`) elapses, simulating a stuck
  train-approach sensor (continuously confirming "train present" even
  though it might not be).
- **Expected result**: Per `rlx_fsm_simulate_train_approaching()`, the
  `RLX_WARNING` case only calls `register_window()` again (self-loop),
  without resetting the gate-close timer and without setting any
  fault. Per the comment in `rlx_fsm_on_tick()`'s `RLX_WARNING` case:
  the constant `RLX_WARNING_DIAGNOSTIC_TIMEOUT_MS` (60 s) exists in
  `rlx_timer.h` but is **RESERVED, never referenced anywhere** - real
  "stuck active" logic would need a continuous sensor signal line,
  which discrete keypress events cannot simulate. So `fsm->faults`
  will **never** get bit `FAULT_TRAIN_SENSOR_STUCK` set in the current
  build, no matter how many times or how long `0` is pressed. This is
  a limitation already documented by the dev team in the code (not a
  new finding); both code review and runtime observation reach the
  same conclusion.

---

## Summary of runtime-testable scope on real QNX hardware

| Group | Testable at runtime (no debugger) | Needs debugger | Code review only |
|---|---|---|---|
| 1. RC-06 gate confirm | TC-01, TC-02, TC-03 | - | - |
| 2. Fault forces gate close | TC-04 | - | - |
| 3. REQUEST_FAULT_CLEAR | TC-05, TC-06, TC-08, TC-09 | TC-07, TC-10 | - |
| 4. FAULT_SAFE at Lx | TC-14b (depends on Group 5 to trip first) | TC-12, TC-13, TC-14c (depend on Group 5) | TC-11, TC-14 |
| 5. Watchdog trip | (counter-proof) TC-16 | TC-17, TC-18 | TC-15 |
| 6. Stuck sensor | TC-19, TC-20, TC-21 | - | - |

23 test cases (TC-FAULT-14b/14c added after `MSG_REQUEST_FAULT_CLEAR`
was wired up for Lx). Most (14/23) run entirely via keyboard on real
QNX hardware with no extra tooling; 6 test cases need a debugger
(caveats noted if infeasible); 2 test cases are pure code review due
to the current architecture (Lx has no fault-trigger key), which rules
out runtime execution; 1 test case (TC-16) is a deliberate
counter-proof.
