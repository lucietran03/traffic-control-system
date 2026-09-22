# Test Plan 05 - Safety / Fault Handling (Fault-Safety)

Scope of this document: the safety mechanisms and fault-handling logic of
the QNX traffic control system - the internal watchdog (PA-10), FAULT_SAFE
supervision at intersections (Lx), the "gate confirmation missing on
close/open" fault flow at the rail crossing (RC-06), fault reporting +
fault clearing (RC-09/RC-10), and the known limitation around "stuck"
sensors (stuck-sensor).

All test cases below were written by reading the current source code
directly (no guessing):
- `app/intersection/src/lx_watchdog.c`, `app/intersection/src/lx_fsm.c`
- `app/railway/src/rlx_watchdog.c`, `app/railway/src/rlx_gate.c`,
  `app/railway/src/rlx_fsm.c`, `app/railway/src/rlx_sensor.c`
- `app/central/src/c_operator.c`, `app/central/src/c_comm.c`,
  `app/central/src/c_server.c`, `app/central/src/c_main.c`

Guiding principle throughout this document: **honesty over headcount**. If
a scenario has no real trigger path through keyboard/IPC in the current
build, the test case will say so plainly and propose the closest feasible
alternative (code review, debugger, etc.) rather than inventing a
key/flow that doesn't exist.

## Environment convention (A/B/C)

Per `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`, section "Deployment Topologies":

- **(A) Single node**: run only ONE process (`rlx_main` or `lx_main`)
  standalone on one machine/VM QNX, no `c_main` needed. Observe behavior
  via that process's own console/stderr (e.g. lines printed by
  `rlx_gate.c`, `rlx_fsm.c`, `rlx_watchdog.c`) and via the sensor
  simulation keys of `rlx_sensor.c`/`lx_sensor.c`. Used for tests that
  don't need Central to issue commands.
- **(B) Multiple nodes on the same QNX machine**: run `c_main` +
  `rlx_main` (+ `lx_main` if needed) on the **same** QNX VM/machine (same
  Qnet node, no need to declare `TRAFFIC_NODE_MAP` since `name_open()`
  treats them as "same node"). Used for tests that need Central to send
  real IPC commands (`MSG_REQUEST_FAULT_CLEAR`, `MSG_SET_MODE`, ...) to
  Lx/RLx.
- **(C) Multiple machines/VMs QNX over a real network**: like (B) but
  `c_main` and `lx_main`/`rlx_main` run on different physical
  machines/VMs, connected via real Qnet (Case 1/2/3 in
  QNX_DEPLOYMENT_RUN_GUIDE.md), requiring `TRAFFIC_NODE_MAP` to be
  exported correctly per the guide. Used to re-confirm that safety
  behavior does not change with real network/Qnet latency involved, after
  already PASSing in environment (B).

Recommendation: run the entire test suite in (A)/(B) first; only a
representative subset (RC-06 positive, REQUEST_FAULT_CLEAR NACK/ACK,
watchdog) needs to be repeated in (C) to confirm there is no difference
due to a real network.

General note when running the tests in this file (environments (B)/(C)
with a live `c_main`): avoid pressing `d`/`a` on the shared C1 console
while running the fault-injection tests in this file - if a real
peak/off-peak boundary or demo happens to fire mid-test, an unexpected
SET_MODE NACK line may appear (due to `lx_fsm_on_set_mode()`'s fault
guard), which is not a bug.

## Related timing constants (looked up from code, not guessed)

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

Note on watchdog detection latency: the watchdog loop is not synchronized
with the moment a "hang" begins (it just `sleep()`s then compares the
counter to the previous value). So actual detection latency falls within
1-2 check periods, i.e. roughly 2-4 s for Lx and 3-6 s for RLx, not
exactly 2 s/3 s.

---

## Group 1 - RC-06: Gate confirmation missing on close/open

Available demo mechanism: `rlx_gate_arm_demo_fault()` (`rlx_gate.c`) sets
the one-shot flag `g_demo_fault_armed`; the **next gate movement** (close
or open, whichever is commanded first) will never confirm completion
(`g_confirmed_closed`/`g_confirmed_open` stay at 0), forcing `rlx_fsm.c`
to detect this itself via the deadline (`RLX_CLOSING_DEADLINE_MS`/
`RLX_OPENING_DEADLINE_MS`) and call
`enter_fault(FAULT_GATE_CONFIRM_MISSING)`.
Trigger key: `x` in `rlx_sensor.c`.

### TC-FAULT-01: RC-06 - gate fails to confirm closed when a train arrives -> FAULT, never PROCEED
- **Type**: Positive
- **Related**: RC-06, RC-03
- **Environment**: (A) or (B)
- **Setup**: Start `rlx_main 1` (RL1), crossing at rest state `RLX_OPEN`
  (default on startup).
- **Steps**:
  1. Press `x` (arm demo fault for the next gate movement).
  2. Press `0` (TRAIN_APPROACHING direction 0) -> RLx enters `RLX_WARNING`,
     flashers turn on (`rlx_signal_show_flashers_on`).
  3. Wait 5 s (`RLX_WARNING_TO_CLOSING_MS`) -> RLx automatically switches
     to `RLX_CLOSING`, calls `rlx_gate_command_close()` (log
     "commanding gates DOWN..."). Since armed in step 1, this motion is
     flagged `g_fail_this_motion = 1`.
  4. After 3 s of simulated motion (`RLX_GATE_MOTION_MS`), observe the log
     "gate FAILED TO CONFIRM (simulated fault) ..." instead of a gate
     close confirmation.
  5. Keep waiting until the total time spent in `RLX_CLOSING` reaches
     `RLX_CLOSING_DEADLINE_MS` = 15 s from entering CLOSING (i.e. about
     10 s after step 4).
- **Expected Result**:
  - As soon as `state_elapsed_ms >= 15000` while
    `gates_confirmed_closed()` is still 0,
    `check_closing_or_reclosing_complete()` calls
    `enter_fault(FAULT_GATE_CONFIRM_MISSING)`.
  - A log line commands the gate closed again (`enter_fault()`
    unconditionally calls `rlx_gate_command_close()`) and
    `rlx_signal_show_fault(FAULT_GATE_CONFIRM_MISSING)`.
  - `fsm->state == RLX_FAULT`, `fsm->faults` has the
    `FAULT_GATE_CONFIRM_MISSING` bit set, `fault_report_pending = 1`.
  - **Never** a `rlx_signal_show_train_proceed(...)` log line (the
    PROCEED function is only called inside
    `check_closing_or_reclosing_complete()` when the gate confirms closed
    successfully - this branch never runs).
  - If run in environment (B), RL1 sends `MSG_FAULT_REPORT` in the same
    tick (`rlx_main.c` calls `rlx_comm_send_fault_report()` right after
    `rlx_fsm_on_tick()`); Central logs the line
    `FAULT_REPORT from <RL1>: fault_code=... severity=... detail="..."`.

### TC-FAULT-02: Baseline (control) - normal gate close without a fault -> correct PROCEED
- **Type**: Negative (actually a control/sanity check)
- **Related**: RC-06 (control to ensure the measurement in TC-FAULT-01 is meaningful)
- **Environment**: (A) or (B)
- **Setup**: `rlx_main 1`, do **not** press `x`.
- **Steps**: Press `0` -> wait 5 s -> CLOSING -> wait 3 s for the gate to
  confirm closed normally.
- **Expected Result**: After exactly ~3 s (not 15 s), the gate confirms
  closed, `fsm->state -> RLX_CLOSED`, the log
  `rlx_signal_show_train_proceed(direction=0)` is called, NO fault is
  set. This test proves that the 15 s deadline violated in TC-FAULT-01 is
  solely due to the demo-fault flag, not a random glitch in normal timing
  logic.

### TC-FAULT-03: RC-06 edge case - gate fails to confirm re-closed when a second train arrives during OPENING (RECLOSING)
- **Type**: Edge case
- **Related**: RC-06, RC-04
- **Environment**: (A) or (B)
- **Setup**: Take RL1 through one normal WARNING->CLOSING->CLOSED->
  TRAIN_PRESENT->OPENING cycle (press `0`, wait long enough for the train
  to "pass" through the full 20 s occupancy window, RLx automatically
  enters `RLX_OPENING` and calls `rlx_gate_command_open()`).
- **Steps**:
  1. As soon as RLx has just entered `RLX_OPENING` (gate mid-opening, not
     yet confirmed open), press `x` to arm the demo fault for the NEXT
     gate movement.
  2. Immediately after, press `1` (TRAIN_APPROACHING direction 1) while
     still in `RLX_OPENING`.
  3. Per `rlx_fsm_simulate_train_approaching()`, the `RLX_OPENING` case
     calls `enter_reclosing()`, i.e. immediately
     `rlx_gate_command_close()` starts a NEW motion - and since armed in
     step 1, this close motion will never confirm completion.
  4. Wait the full `RLX_CLOSING_DEADLINE_MS` = 15 s from entering
     `RLX_RECLOSING`.
- **Expected Result**: `check_closing_or_reclosing_complete()` (shared by
  both CLOSING and RECLOSING) detects the deadline exceeded ->
  `enter_fault(FAULT_GATE_CONFIRM_MISSING)`, identical to the CLOSING
  branch. While waiting, the externally reported state
  (`map_to_crossing_state`) must be `CROSSING_WARNING` for
  `RLX_RECLOSING` too (per the compliance-fix comment in
  `map_to_crossing_state()`), and must not jump straight to
  `CROSSING_CLOSED` before actually confirming closed.

---

## Group 2 - Fault forces the gate to close immediately (bugfix just applied in `enter_fault()`)

`enter_fault()` now **unconditionally** calls `rlx_gate_command_close()`
before setting `fsm->state = RLX_FAULT`, so the gate is never left in an
open/half-open position when a fault occurs (before the fix, a fault only
set a flag and printed "gates held as-is", leaving the crossing exposed
if the fault happened while OPEN/OPENING).

### TC-FAULT-04: Fault occurring while gate is OPENING -> must be commanded closed immediately
- **Type**: Positive (regression test for the bugfix)
- **Related**: RC-06, RC-10, PA-10 (fail-safe output)
- **Environment**: (A) or (B)
- **Setup**: Take RL1 into `RLX_TRAIN_PRESENT` with exactly 1 occupancy
  window running (press `0`, let the WARNING(5s)->CLOSING(3s, do NOT arm
  fault at this step so CLOSING succeeds normally)->CLOSED->
  TRAIN_PRESENT cycle happen naturally).
- **Steps**:
  1. While in `RLX_TRAIN_PRESENT` (gate confirmed closed, no open command
     sent yet), press `x` to arm the demo fault for the NEXT movement
     (which will be the upcoming gate open).
  2. Wait until the 20 s occupancy window (`RLX_OCCUPANCY_WINDOW_MS`)
     expires -> `active_window_count == 0` -> RLx automatically calls
     `enter_opening()`, i.e. `rlx_gate_command_open()` (log "commanding
     gates UP...").
  3. Since armed in step 1, this open motion never confirms completion.
     Wait the full `RLX_OPENING_DEADLINE_MS` = 15 s.
- **Expected Result**:
  - `check_opening_complete()` detects the deadline exceeded while
    `fsm->state == RLX_OPENING` -> calls
    `enter_fault(FAULT_GATE_CONFIRM_MISSING)`.
  - Observe a NEW log line "commanding gates DOWN (simulated motion,
    3000 ms)" appearing right at the moment of entering FAULT - this is
    exactly the bugfix's behavior (before the fix, no re-close command
    would occur, and the gate would be left as-is in an open/half-open
    state).
  - Since `rlx_gate_command_close()` resets `g_confirmed_open = 0` and
    starts a new close motion (not armed with a fault this time, unless
    the tester presses `x` again deliberately), after another 3 s the
    gate will confirm closed successfully (`g_confirmed_closed = 1`)
    even though the FSM has stopped at `RLX_FAULT` (no longer processing
    tick logic other than `rlx_gate_on_tick()`, which runs
    unconditionally every tick).
  - Total expected wait time for the whole scenario: roughly
    5 + 3 + 20 + 15 ≈ 43 s from pressing `0` to entering FAULT.

---

## Group 3 - Central REQUEST_FAULT_CLEAR (RC-09/RC-10: never bypass real verification)

`rlx_fsm_on_fault_clear()` only ACKs when `gates_confirmed_open()`
returns true **at the moment the request is processed**, reading directly
from `rlx_gate.c` (no other cached value used) - this is exactly what
RC-10 requires ("never bypass live verification").

**Update (known gap now resolved)**: this section used to note that once
the FSM had entered `RLX_FAULT`, no key or logic flow in the application
could bring the gate back to a "confirmed open" state on its own, since
`enter_fault()` always commands the gate CLOSED and
`rlx_gate_command_open()` is only ever called from `enter_opening()`
(the `RLX_TRAIN_PRESENT` branch of `rlx_fsm_on_tick()`, never run while
`fsm->state == RLX_FAULT`) anywhere in the codebase - the conclusion at
the time was that the ACK branch of `rlx_fsm_on_fault_clear()` could only
be tested with a debugger (`call rlx_gate_command_open()`). **That is no
longer true**: `rlx_sensor.c` now has key `r`, which calls
`rlx_gate_force_confirmed_open()` (`rlx_gate.c`) directly - this function
forces the simulated gate sensor state to `g_confirmed_open = 1`/
`g_confirmed_closed = 0` immediately (simulating "gate mechanism
physically repaired/confirmed OPEN", per the RC-09/RC-10 fault-clear demo
comment in the code), independent of `rlx_fsm.c`'s state machine (it does
not go through `enter_opening()` or any simulated motor). Since
`rlx_fsm_on_fault_clear()` reads `gates_confirmed_open()` live at the
moment of processing (no cache), pressing `r` then sending
`MSG_REQUEST_FAULT_CLEAR` from Central creates a **real** ACK branch,
**fully reproducible via keyboard, no debugger needed** - see
TC-FAULT-22 (added) below. TC-FAULT-07/TC-FAULT-10 below are still kept
verbatim because they illustrate how to reach the same result via
debugger (useful for verifying independently of `rlx_sensor.c`'s `r` key,
or on a build without that key), but they are **no longer the only way**.

### TC-FAULT-05: REQUEST_FAULT_CLEAR while gate NOT confirmed open -> NACK (RC-10 core)
- **Type**: Negative
- **Related**: RC-09, RC-10
- **Environment**: (B) - needs a real Central to send `MSG_REQUEST_FAULT_CLEAR`
- **Setup**: Bring RL1 into `RLX_FAULT` via TC-FAULT-01 (or any scenario
  in Group 1/2). The gate has already been commanded closed by
  `enter_fault()` - after 3 s it will confirm CLOSED (not OPEN).
- **Steps**: At the Central console (`c_main`), press `f` ->
  `RLx number (1-3): 1` (enter `1` for RL1) -> Enter.
- **Expected Result**:
  - RL1 receives `MSG_REQUEST_FAULT_CLEAR`, calls
    `rlx_fsm_on_fault_clear()`.
  - Since `fsm->state == RLX_FAULT` but `gates_confirmed_open() == 0`
    (gate closed, not open), returns `RESULT_NACK` /
    `NACK_REASON_FAULT_ACTIVE`.
  - Central log: `C1: REQUEST_FAULT_CLEAR to <RL1> -> NACK reason=FAULT_ACTIVE`
    (per `on_command_reply()` in `c_comm.c`).
  - `fsm->state` remains `RLX_FAULT`, `fsm->faults` unchanged.

### TC-FAULT-06: REQUEST_FAULT_CLEAR while RLx is NOT faulted at all -> NACK UNKNOWN_TARGET
- **Type**: Negative (edge case)
- **Related**: RC-09
- **Environment**: (B)
- **Setup**: RL1 in normal state (`RLX_OPEN`, no fault).
- **Steps**: From Central, press `f` -> enter RLx = 1.
- **Expected Result**: `rlx_fsm_on_fault_clear()` sees
  `fsm->state != RLX_FAULT` -> `RESULT_NACK` /
  `NACK_REASON_UNKNOWN_TARGET`. Central logs
  `... -> NACK reason=UNKNOWN_TARGET`. No state change.

### TC-FAULT-07: REQUEST_FAULT_CLEAR when the gate has actually confirmed open -> ACK, exit FAULT
- **Type**: Positive - uses a debug tool to force `g_confirmed_open`.
  **No longer the only way**: see TC-FAULT-22 (key `r` in
  `rlx_sensor.c`) for an equivalent keyboard-only path with no debugger
  (see updated analysis at the top of Group 3).
- **Related**: RC-09, RC-10
- **Environment**: (B), plus a debugger (gdb/QNX Momentics debugger, or
  `pdebug` + `qnx-gdb` from the host) attached to the running `rlx_main`
  process on target QNX. Build `rlx_main` with debug info (not stripped)
  so calling functions by name still works.
- **Setup**: Bring RL1 into `RLX_FAULT` (TC-FAULT-01/04).
- **Steps**:
  1. Attach the debugger to the `rlx_main` process (no need to hold it
     long).
  2. Call the public function `rlx_gate_command_open()` directly via the
     debugger, e.g. in gdb: `call rlx_gate_command_open()`. This function
     sets `g_motion = GATE_MOVING_OPEN`, `g_remaining_ms = 3000`,
     `g_fail_this_motion = g_demo_fault_armed` (make sure `x` was NOT
     pressed beforehand, so `g_demo_fault_armed == 0` and this motion
     succeeds).
  3. `continue`/resume the process. `rlx_gate_on_tick()` is called
     unconditionally every tick (even when `fsm->state == RLX_FAULT`,
     since this call sits BEFORE the switch on `fsm->state` in
     `rlx_fsm_on_tick()`), so the open motion progresses on its own and
     after 3 s (`RLX_GATE_MOTION_MS`) `g_confirmed_open` becomes 1, even
     though the FSM is still stopped at `RLX_FAULT`.
  4. From Central, press `f` -> enter RLx = 1.
- **Expected Result**:
  - `rlx_fsm_on_fault_clear()` sees `fsm->state == RLX_FAULT` AND
    `gates_confirmed_open() == 1` (read live, no cache) -> returns
    `RESULT_ACK`.
  - `fsm->state -> RLX_OPEN`, `fsm->state_elapsed_ms = 0`,
    `fsm->faults = FAULT_NONE`, `active_window_count = 0`, both
    `windows[]` slots reset (`active = 0`) - per the code that clears
    "stale occupancy bookkeeping" when clearing a fault.
  - Central log: `C1: REQUEST_FAULT_CLEAR to <RL1> -> ACK`.
  - **If no debugger is available/cannot attach to the target QNX**:
    skip the runtime execution part of this test case, only verify by
    code review that the ACK branch in `rlx_fsm_on_fault_clear()` reads
    `gates_confirmed_open()` (not a cached flag) at the exact moment of
    processing the request - record clearly in the test report as
    "verified by code review only, the ACK branch cannot be reproduced
    via the current UI".

### TC-FAULT-08: Repeated consecutive REQUEST_FAULT_CLEAR while conditions still not met -> always NACK, no side effects
- **Type**: Negative / idempotency edge case
- **Related**: RC-09, RC-10
- **Environment**: (B)
- **Setup**: Same as TC-FAULT-05 (RL1 in `RLX_FAULT`, gate closed).
- **Steps**: From Central, press `f` -> RLx=1 three times in a row (no
  long wait in between).
- **Expected Result**: All 3 attempts get `NACK reason=FAULT_ACTIVE`.
  `fsm->faults`, `fsm->state`, `active_window_count` are not
  changed/corrupted by the repeated calls (no field is partially
  overwritten and left inconsistent - the NACK branch touches no field
  other than `reply`).

### TC-FAULT-09: Local `f` key in `rlx_sensor.c` still complies with RC-10 (not a "backdoor" bypassing verification)
- **Type**: Negative (clarifying a potential misunderstanding)
- **Related**: RC-09, RC-10
- **Environment**: (A) or (B) - Central not required, since this is a
  trigger local to the RLx process itself.
- **Setup**: RL1 in `RLX_FAULT`, gate closed (not confirmed open) - same
  as TC-FAULT-05.
- **Steps**: At the console of `rlx_main 1` itself, press `f`. Per the
  comment in `rlx_sensor.c`: this is a "DEMO-ONLY local fault-clear
  trigger (bypasses the real MSG_REQUEST_FAULT_CLEAR path)" - i.e. it
  only bypasses the **IPC transport from Central**, simulating a
  technician present directly at the crossing control cabinet, NOT
  bypassing the safety verification step.
- **Expected Result**: The local `f` key calls
  `rlx_fsm_on_fault_clear(fsm, &reply)` directly - **the same function,
  same live verification logic** as when Central calls via IPC. Since
  the gate is not confirmed open, the result is still
  `RESULT_NACK`/`NACK_REASON_FAULT_ACTIVE`, printing
  `[rlx_sensor] fault-clear result=... reason=...`. This test proves
  RC-10 is applied uniformly regardless of the request's origin (Central
  or a local action) - "local" here only differs in the request's
  transport path, not a different safety shortcut.

### TC-FAULT-10: Local `f` key ACKs when the gate has really opened (positive control for TC-FAULT-09, now keyboard-only via `r`, no debugger needed)
- **Type**: Positive — **update: debugger no longer needed**, uses key
  `r` (`rlx_gate_force_confirmed_open()`) like TC-FAULT-22, the only
  difference being pressing `f` at the `rlx_main` console itself instead
  of via Central. (`tools/test-automation/cases/05-fault-safety.json`'s
  `TC-FAULT-10` already uses exactly this sequence.)
- **Related**: RC-09, RC-10
- **Environment**: (A) — `rlx_main 1` standalone, no Central needed, no
  debugger needed.
- **Steps**:
  1. Press `x` then `0` on RL1 to enter `RLX_FAULT` (same as
     TC-FAULT-01/TC-FAULT-09).
  2. After `"RLx: FAULT latched (GATE_CONFIRM_MISSING, bit 0x1)"`
     appears, press `r` to force `g_confirmed_open=1` (log
     `"[DEMO] Gate mechanism simulated as physically repaired - now
     confirmed OPEN"`).
  3. Press `f` right at the `rlx_main` console (not via Central).
- **Expected Result**: `RESULT_ACK`, `fsm->state -> RLX_OPEN`, identical
  to the result of TC-FAULT-07/TC-FAULT-22 but reached via the local
  path - confirming that both paths (Central IPC and the local key) share
  exactly the same safety logic execution point
  (`rlx_fsm_on_fault_clear()`), with no divergent duplicate logic.

### TC-FAULT-22: REQUEST_FAULT_CLEAR via Central after pressing `r` to force gate confirmed-open -> real ACK, exit FAULT, NO debugger needed (added, replaces the old known-gap)
- **Type**: Positive - **keyboard-only, no debugger needed** (see the
  update at the top of Group 3: the previous known gap has been resolved
  thanks to key `r` in `rlx_sensor.c`)
- **Related**: RC-09, RC-10
- **Environment**: (B) - needs a real Central to send
  `MSG_REQUEST_FAULT_CLEAR` to RL1 (same as TC-FAULT-05/07), but no
  debugger attachment needed.
- **Setup**: Bring RL1 into `RLX_FAULT` via TC-FAULT-01 (or any scenario
  in Group 1/2) - e.g. press `x` then `0`, wait the full
  `RLX_CLOSING_DEADLINE_MS` = 15 s for
  `enter_fault(FAULT_GATE_CONFIRM_MISSING)` to be called. The gate has
  already been commanded closed by `enter_fault()` and (after 3 s)
  confirms CLOSED on its own.
- **Steps**:
  1. At the console of `rlx_main 1` (RL1) itself, press `r`. Per
     `rlx_sensor.c`, this key calls `rlx_gate_force_confirmed_open()`
     (`rlx_gate.c`) directly, simulating "gate mechanism physically
     repaired/confirmed OPEN": this function sets `g_motion = GATE_IDLE`,
     `g_confirmed_closed = 0`, `g_confirmed_open = 1` immediately,
     independent of `rlx_fsm.c` (the FSM is still stopped at
     `RLX_FAULT`, not going through `enter_opening()`). Observe the log
     `[DEMO] Gate mechanism simulated as physically repaired - now
     confirmed OPEN (RC-09/RC-10 fault-clear demo path)`.
  2. At the Central console (`c_main`), press `f` ->
     `RLx number (1-3): 1` -> Enter.
- **Expected Result**:
  - RL1 receives `MSG_REQUEST_FAULT_CLEAR`, calls
    `rlx_fsm_on_fault_clear()`.
  - Since `fsm->state == RLX_FAULT` AND `gates_confirmed_open()` reads
    live as 1 (thanks to step 1) -> returns a real `RESULT_ACK` (not
    NACK).
  - `fsm->state -> RLX_OPEN`, `fsm->state_elapsed_ms = 0`,
    `fsm->faults = FAULT_NONE`, `active_window_count = 0`, both
    `windows[]` slots reset - identical to the expected result of
    TC-FAULT-07 but achieved **entirely via keyboard**, no
    debugger/gdb/QNX Momentics needed.
  - Central log: `C1: REQUEST_FAULT_CLEAR to <RL1> -> ACK`.
  - RL1 returns to normal operation at `RLX_OPEN` (lights/flasher off,
    ready to receive the next `TRAIN_APPROACHING` via key `0`/`1` as
    usual) - confirming the ACK branch of `rlx_fsm_on_fault_clear()` can
    not only be verified by code review but also reproduced with a real
    keyboard action sequence on a QNX machine (environment (B)).

---

## Group 4 - FAULT_SAFE at intersections (Lx)

**Finding from reading the code**: `app/intersection/src/lx_sensor.c` only
has keys `a/A/c/C/1/2/3/4/w/W/h/?/q` - there is no key at all that calls
any function raising a fault at Lx. The ONLY current path for Lx to enter
`SUPERVISORY_FAULT_SAFE` is via `lx_fsm_report_watchdog_trip()`, called
from `lx_watchdog_thread()` when the watchdog actually trips (see Group
5).

**Update (test-plan finding now fixed)**: this section used to note that
`lx_fsm_local_fault_clear()` exists but is never called anywhere - i.e.
Lx had no way out of `FAULT_SAFE` other than restarting the process. That
is no longer true: `MSG_REQUEST_FAULT_CLEAR` is now handled by
`lx_main.c`'s `on_request()`, which calls
`lx_fsm_on_request_fault_clear()` (`lx_fsm.c`/`lx_fsm.h`), and
`c_operator.c`'s `f` key asks for `node type` (0=Lx, 1=RLx) before asking
for the number, so an Lx can be targeted directly from the C1 console.
Unlike RLx's `rlx_fsm_on_fault_clear()` (which requires
`gates_confirmed_open()==1`), the Lx-side function has no physical
condition to re-verify - it is unconditional/idempotent: always ACKs,
clears `fsm->faults`, and only changes `supervisory` if it is actually
`FAULT_SAFE`. A related safety fix (re-audit finding, see
`last_crossing_state` in `lx_fsm.h`): this clear correctly resumes
`RAILWAY_PREEMPTION` (not always `NORMAL_OPERATION`) if the adjacent
crossing is still not `CROSSING_OPEN` at the time of clearing - see
TC-FAULT-14b/14c below.

### TC-FAULT-11: Confirm there is no direct fault-trigger key for Lx
- **Type**: Edge case (structural) - **code review only, no runtime step
  to perform via keyboard**
- **Related**: PA-10, SC-03A
- **Environment**: N/A (code review)
- **Steps**: Read the entire `switch (input)` in
  `lx_sensor_reader_thread()` (`lx_sensor.c`) and all of `lx_fsm.h`/
  `lx_fsm.c` to find every call to `lx_fsm_report_watchdog_trip()` or any
  setter that sets `fsm->faults`.
- **Expected Result**: There is exactly 1 place that sets `fsm->faults`:
  `lx_fsm_report_watchdog_trip()` (the line `fsm->faults |=
  FAULT_WATCHDOG_TRIP;` in `lx_fsm.c`), and this function has exactly 1
  caller: `lx_watchdog_thread()`. Conclusion to record in the test
  report: **"Lx has no keyboard-triggerable path to FAULT_SAFE in the
  current build - verifiable only by code review, not triggerable via
  the UI"**, exactly per this document's honesty requirement.

### TC-FAULT-12: FAULT_SAFE always wins, NACKs every new command with FAULT_ACTIVE (review + conditional runtime)
- **Type**: Positive, conditional
- **Related**: SC-03A, PA-09
- **Environment**: (B), depends on successfully tripping the watchdog via
  debugger (Group 5, TC-FAULT-17 — **not TC-FAULT-16**, which is the
  counter-proof case showing `kill -STOP` WITHOUT a debugger does NOT
  trip it; TC-FAULT-17 is the real debugger path)
- **Setup**: If TC-FAULT-17 (forcing the Lx watchdog to trip via
  debugger) succeeds, use that same Lx. If not (no suitable debugger
  available), **skip the runtime portion, verify by code review only —
  do not record a Pass for the runtime portion unless it was actually
  tripped**.
- **Steps (if tripped)**: After Lx enters `SUPERVISORY_FAULT_SAFE`, from
  Central send `m` (SET_MODE), `t` (SET_TIMING_PROFILE), `o`
  (REQUEST_OVERRIDE) in turn to that Lx.
- **Expected Result**: Every command gets `RESULT_NACK`/
  `NACK_REASON_FAULT_ACTIVE` - because every `lx_fsm_on_*()` function
  calls `lx_fsm_check_fault_locked(fsm)` first thing and checks
  `fsm->supervisory == SUPERVISORY_FAULT_SAFE` before processing the
  verb. **Code review (always feasible, unconditional)**: confirm all 5
  handlers (`lx_fsm_on_set_timing_profile`, `lx_fsm_on_set_mode`,
  `lx_fsm_on_request_override`, `lx_fsm_on_crossing_status` - this verb
  is special, always ACKs even while faulted since Lx only observes, it
  doesn't use it to issue commands) each have an
  `if (fsm->supervisory == SUPERVISORY_FAULT_SAFE)` branch in the right
  place.

### TC-FAULT-13: FAULT_SAFE forces termination of an active override (SC-03A)
- **Type**: Positive, conditional - **interaction exactly per scenario 6
  of the assignment**
- **Related**: SC-03A, PA-10
- **Environment**: (B), depends on TC-FAULT-17 (the debugger path —
  TC-FAULT-16 is the kill -STOP counter-proof case that does NOT trip
  it, not used here)
- **Setup**: From Central, grant a `REQUEST_OVERRIDE` (key `o`) to the
  target Lx with a sufficiently long `duration_ms` (e.g. 60000), confirm
  `override_active = 1` in the STATUS/HEARTBEAT display on the Central
  console (`c_hmi.c`'s status table, `OVERRIDE` column).
- **Steps**: While the override is `OVR_ACTIVE`, force that Lx's watchdog
  to trip (see TC-FAULT-17).
- **Expected Result**: `lx_fsm_report_watchdog_trip()` sees
  `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE` -> calls
  `lx_fsm_terminate_override_locked(fsm)` (returning `override_substate`
  to `OVR_NONE`, `override_remaining_ms = 0`) BEFORE setting
  `fsm->supervisory = SUPERVISORY_FAULT_SAFE`. In the next STATUS/
  HEARTBEAT sent to Central, the `OVERRIDE` column must switch to 0 and
  the `SUPERVISORY` column shows `FAULT_SAFE` - an override never
  "auto-resumes" after a fault clear (per the "never automatically
  resumes" principle).
  **If the watchdog cannot be tripped via runtime**: verify by code
  review lines 470-475 of `lx_fsm.c` (`lx_fsm_report_watchdog_trip()`)
  calling `lx_fsm_terminate_override_locked()` when
  `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE`, clearly noting
  "verified by code review only".

### TC-FAULT-14: FAULT_SAFE does not affect the reply to `MSG_CROSSING_STATUS` (Lx only observes, never blocks)
- **Type**: Edge case / mild Negative
- **Related**: RC-02, SC-03A
- **Environment**: N/A (code review; runtime would require both tripping
  the Lx watchdog and having a real RLx send crossing status - a
  complex combination, not required)
- **Steps**: Read `lx_fsm_on_crossing_status()` in `lx_fsm.c`.
- **Expected Result**: The function always returns `RESULT_ACK` regardless
  of `fsm->supervisory` (including `FAULT_SAFE`) - exactly per the
  comment "RC-02: Lx only ever observes crossing status, it never
  rejects it" - but while `FAULT_SAFE`, that update does NOT change
  `fsm->supervisory` (does not exit FAULT_SAFE, does not enter
  RAILWAY_PREEMPTION). This is correct by-design behavior, not a bug -
  recorded via code review.

### TC-FAULT-14b: REQUEST_FAULT_CLEAR brings Lx out of FAULT_SAFE into NORMAL_OPERATION (fixed - no longer a known gap)
- **Type**: Positive
- **Related**: SC-03A, `lx_fsm_on_request_fault_clear()` (`lx_fsm.c`)
- **Environment**: (B), depends on TC-FAULT-17 (the debugger path —
  TC-FAULT-16 is the kill -STOP counter-proof case, not used here) to
  trip the Lx watchdog first.
  **There is no code-review fallback for this case** (unlike
  TC-FAULT-14c) — so **record Skip, not Pass**, until there is a way to
  really trip the Lx watchdog (a suitable debugger, or `kill -STOP` once
  the contradiction with TC-FAULT-16 is resolved).
- **Setup**: L1 in `SUPERVISORY_FAULT_SAFE` (watchdog tripped via Group
  5), and no adjacent RLx currently pre-empting
  (`last_crossing_state == CROSSING_OPEN`, e.g. never received a
  non-OPEN `CROSSING_STATUS`, or RL1 already reported `OPEN` again
  before the trip).
- **Steps**: At C1: `f` -> node type `0` (Lx) -> Lx number `1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 1
  -> ACK`. `fsm->faults` back to `FAULT_NONE`, L1's SUPERVISORY changes
  `FAULT_SAFE -> NORMAL_OPERATION`. There is no NACK branch for this verb
  on the Lx side (unlike RLx) - it always ACKs, even called again after
  the fault is already cleared (idempotent).

### TC-FAULT-14c: REQUEST_FAULT_CLEAR while the adjacent crossing is still closed - resumes RAILWAY_PREEMPTION, not NORMAL_OPERATION (re-audit fix, safety-relevant)
- **Type**: Positive (safety-relevant regression case)
- **Related**: SC-03A, CC-02, `last_crossing_state` (`lx_fsm.h`) - before
  this patch, `lx_fsm_on_request_fault_clear()` always unconditionally
  resumed `NORMAL_OPERATION`, potentially allowing green toward a
  crossing that is still closed if the fault occurred (or was still
  active) while railway pre-emption was suppressing that intersection.
- **Environment**: (B), needs both an adjacent `rlx_main` and the ability
  to trip the Lx watchdog (depends on Group 5).
- **Setup**: Bring L1 into real `RAILWAY_PREEMPTION` (RL1 adjacent to L1
  in WARNING/CLOSED, sending `CROSSING_STATUS` other than
  `CROSSING_OPEN`), then trip the Lx watchdog while still pre-empting
  (SUPERVISORY transitions directly `RAILWAY_PREEMPTION -> FAULT_SAFE`;
  `lx_fsm_on_crossing_status()` still updates `fsm->last_crossing_state`
  unconditionally even while `FAULT_SAFE` - see TC-FAULT-14 above).
- **Steps**: At C1: `f` -> `0` -> `1`, **before** RL1 manages to report
  `OPEN` again.
- **Expected Result**: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`, but L1's
  SUPERVISORY afterward must be `RAILWAY_PREEMPTION`, **not**
  `NORMAL_OPERATION` - CONNECTOR_GREEN (crossing direction) remains
  suppressed until L1 actually receives `CROSSING_STATUS(OPEN)` from RL1.
  **If the watchdog cannot be tripped via runtime**: verify by code
  review `lx_fsm_on_request_fault_clear()` in `lx_fsm.c` (the branch
  `fsm->last_crossing_state != CROSSING_OPEN ? SUPERVISORY_RAILWAY_
  PREEMPTION : SUPERVISORY_NORMAL_OPERATION`), clearly noting "verified
  by code review only".

---

## Group 5 - Watchdog trip (PA-10)

This is the hardest group to test at runtime in the entire document,
because the watchdog is by nature a dead-man's-switch: to trigger it for
real, the **main tick-handling thread** (server thread) of the process
must stop responding while the **separate watchdog thread** (an
independent thread in the same process) keeps running `sleep()`/counter
comparisons. These two threads run in the SAME process, so any way of
"freezing the whole process" (including `kill -STOP`) also stops the
watchdog thread, so it never gets a chance to detect anything.

### TC-FAULT-15: Confirm watchdog logic by code review (always feasible, used as baseline)
- **Type**: Positive - code review, not runtime
- **Related**: PA-10
- **Environment**: N/A (reading code)
- **Steps**: Read `lx_watchdog_thread()` (`lx_watchdog.c`) and
  `rlx_watchdog_thread()` (`rlx_watchdog.c`).
- **Expected Result**:
  - Both run a `for(;;) { sleep(N); compare counter; }` loop with N = 2 s
    (Lx) / 3 s (RLx), independently, on their own thread (not sharing a
    thread with server/client - matching the comment "PA-10 dead man's
    switch thread ... alongside the server, client, and sensor
    threads").
  - When the counter is unchanged between 2 consecutive checks, calls
    `lx_fsm_report_watchdog_trip(args->fsm)` /
    `rlx_fsm_report_watchdog_trip(args->fsm)` - these functions take the
    lock themselves and set the FAULT state directly from the watchdog
    thread, **independent of whether the server thread is still alive**
    (exactly per the comment in `lx_fsm.c`: "this function is called
    FROM the watchdog thread itself ... independent of whether the
    server thread ever runs another event again").
  - This is the only test case in Group 5 that **always PASSes, no QNX
    machine needed** - used as the minimum mandatory evidence before
    attempting the runtime tests below.

### TC-FAULT-16: Counter-proof - `kill -STOP`ing the whole process does NOT trigger the watchdog
- **Type**: Negative (deliberately disproving a seemingly reasonable
  approach, so the team doesn't waste time retrying it)
- **Related**: PA-10
- **Environment**: (A), a real QNX machine, need to know the PID of
  `rlx_main`/`lx_main` (e.g. via `pidin` on QNX)
- **Steps**:
  1. Run `rlx_main 1`, record its PID (`pidin | grep rlx_main`).
  2. `kill -STOP <pid>`, wait 10 s (much longer than the 3 s check
     interval), then `kill -CONT <pid>`.
- **Expected Result**: **NO** fault is raised, no
  "WATCHDOG - no tick activity..." log line. Reason (recorded to
  explain, not because the test "failed"): per POSIX semantics,
  `SIGSTOP`/`SIGCONT` act on the whole process (every thread), not a
  single thread. The watchdog thread is stopped at the same time, so
  when both threads wake up together, the `tick_counter` still appears
  "just" incremented at the value the watchdog expects (since the time
  elapsed during STOP is not perceived by any thread as a "gap").
  Conclusion: **`kill -STOP` is not a valid way to test PA-10** in this
  single-process multi-thread architecture - a way to block only the
  server thread is needed (see TC-FAULT-17/18).
- **Note**: `docs/test-plan/02-state-machine-transition.md` (TC-SC01A-3
  and similar cases, e.g. around the line describing `kill -STOP <pid
  lx_main 1>` to bring L1 into FAULT_SAFE) asserts the **opposite
  conclusion** for this exact same mechanism (`lx_watchdog.c`/
  `rlx_watchdog.c`'s dedicated watchdog thread per PA-10) - that
  `kill -STOP` then `kill -CONT` WILL cause `lx_watchdog_thread` to
  detect that `phase_tick_counter` is unchanged and trip the fault. This
  is a real, unresolved contradiction between the two test-plan files,
  depending on the actual SIGSTOP/SIGCONT scheduling behavior of QNX
  (which cannot be determined by reading source code alone - both lines
  of reasoning rely on inference about OS behavior, not on a code path
  that measures this directly). **Neither side is concluded correct
  here** - once a real QNX machine is available, the `kill -STOP`/
  `kill -CONT` experiment described in both files needs to actually be
  run and the observed result recorded, then both documents updated to
  match that empirical evidence (instead of just trusting either file's
  existing reasoning). **This very case must be recorded as Skip in the
  summary report, not Pass** — the "NO fault" result above is only a
  theoretical inference about SIGSTOP/SIGCONT made by the team that
  wrote this case, not an actual observed result on a real QNX machine;
  the tools needed (`kill -STOP`, `pidin`) are fully available, the issue
  is that this decisive experiment has not actually been run and
  recorded in the latest run.

### TC-FAULT-17: Best-effort - force the Lx watchdog to trip using a debugger (blocking only the server thread)
- **Type**: Positive - best effort, tool-dependent, may not reproduce
  100% on every configuration
- **Related**: PA-10
- **Environment**: (A) or (B), needs gdb/QNX Momentics debugger with
  per-thread control support (non-stop mode or equivalent) attached to
  `lx_main`
- **Setup**: Run `lx_main 1` (L1), built with debug symbols.
- **Steps**:
  1. Attach the debugger to the `lx_main` process.
  2. Set a breakpoint inside the server thread's main pulse-handling
     function (e.g. the start of `lx_fsm_on_phase_timer()` in
     `lx_fsm.c`, or inside `lx_main.c`'s `on_pulse()`/dispatch loop).
  3. When the breakpoint hits, **hold only the server thread**; if the
     debugger supports it, use "non-stop"/"scheduler-locking off for
     other threads" mode so the remaining threads (especially the thread
     running `lx_watchdog_thread()`) keep running normally.
  4. Keep it in that state for at least 4 s (2 check periods of 2 s)
     before `continue`/releasing the breakpoint.
- **Expected Result**: After resuming, `lx_main`'s stderr prints
  `Lx: WATCHDOG - no phase-timer activity for 2 s, reporting fault (PA-10)`
  and `lx_fsm_report_watchdog_trip()` is called ->
  `fsm->supervisory == SUPERVISORY_FAULT_SAFE`,
  `fsm->faults & FAULT_WATCHDOG_TRIP` != 0.
- **Honest note**: many gdb/IDE builds by default stop ALL threads when a
  breakpoint is hit (including the watchdog thread), exactly like the
  problem in TC-FAULT-16. If the debug tooling available on the team's
  QNX machine does NOT support holding 1 thread while others keep
  running, this test case **cannot be reliably performed at runtime** -
  in that case only record the result of TC-FAULT-15 (code review) as
  substitute evidence, without forcing an uncertain runtime result.

### TC-FAULT-18: Best-effort - force the RLx watchdog to trip using a debugger (similar to TC-FAULT-17, 3 s threshold)
- **Type**: Positive - best effort, same tooling limits as TC-FAULT-17
- **Related**: PA-10, RC-10 (gate must be closed on fault)
- **Environment**: (A) or (B), same debugger requirement as TC-FAULT-17
- **Steps**: Same as TC-FAULT-17 but attach to `rlx_main`, set the
  breakpoint in `rlx_fsm_on_tick()`, hold for at least 6 s (2 periods of
  3 s), only stopping the server thread, leaving
  `rlx_watchdog_thread()` running.
- **Expected Result**: stderr prints
  `RLx: WATCHDOG - no tick activity for 3 s, reporting fault (PA-10)`,
  `rlx_fsm_report_watchdog_trip()` calls `enter_fault(FAULT_WATCHDOG_TRIP)`
  (since `fsm->state != RLX_FAULT` beforehand) -> gate is commanded
  closed immediately (`rlx_gate_command_close()`) even though it may have
  previously been `RLX_OPEN` with no motion running at all - this is an
  important difference from TC-FAULT-04 (where the fault occurs mid-way
  through an ongoing OPENING motion): here the fault occurs while the
  crossing is fully at rest in `RLX_OPEN`, proving `enter_fault()`
  proactively closes the gate rather than merely "correcting" an
  in-progress motion.
- **Honest note**: same caveat about debug tooling as TC-FAULT-17. If not
  feasible, use TC-FAULT-15 as substitute evidence.

---

## Group 6 - Stuck-sensor (known, accepted limitation - not a bug)

Per the comment in the source code itself, detecting a sensor "stuck
active" (a pedestrian button held continuously, a stuck vehicle sensor, a
stuck train sensor) **has not been implemented** in the current PoC build
- this is a limitation the team accepted beforehand (documented future
work), not a bug to fix. The purpose of the test cases below is to
**confirm current behavior matches the documented description** (i.e.
"not detected" as already known), to avoid mistaking "not detected" for a
new bug during full-system testing.

### TC-FAULT-19: A "stuck" pedestrian button (held/pressed continuously) does not set FAULT_PED_BUTTON_STUCK
- **Type**: Negative (confirming an accepted limitation)
- **Related**: PA-03
- **Environment**: (A)
- **Setup**: `lx_main 1`.
- **Steps**: Press key `1` (pedestrian button, side 0) repeatedly over a
  long period (e.g. every 1 s for 2 minutes, simulating a button
  physically stuck/held continuously).
- **Expected Result**: `fsm->ped_latched[0]` is still set/held correctly
  per the normal coalescing logic of
  `lx_fsm_latch_pedestrian_request()` (no crash, no infinite loop), BUT
  `fsm->faults` **never** gets the `FAULT_PED_BUTTON_STUCK` bit set no
  matter how long it's held - exactly per the comment "there is no
  'stuck active beyond a diagnostic timeout' detection here, so
  FAULT_PED_BUTTON_STUCK ... is never set by this file" in `lx_fsm.c`.
  This is the **expected** (pass) result, not a bug to report.

### TC-FAULT-20: A "stuck" vehicle sensor (demand held continuously) does not set FAULT_VEHICLE_SENSOR_STUCK
- **Type**: Negative (confirming an accepted limitation)
- **Related**: PA-06
- **Environment**: (A)
- **Steps**: Press `a` (arterial vehicle present) and never press `A`
  (clear) over a long period (several minutes, spanning multiple phase
  cycles).
- **Expected Result**: `fsm->arterial_vehicle_demand` stays at 1
  (correctly simulating a genuinely stuck sensor), the system keeps
  operating normally (in `MODE_OFF_PEAK_SENSOR`, the arterial lane will
  continuously be considered to have demand, possibly extending the green
  session longer than reality but never beyond `LX_MAX_GREEN_MS`), but
  `fsm->faults` never gets the `FAULT_VEHICLE_SENSOR_STUCK` bit - as
  currently designed (no mechanism to measure "how long has this demand
  been active"). Recorded as an **accepted limitation**, not a bug.

### TC-FAULT-21: A "stuck" train sensor (continuous TRAIN_APPROACHING) does not set FAULT_TRAIN_SENSOR_STUCK
- **Type**: Negative (confirming an accepted limitation)
- **Related**: RC-11
- **Environment**: (A)
- **Steps**: While RL1 is in `RLX_WARNING`, press `0` repeatedly multiple
  times before the full 5 s (`RLX_WARNING_TO_CLOSING_MS`) elapses,
  simulating a stuck train-approaching sensor (continuously reasserting
  "train present" even though it may no longer be true).
- **Expected Result**: Per `rlx_fsm_simulate_train_approaching()`, the
  `RLX_WARNING` case only calls `register_window()` again (self-loop),
  without resetting the gate-close timer and without setting any fault.
  Per the comment in `rlx_fsm_on_tick()`'s `RLX_WARNING` case: the
  constant `RLX_WARNING_DIAGNOSTIC_TIMEOUT_MS` (60 s) exists in
  `rlx_timer.h` but is **RESERVED, never referenced anywhere** - real
  "stuck active" logic would need a continuous sensor line, which
  discrete keypress events cannot simulate. So `fsm->faults` will
  **never** get the `FAULT_TRAIN_SENSOR_STUCK` bit set in the current
  build, no matter how many times or how long `0` is pressed. This is a
  limitation the development team itself has already documented in the
  code (not a new finding), verified by both code review and runtime
  observation reaching the same conclusion.

---

## Summary of runtime-testable scope on a real QNX machine

| Group | Runtime-testable (no debugger needed) | Needs debugger | Code review only |
|---|---|---|---|
| 1. RC-06 gate confirm | TC-01, TC-02, TC-03 | - | - |
| 2. Fault forces gate closed | TC-04 | - | - |
| 3. REQUEST_FAULT_CLEAR | TC-05, TC-06, TC-08, TC-09, TC-22 | TC-07, TC-10 (no longer the only way, see TC-22) | - |
| 4. FAULT_SAFE at Lx | TC-14b (depends on Group 5 to trip first) | TC-12, TC-13, TC-14c (depend on Group 5) | TC-11, TC-14 |
| 5. Watchdog trip | (counter-proof) TC-16 | TC-17, TC-18 | TC-15 |
| 6. Stuck-sensor | TC-19, TC-20, TC-21 | - | - |

24 test cases (TC-FAULT-14b/14c added after `MSG_REQUEST_FAULT_CLEAR` was
wired up for Lx; TC-FAULT-22 added after confirming key `r` in
`rlx_sensor.c` unlocks RLx's real REQUEST_FAULT_CLEAR ACK branch via
keyboard, no debugger needed), most of which (15/24) run entirely via
keyboard on a real QNX machine with no extra tooling; the remaining test
cases need a debugger (with caveats noted if infeasible, and for
TC-07/TC-10 the debugger is no longer the only way); 2 test cases are
pure code-review by architectural necessity (Lx has no fault-trigger key)
that rules out runtime; 1 test case (TC-16) is a deliberate counter-proof
- **note the unresolved contradiction with TC-SC01A-3 of
`02-state-machine-transition.md`**, see the Note in TC-FAULT-16.
