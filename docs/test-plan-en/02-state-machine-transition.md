# Test Plan 02 — State Machine Transitions

This document lists test cases for every transition in `STATE_CHARTS.md`
(SC-01A/B/C, SC-02, SC-03A/B, SC-04A/B, SC-05), cross-checked directly
against the actual source code at the time of writing:

- `app/intersection/src/lx_fsm.c`, `app/intersection/includes/lx_timer.h`
- `app/intersection/src/lx_sensor.c`, `app/intersection/src/lx_signal.c`
- `app/railway/src/rlx_fsm.c`, `app/railway/includes/rlx_timer.h`
- `app/railway/src/rlx_sensor.c`, `app/railway/src/rlx_gate.c`, `app/railway/src/rlx_signal.c`
- `app/central/src/c_watchdog_mon.c`, `app/central/src/c_operator.c`,
  `app/central/src/c_hmi.c`, `app/central/src/c_server.c`, `app/central/src/c_logger.c`
- `app/shared/includes/sys_types.h`

Every timing constant, key binding, and log field name in this document is
taken directly from these files — nothing is guessed. Where the code has a
known limitation/gap (no way to trigger the transition through the existing
interface), the test case is explicitly marked "Known gap" rather than
inventing a trigger path that doesn't exist.

## 0. General Conventions

### 0.1 Three Test Environments A/B/C

| Symbol | Description | When to use |
|---|---|---|
| **(A) Single node** | Only **one** process (`lx_main N` or `rlx_main N`) runs on one QNX machine, no `c_main`. Fully controlled via that process's own sensor keyboard (`lx_sensor.c` / `rlx_sensor.c`). | Testing internal SC-01, SC-02, SC-04 transitions that don't need Central and don't need Lx/RLx communication. |
| **(B) Multiple nodes, same QNX machine** | Multiple processes (`c_main`, `lx_main 1`, `rlx_main 1`, …) run on the **same** QNX target (multiple windows/consoles on one machine). `TRAFFIC_NODE_MAP` does not need to be exported (defaults to "same node"). | Testing override (SC-03B) — `c_main` is required since only `c_operator.c` can send `REQUEST_OVERRIDE`/`RENEW_OVERRIDE`/`CANCEL_OVERRIDE`. Also SC-03A's Lx–RLx interaction (RAILWAY_PREEMPTION), and SC-05. |
| **(C) Multiple machines/VMs over a real network** | Each role runs on a separate physical VM/PC, connected via Qnet, with `TRAFFIC_NODE_MAP` exported per `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` (Case 1/2/3). | Repeat the most critical (B) test cases (especially SC-03A regression #1, SC-05) on a real network topology before final acceptance, since real Qnet latency can expose race conditions invisible on same-node runs. |

Every test case below states the minimum required environment on the
**Environment** line. A test marked (B) can always be repeated on (C) if
the team has enough machines — recommended for tests labeled "Critical
regression."

### 0.2 Build & Process Startup

```
make        # build/bin/c_main, build/bin/lx_main, build/bin/rlx_main
/tmp/c_main            # Central, no parameters
/tmp/lx_main <1-6>     # select L1..L6
/tmp/rlx_main <1-3>    # select RL1..RL3
```

Recommended startup order (same as `QNX_DEPLOYMENT_RUN_GUIDE.md`): `c_main`
first, then `rlx_main`, then `lx_main`. For environment (C), export
`TRAFFIC_NODE_MAP` in **every** shell before running any binary, per
section 2.2 of that guide. The RLx–Lx adjacency map is fixed in code
(`app/railway/src/rlx_comm.c`, array `ADJACENCY`):

| RLx | Adjacent to |
|---|---|
| RL1 | L1, L2 |
| RL2 | L3, L4 |
| RL3 | L5, L6 |

### 0.3 Keyboard Shortcut Table

**`lx_sensor.c` (keyboard for each `lx_main` process)**

| Key | Effect |
|---|---|
| `a` / `A` | Vehicle present / departed on arterial approach |
| `c` / `C` | Vehicle present / departed on connector approach |
| `1`/`2`/`3`/`4` | Press pedestrian button side 0/1/2/3 |
| `w` / `W` | Toggle queue-warning on/off (CC-01) |

**`rlx_sensor.c` (keyboard for each `rlx_main` process)**

| Key | Effect |
|---|---|
| `0` / `1` | TRAIN_APPROACHING direction 0 / direction 1 |
| `x` | Force the **next** gate close/open motion to never confirm (demo RC-06 fault) |
| `f` | Demo-only: directly call `rlx_fsm_on_fault_clear()` locally (bypasses the real `MSG_REQUEST_FAULT_CLEAR` IPC path from Central) |

**`c_operator.c` (console of `c_main`, only exists while Central is running)**

| Key | Effect |
|---|---|
| `m` | `SET_MODE` for an Lx (enter Lx number 1-6, mode 0=PEAK_FIXED/1=OFF_PEAK_SENSOR) |
| `t` | Broadcast `SET_TIMING_PROFILE` to chain R1 or R2 |
| `o` | `REQUEST_OVERRIDE` (enter Lx, `target_movement` 0=arterial/1=connector, `duration_ms`) |
| `r` | `RENEW_OVERRIDE` (enter Lx, `extend_duration_ms`, 0 = keep current duration) |
| `c` | `CANCEL_OVERRIDE` (enter Lx) |
| `f` | `REQUEST_FAULT_CLEAR` (choose node type 0=Lx 1-6 or 1=RLx 1-3 — now also targets Lx, see `lx_fsm_on_request_fault_clear()`/TC-SC03A-6 Part 2) |

### 0.4 How to Read Logs

- Each `lx_main`/`rlx_main` prints directly to its own stdout (no
  timestamp) — e.g. `Lx 1: SIGNAL -> ARTERIAL GREEN`,
  `RLx: commanding gates DOWN (simulated motion, 3000 ms)`.
- `c_main` prints to stdout **and** writes to `central_log.txt` (same
  directory as `c_main` is run from) in the format
  `[YYYY-MM-DD HH:MM:SS] <content>` (`c_logger_log()`).
- `c_main`'s network status table (`c_hmi_render()`, columns
  `ID ROLE MODE PHASE CROSSING_STATE SUPERVISORY FAULTS SENSOR OVERRIDE
  AVAILABILITY`) auto-refreshes **every 1 second** (driven by
  `IPC_PULSE_HEARTBEAT_TICK` at 1 Hz) — no key press needed. Those
  columns are raw integers; decode using the table below.

**C1 display code table**

| Field | Values / meaning |
|---|---|
| MODE | `0`=PEAK_FIXED, `1`=OFF_PEAK_SENSOR |
| PHASE | `0`=ARTERIAL_GREEN, `1`=ARTERIAL_YELLOW, `2`=ALL_RED_A_TO_B, `3`=CONNECTOR_GREEN, `4`=CONNECTOR_YELLOW, `5`=ALL_RED_B_TO_A |
| CROSSING_STATE | `0`=OPEN, `1`=WARNING, `2`=CLOSED, `3`=FAULT |
| SUPERVISORY | `0`=FAULT_SAFE, `1`=RAILWAY_PREEMPTION, `2`=CENTRAL_OVERRIDE, `3`=NORMAL_OPERATION |
| OVERRIDE | `1` if `override_substate==OVR_ACTIVE`, else `0` (this column cannot distinguish `OVR_PENDING_CLEARANCE` — check the `c_operator`/Lx console log instead) |
| AVAILABILITY | `AVAILABLE` / `UNAVAILABLE` (`marked_unavailable`, PA-07) |

### 0.5 Timing Constants Used Throughout This Document

From `app/intersection/includes/lx_timer.h`:

| Constant | Value |
|---|---|
| `LX_PEAK_ARTERIAL_GREEN_MS` | 48000 ms |
| `LX_PEAK_CONNECTOR_GREEN_MS` | 30000 ms |
| `LX_YELLOW_MS` | 4000 ms |
| `LX_ALL_RED_MS` | 2000 ms |
| `LX_MIN_GREEN_MS` | 8000 ms |
| `LX_MAX_GREEN_MS` | 40000 ms |
| `LX_EXTENSION_MS` | 4000 ms |
| `LX_OVERRIDE_DURATION_CAP_MS` | 300000 ms (5 minutes, PA-11) |
| `LX_PHASE_TICK_MS` | 100 ms |
| `LX_WALK_MS` | 6000 ms |
| `LX_FLASHING_DONT_WALK_MS` | 4000 ms |
| `LX_DRAIN_MAX_EXTENSION_MS` | 60000 ms |
| `LX_CYCLE_LENGTH_MS` | 90000 ms (48+4+2+30+4+2) |

From `app/railway/includes/rlx_timer.h`:

| Constant | Value |
|---|---|
| `RLX_WARNING_TO_CLOSING_MS` | 5000 ms |
| `RLX_CLOSING_DEADLINE_MS` | 15000 ms |
| `RLX_EXPECTED_ARRIVAL_MS` | 20000 ms |
| `RLX_OCCUPANCY_WINDOW_MS` | 20000 ms |
| `RLX_OPENING_DEADLINE_MS` | 15000 ms |

From `app/railway/includes/rlx_gate.h`: `RLX_GATE_MOTION_MS` = 3000 ms
(simulated gate motion time), and the railway tick = 1000 ms/cycle
(`rlx_fsm_on_tick()` is called from `IPC_PULSE_RAILWAY_WARNING` every 1 s).

---

## 1. SC-01A — Vehicle-Signal Mode Selection Overview

### TC-SC01A-1: SET_MODE deferred to the correct safe phase boundary
- **Type**: Positive
- **Related**: SC-01A, `PEAK_FIXED -> mode_selection : pending mode change [safe phase boundary]`
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: L1 in `MODE_PEAK_FIXED` (default at startup — `lx_fsm_init()`), currently in `PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. On C1 console, press `m`, enter Lx = `1`, mode = `1` (OFF_PEAK_SENSOR) right as L1 enters ARTERIAL_GREEN (watch for `Lx 1: SIGNAL -> ARTERIAL GREEN` in the L1 terminal).
  2. Observe the reply to C1 (`c_comm.c` logs `RESULT_ACK_PENDING`, since the new mode differs from the current one — `lx_fsm_on_set_mode()`).
  3. Wait until L1 logs `ARTERIAL YELLOW` (48s after step 1), then `ALL RED (A to B)` (+4s) — mode must not change at either point.
  4. Wait 2 more seconds (~54s total from step 1) until L1 crosses the `ALL_RED_A_TO_B -> boundary_after_arterial` boundary.
- **Expected Result**: The mode only actually changes at the moment `ALL_RED_A_TO_B` finishes processing (in `lx_fsm_advance_phase_locked()`, `case PHASE_ALL_RED_A_TO_B`) — L1 enters `CONNECTOR_GREEN` as usual (since `boundary_after_arterial --> CONNECTOR_GREEN` doesn't depend on the new mode), but the **next** phase after that (back at ARTERIAL_GREEN) already runs on OFF_PEAK_SENSOR timing (no longer fixed at 48s/30s). C1's state table (`c_hmi_render`) flips L1's MODE from `0` to `1` right at the 54s mark, not earlier.

### TC-SC01A-2: Requesting the currently-running mode → immediate ACK, no deferral
- **Type**: Edge case
- **Related**: SC-01A, no-op branch in `lx_fsm_on_set_mode()` (request matches `fsm->mode`)
- **Environment**: (B)
- **Setup**: L1 currently `MODE_PEAK_FIXED`.
- **Steps**: Press `m`, Lx=`1`, mode=`0` (PEAK_FIXED — matches current mode).
- **Expected Result**: `c_operator` logs `RESULT_ACK` (not `ACK_PENDING`). `mode_change_pending` is not set (indirect test: sending another `SET_MODE` with mode=1 right after must work normally as a fresh request, not get confused with the old one). L1's current phase is uninterrupted — signal logs continue at their normal cadence.

### TC-SC01A-3: SET_MODE NACKed while in FAULT_SAFE
- **Type**: Negative
- **Related**: SC-01A / SC-03A overlap — `lx_fsm_check_fault_locked()` is called at the start of `lx_fsm_on_set_mode()`
- **Environment**: (B)
- **Setup**: Put L1 into FAULT_SAFE via a real watchdog trip: suspend the whole `lx_main 1` process with `kill -STOP <pid>` for more than 2 seconds (threshold `LX_WATCHDOG_CHECK_INTERVAL_S`=2s in `lx_watchdog.c`), then `kill -CONT <pid>` — on resume, `lx_watchdog_thread` detects `phase_tick_counter` hasn't changed and calls `lx_fsm_report_watchdog_trip()`.
- **Steps**:
  1. `kill -STOP <pid of lx_main 1>`, wait 3s, `kill -CONT <pid>`.
  2. Watch for logs `Lx: WATCHDOG - no phase-timer activity for 2 s, reporting fault (PA-10)` and `Lx 1: FAULT_SAFE - holding safe outputs (all-red/dark)`.
  3. On C1, press `m`, Lx=`1`, mode=`1`.
- **Expected Result**: `reply->result = RESULT_NACK`, `reply->reason = NACK_REASON_FAULT_ACTIVE`. L1's SUPERVISORY column on C1 = `0` (FAULT_SAFE) and unchanged.

---

## 2. SC-01B — PEAK_FIXED Phase Detail

### TC-SC01B-1: Fixed 90s cycle (48+4+2+30+4+2)
- **Type**: Positive
- **Related**: SC-01B, the full chain `ARTERIAL_GREEN -> ... -> ALL_RED_B_TO_A -> ARTERIAL_GREEN`
- **Environment**: (A) `lx_main 1` alone
- **Setup**: L1 defaults to `MODE_PEAK_FIXED`; no sensor keys needed (TL-01/TL-02: sensors don't affect PEAK_FIXED).
- **Steps**: Start a stopwatch at the first `Lx 1: SIGNAL -> ARTERIAL GREEN` log line, record relative timestamps of each subsequent log line up to the next `ARTERIAL GREEN` line.
- **Expected Result**: Order and delay between log lines:
  `ARTERIAL GREEN` (t=0) → `ARTERIAL YELLOW` (t≈48.0s) → `ALL RED (A to B)` (t≈52.0s) → `CONNECTOR GREEN` (t≈54.0s) → `CONNECTOR YELLOW` (t≈84.0s) → `ALL RED (B to A)` (t≈88.0s) → `ARTERIAL GREEN` (t≈90.0s). Tolerance ±1 tick (100ms) due to `LX_PHASE_TICK_MS` granularity.

### TC-SC01B-2: Sensors don't shorten/lengthen PEAK_FIXED
- **Type**: Negative (control test)
- **Related**: SC-01B, note "Ordinary approach-presence sensors ... never extend or shorten this fixed cycle (TL-01, TL-02)"
- **Environment**: (A)
- **Setup**: L1 in `MODE_PEAK_FIXED`, just entered `ARTERIAL_GREEN`.
- **Steps**:
  1. Right after entering ARTERIAL_GREEN, repeatedly press `a A a A c C` (toggle both approaches multiple times).
  2. Wait the full 48s.
- **Expected Result**: `ARTERIAL_GREEN -> ARTERIAL_YELLOW` still occurs at exactly t≈48.0s — not earlier despite `arterial_vehicle_demand`=0 (last key was `A`), not later despite `connector_vehicle_demand`=1 (last key was `c`). Confirms the `if (fsm->mode == MODE_PEAK_FIXED)` branch in `lx_fsm_on_phase_timer()` (case `PHASE_ARTERIAL_GREEN`) only compares `green_elapsed_ms` against `lx_timer_peak_green_duration_ms()`, never reading `arterial_vehicle_demand`/`connector_vehicle_demand`.

### TC-SC01B-3: Exact 48000ms boundary (not 100ms early, not late)
- **Type**: Edge case
- **Related**: SC-01B, `ARTERIAL_GREEN --> ARTERIAL_YELLOW : after 48 s`
- **Environment**: (A)
- **Setup**: L1 `MODE_PEAK_FIXED`, just entered ARTERIAL_GREEN.
- **Steps**: Since logs have no millisecond timestamp, use an external clock with ≤50ms precision; record when the `ARTERIAL YELLOW` log line appears relative to the `ARTERIAL GREEN` line.
- **Expected Result**: The difference is within [47.9s, 48.1s] — consistent with `lx_fsm_on_phase_timer()` accumulating exactly 100ms/tick and checking `green_elapsed_ms >= 48000` (tick 480 is the first tick that satisfies `>=`, i.e. exactly 48000ms, not 47900ms).

---

## 3. SC-01C — OFF_PEAK_SENSOR Phase Detail

### TC-SC01C-1: 8s minimum green is honored even with immediate opposing demand
- **Type**: Edge case (timing boundary)
- **Related**: SC-01C, guard `lx_timer_should_exit_green()` — "Never returns 1 (exit) below LX_MIN_GREEN_MS"
- **Environment**: (A)
- **Setup**: Switch L1 to OFF_PEAK_SENSOR (simplest (A)-only path doesn't exist since (A) has no C1 to send SET_MODE; instead use the minimal (B) environment: `c_main` + `lx_main 1`, press `m` Lx=1 mode=1 and wait for it to apply as in TC-SC01A-1). Right when L1 enters `ARTERIAL_GREEN` under OFF_PEAK_SENSOR (no `arterial_vehicle_demand`), immediately press `c` (connector demand=1) at t=0.
- **Steps**:
  1. t=0: press `c`.
  2. Watch logs every 4s (`LX_EXTENSION_MS`) — this is the guard's re-check cycle.
- **Expected Result**: `ARTERIAL_GREEN` does **not** exit at t=4000ms even though `own_demand=0` (arterial) and `other_demand=1` (connector) satisfy the logic condition — because `lx_timer_should_exit_green()` always returns `0` while `elapsed_ms < LX_MIN_GREEN_MS` (8000). `ARTERIAL_GREEN -> ARTERIAL_YELLOW` only occurs at tick t=8000ms (the first `% LX_EXTENSION_MS == 0` check where `elapsed_ms >= 8000`).

### TC-SC01C-2: No demand anywhere → rests on arterial green indefinitely (DP-04)
- **Type**: Positive
- **Related**: SC-01C, note "With no demand anywhere ... rests on arterial green indefinitely (DP-04)"
- **Environment**: (B) (use the mode-switch path from TC-SC01C-1)
- **Setup**: L1 in OFF_PEAK_SENSOR, just entered ARTERIAL_GREEN, no `a/c/1/2/3/4` keys pressed.
- **Steps**: Watch logs for at least 60s (past `LX_MAX_GREEN_MS`=40000ms).
- **Expected Result**: No other `SIGNAL ->` log line appears — L1 keeps showing `ARTERIAL GREEN` past the 40s mark, because the exit condition "`connector demand pending`" (`requires_other_demand=1`) is never true when neither approach has demand — even with `green >= 40s`, `lx_timer_should_exit_green()` (which reads `own_demand`/`other_demand`, with no unconditional-expiry branch for the arterial side when `requires_other_demand=1`) still returns `0`.

### TC-SC01C-3: DP-06 anti-starvation — arterial forced to yield at exactly 40s despite ongoing own demand
- **Type**: Edge case (two competing conditions: continuous arterial demand vs. 40s cap)
- **Related**: SC-01C, `ARTERIAL_GREEN --> ARTERIAL_YELLOW : [green >= 8 s and connector demand pending and (no arterial demand or green = 40 s)]`
- **Environment**: (B)
- **Setup**: L1 OFF_PEAK_SENSOR, in ARTERIAL_GREEN.
- **Steps**:
  1. t=0: press `a` (arterial demand=1) and `c` (connector demand=1), keep both on for the whole test (do not press `A`/`C`).
  2. Watch logs every 4s.
- **Expected Result**: `ARTERIAL_GREEN` self-extends silently (no new log, same phase) at 8s/12s/…/36s (since `own_demand=1` makes "no arterial demand" false, and `green=40s` isn't true yet, so the guard still returns 0). Exactly at t=40000ms, `green_elapsed_ms >= LX_MAX_GREEN_MS` makes `(no arterial demand or green = 40s)` true regardless of `own_demand` — `ARTERIAL_GREEN -> ARTERIAL_YELLOW` fires exactly at t≈40.0s, not earlier or later, even though `arterial_vehicle_demand` is still 1.

### TC-SC01C-4: Connector green also forced to exit at 40s despite continuous demand (no reverse anti-starvation)
- **Type**: Edge case
- **Related**: SC-01C, `CONNECTOR_GREEN --> CONNECTOR_YELLOW : [green >= 8 s and (no connector demand or green = 40 s)]`
- **Environment**: (B)
- **Setup**: L1 OFF_PEAK_SENSOR, already in `CONNECTOR_GREEN` (wait through one ARTERIAL cycle first, or let natural demand lead there).
- **Steps**: Keep `c` pressed (connector demand=1) continuously for the whole phase, never release.
- **Expected Result**: `CONNECTOR_GREEN -> CONNECTOR_YELLOW` still fires exactly at t≈40.0s from entering CONNECTOR_GREEN — unlike arterial, the `lx_timer_should_exit_green(..., other_demand=0u, requires_other_demand=0u)` call in `lx_fsm_on_phase_timer()`'s `PHASE_CONNECTOR_GREEN` case has no other condition beyond "no connector demand or green=40s", so the 40s cap always wins regardless of demand — confirming the `lx_timer.h` comment "arterial gets service again unconditionally next cycle regardless of its own demand."

---

## 4. SC-02 — Generic Pedestrian-Signal State Chart

### TC-SC02-1: Request latched during an incompatible phase, served when a compatible phase starts
- **Type**: Positive
- **Related**: SC-02, `DONT_WALK -> REQUEST_LATCHED -> WALK`
- **Environment**: (A) `lx_main 1`, `MODE_PEAK_FIXED`
- **Setup**: Wait for L1 to enter `CONNECTOR_GREEN` (sides 0/1 are compatible with ARTERIAL, not with CONNECTOR — `lx_fsm_arterial_ped_compatible_locked()`).
- **Steps**:
  1. During `CONNECTOR_GREEN`/`CONNECTOR_YELLOW`/`ALL_RED_B_TO_A`, press `1` (side 0).
  2. Confirm no `PED SIGNAL side 0 -> WALK` line appears immediately.
  3. Wait until L1 logs `SIGNAL -> ARTERIAL GREEN`.
- **Expected Result**: In the same tick L1 enters `PHASE_ARTERIAL_GREEN` (technically the next tick of `lx_fsm_ped_service_tick_locked()`, since it reads the current `fsm->phase`), the log prints `Lx 1: PED SIGNAL side 0 -> WALK`. The request is never discarded while waiting (per PA-02/note "not discarded").

### TC-SC02-2: Exact WALK (6000ms) and FLASHING_DONT_WALK (4000ms) durations, latch cleared at the right time
- **Type**: Positive
- **Related**: SC-02, `WALK --> FLASHING_DONT_WALK : after 6s`, `FLASHING_DONT_WALK --> DONT_WALK : after 4s`
- **Environment**: (A)
- **Setup**: Continue from TC-SC02-1, or press `1` right as ARTERIAL_GREEN starts.
- **Steps**: Time the gap between three log lines: `WALK` → `FLASHING_DONT_WALK` → `DONT_WALK`.
- **Expected Result**: `WALK` (t=0) → `FLASHING_DONT_WALK` (t≈6.0s) → `DONT_WALK` (t≈10.0s). After `DONT_WALK`, pressing `1` again must start a **brand-new** WALK/FDW sequence at the next ARTERIAL_GREEN (confirms `ped_latched[0]` was cleared to 0 in `lx_fsm_ped_service_tick_locked()`, `else { fsm->ped_latched[side] = 0; }` branch).

### TC-SC02-3: Coalescing repeated presses while still REQUEST_LATCHED
- **Type**: Positive
- **Related**: SC-02, `REQUEST_LATCHED --> REQUEST_LATCHED : PED_REQUEST(side) [request already pending]`
- **Environment**: (A)
- **Setup**: L1 in `CONNECTOR_GREEN` (side 0 not yet compatible).
- **Steps**: Press `1` five times, ~1s apart, while still in CONNECTOR_GREEN/YELLOW/ALL_RED.
- **Expected Result**: No extra log lines from the redundant presses (`lx_fsm_latch_pedestrian_request()` only sets `ped_latched[0]=1`; re-setting it is idempotent/harmless). When ARTERIAL_GREEN arrives, only **one** WALK/FDW sequence runs for side 0 — no repeats, no extended duration.

### TC-SC02-4: Coalescing a re-press during WALK (doesn't restart the timer)
- **Type**: Positive
- **Related**: SC-02, `WALK --> WALK : PED_REQUEST(side) / coalesce repeated request`
- **Environment**: (A)
- **Setup**: side 0 is currently in WALK (t≈2s since WALK began).
- **Steps**: Press `1` again at t≈2s (within WALK's 0–6s window).
- **Expected Result**: `FLASHING_DONT_WALK` still appears at t≈6.0s from the **original** WALK start (not pushed to t≈8s) — confirms a mid-sequence press doesn't reset `ped_phase_elapsed_ms`. (In code, pressing during WALK falls into the `ped_recall[side]=1` branch since `ped_serving_mask` already has bit 0 set — see TC-SC02-5 for the consequence of this flag.)

### TC-SC02-5 (Critical regression): `ped_recall` — a mid-sequence re-press never loses the request
- **Type**: Positive / Regression
- **Related**: SC-02 note "remains latched, not discarded (PA-02)" + the `ped_recall` mechanism in `lx_fsm_ped_service_tick_locked()`/`lx_fsm_latch_pedestrian_request()`
- **Environment**: (A)
- **Setup**: side 0 currently being served (WALK or FDW running, `ped_serving_mask` has bit 0 set).
- **Steps**:
  1. During FLASHING_DONT_WALK (e.g. t≈2s into the 4s FDW window), press `1` again.
  2. Wait for FDW to finish → `DONT_WALK` appears (t≈4s after that FDW window started).
  3. Continue watching L1 through the rest of CONNECTOR_GREEN/…/ALL_RED_B_TO_A, until ARTERIAL_GREEN returns.
- **Expected Result**: Right after `DONT_WALK` is printed, `ped_latched[0]` is **still 1** (not cleared), because the `if (fsm->ped_recall[side])` branch only clears `ped_recall[side]` back to 0 and deliberately leaves `ped_latched[side]` untouched. So at the very next ARTERIAL_GREEN, a **new** WALK/FDW sequence for side 0 must start automatically with no further button press — direct proof that a mid-sequence press is never lost.

### TC-SC02-6: Pressing a different side (not yet in the current compatible_mask) while another side of the same phase is being served
- **Type**: Edge case
- **Related**: SC-02, comment "A side that latches mid-sequence for the SAME phase is picked up the next time a sequence starts ... not folded into one already in progress"
- **Environment**: (A)
- **Setup**: side 0 is in WALK (side 1 was **not** pressed when WALK started, so the initial `compatible_mask` only has bit 0).
- **Steps**:
  1. At t≈2s into side 0's WALK, press `2` (side 1 — also compatible with ARTERIAL_GREEN but arrives late).
  2. Confirm no `PED SIGNAL side 1 -> WALK` appears immediately (`ped_serving_mask` currently only has bit 0, side 1 isn't folded in).
  3. Watch the full side-0 sequence (WALK→FDW→DONT_WALK) and the rest of CONNECTOR/ALL_RED, until the next ARTERIAL_GREEN begins.
- **Expected Result**: side 1 only starts WALK at the **next** ARTERIAL_GREEN (one 90s cycle later, under PEAK_FIXED) — never folded into or shortening side 0's already-running sequence, matching the "one WALK/FDW sequence instance at a time" design.

---

## 5. SC-03A — Intersection Supervisory Authority Overview

### TC-SC03A-1 (CRITICAL REGRESSION): RAILWAY_PREEMPTION doesn't freeze the whole intersection at ALL_RED — arterial keeps cycling normally, only connector is suppressed
- **Type**: Positive / Regression (this is the most significant fixed bug — a bare `break` in `lx_fsm_advance_phase_locked()` used to permanently stick `fsm->phase` at `PHASE_ALL_RED_A_TO_B`)
- **Related**: SC-03A, `NORMAL_OPERATION --> RAILWAY_PREEMPTION` and its internal behavior (CC-02)
- **Environment**: (B) or (C) — needs `rlx_main 1` (RL1, adjacent to L1/L2) + `lx_main 1`. `c_main` isn't required (crossing status goes directly RLx→Lx via `rlx_comm_broadcast_crossing_status_if_changed()`), but running it alongside makes it easier to watch SUPERVISORY via `c_hmi_render()`.
- **Setup**: L1 in `MODE_PEAK_FIXED`, cycling normally. RL1 in `RLX_OPEN`.
- **Steps**:
  1. On RL1's console, press `0` (TRAIN_APPROACHING direction 0) → RL1 enters `RLX_WARNING`, sending `MSG_CROSSING_STATUS(WARNING)` to L1 and L2 almost immediately (broadcast every tick, 1s, on change).
  2. On C1 (if running), watch L1's SUPERVISORY column flip from `3` (NORMAL_OPERATION) to `1` (RAILWAY_PREEMPTION) within ≤1s.
  3. **Watch L1's log continuously for at least 3 minutes** (enough for RL1 to naturally progress through WARNING(5s)→CLOSING(~3s)→CLOSED→wait 20s→TRAIN_PRESENT→wait 20s→OPENING(~3s), i.e. ~51s minimum with no second train — but since we deliberately don't let the crossing reopen here, just watch through the extended CLOSED period).
  4. Count how many times `Lx 1: SIGNAL -> ARTERIAL GREEN` appears while RAILWAY_PREEMPTION stays active (RL1 not yet OPEN again).
- **Expected Result**:
  - L1 must show **more than one** full `ARTERIAL_GREEN → ARTERIAL_YELLOW → ALL_RED_A_TO_B → ARTERIAL_GREEN` cycle while RAILWAY_PREEMPTION remains active — i.e. arterial keeps its normal 48+4+2=54s cadence, with no 90s connector phase interleaved.
  - `Lx 1: SIGNAL -> CONNECTOR GREEN` must **never** appear during active RAILWAY_PREEMPTION — every time `PHASE_ALL_RED_A_TO_B` is reached, it must loop straight back to `PHASE_ARTERIAL_GREEN` (the `if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) { fsm->phase = PHASE_ARTERIAL_GREEN; break; }` branch in `lx_fsm_advance_phase_locked()`).
  - Critically: L1 must **never** stay stuck at `ALL RED (A to B)` for more than 2s at a time — if the log shows L1 stuck there while RL1 is still WARNING/CLOSED, that's the old fixed bug recurring (a real regression).

### TC-SC03A-2: NORMAL_OPERATION → CENTRAL_OVERRIDE (accepting REQUEST_OVERRIDE)
- **Type**: Positive
- **Related**: SC-03A, `NORMAL_OPERATION --> CENTRAL_OVERRIDE : REQUEST_OVERRIDE(CLEAR_ROUTE, target, duration)`
- **Environment**: (B)
- **Setup**: L1 NORMAL_OPERATION, no pedestrian clearance running (`ped_clearance_active=0` — avoid pressing a pedestrian button beforehand).
- **Steps**: On C1 press `o`, Lx=`1`, target movement=`0` (arterial), duration_ms=`20000`.
- **Expected Result**: `c_operator` logs "REQUEST_OVERRIDE(...) submitted"; L1's SUPERVISORY on C1 switches to `2` (CENTRAL_OVERRIDE), OVERRIDE column=`1` within ≤1s.

### TC-SC03A-3: CENTRAL_OVERRIDE → RAILWAY_PREEMPTION when the adjacent crossing becomes active mid-override
- **Type**: Positive (contested path: override active + train approaching at the same time)
- **Related**: SC-03A, `CENTRAL_OVERRIDE --> RAILWAY_PREEMPTION : adjacent crossing becomes active / cancel or terminate override, suppress toward-crossing movement`
- **Environment**: (B), needs `rlx_main 1`, `lx_main 1`, and `c_main`.
- **Setup**: First do TC-SC03A-2 so L1 is in CENTRAL_OVERRIDE (target=connector, long duration e.g. 60000ms to allow time to operate).
- **Steps**:
  1. While override is still active (e.g. at second 10 of 60), press `0` on RL1.
  2. Watch L1's log: `Lx 1: override cleared/expired - running safe clearance sequence` (from `lx_signal_show_override_clearance()`, called inside `lx_fsm_terminate_override_locked()`) must appear **before or at the same time as** SUPERVISORY switching to RAILWAY_PREEMPTION.
- **Expected Result**: L1's SUPERVISORY on C1 goes `2` → `1` directly (no intermediate `3`). OVERRIDE returns to `0`. Afterward, behavior matches TC-SC03A-1 (arterial keeps cycling, connector suppressed) — the override does **not** auto-resume once the crossing reopens (per SC-03A's note "an override interrupted by a train is never automatically resumed afterward").

### TC-SC03A-4: RAILWAY_PREEMPTION self-loop — conflicting REQUEST_OVERRIDE gets NACKed
- **Type**: Negative
- **Related**: SC-03A, `RAILWAY_PREEMPTION --> RAILWAY_PREEMPTION : conflicting REQUEST_OVERRIDE(CLEAR_ROUTE) / NACK`
- **Environment**: (B)
- **Setup**: L1 in RAILWAY_PREEMPTION (RL1 in WARNING/CLOSING/CLOSED).
- **Steps**: On C1 press `o`, Lx=`1`, target=`1` (connector), duration=`10000`.
- **Expected Result**: `lx_fsm_on_request_override()` returns `RESULT_NACK`, `reply->reason = NACK_REASON_RAILWAY_CONFLICT` — C1's console shows the NACK (via `c_comm.c`'s reply logging). L1's SUPERVISORY stays `1`, unchanged.

### TC-SC03A-5: RAILWAY_PREEMPTION → NORMAL_OPERATION on crossing reopening, no queue-warning → no drain
- **Type**: Positive
- **Related**: SC-03A, `RAILWAY_PREEMPTION --> NORMAL_OPERATION : crossing reports OPEN and connector drain completes`; UC-05 alt 6.1 "no warning -> skip drain"
- **Environment**: (B) or (C)
- **Setup**: L1 in RAILWAY_PREEMPTION due to RL1. Ensure `w` was **not** pressed on L1 beforehand (`queue_warning_active=0`).
- **Steps**: Take no further action; let RL1 naturally cycle `WARNING→CLOSING→CLOSED→TRAIN_PRESENT→OPENING→OPEN` (~51s, no second train) → RL1 broadcasts `CROSSING_STATUS(OPEN)`.
- **Expected Result**: The moment L1 receives `CROSSING_OPEN`, SUPERVISORY switches `1 → 3` (NORMAL_OPERATION) inside `lx_fsm_on_crossing_status()`. `drain_pending` is **not** set (since `fsm->queue_warning_active==0`) — the next `CONNECTOR_GREEN` must run its normal duration (30s under PEAK_FIXED), no extension — confirmed by no extension log and `CONNECTOR_YELLOW` appearing exactly 30s after `CONNECTOR_GREEN`.

### TC-SC03A-6: NORMAL_OPERATION/CENTRAL_OVERRIDE → FAULT_SAFE via watchdog; and recovery via REQUEST_FAULT_CLEAR
- **Type**: Positive (entering FAULT_SAFE, and — now that `MSG_REQUEST_FAULT_CLEAR` is wired up for Lx — exiting it too, no longer a Known gap)
- **Related**: SC-03A, `NORMAL_OPERATION --> FAULT_SAFE`, `CENTRAL_OVERRIDE --> FAULT_SAFE`, `FAULT_SAFE --> NORMAL_OPERATION : verified repair and accepted local fault-clear request`, `lx_fsm_on_request_fault_clear()`
- **Environment**: (B)
- **Setup/Steps — Part 1 (from CENTRAL_OVERRIDE)**:
  1. Put L1 into CENTRAL_OVERRIDE (as in TC-SC03A-2).
  2. Suspend `lx_main 1` with `kill -STOP <pid>` for >2s, then `kill -CONT <pid>` to trigger a real watchdog trip (`lx_watchdog_thread`).
  3. Watch for `Lx: WATCHDOG - no phase-timer activity for 2 s, reporting fault (PA-10)`, then `Lx 1: override cleared/expired - running safe clearance sequence` (override is terminated **before** entering FAULT_SAFE — a compliance-audit fix in `lx_fsm_report_watchdog_trip()`/`lx_fsm_check_fault_locked()`), then `Lx 1: FAULT_SAFE - holding safe outputs (all-red/dark)`.
- **Expected Result Part 1**: L1's SUPERVISORY: `2 → 0` directly (skipping `3`), OVERRIDE back to `0`.
- **Steps — Part 2 (recovery via REQUEST_FAULT_CLEAR)**: **Updated (fixed, no longer a known gap)** — `MSG_REQUEST_FAULT_CLEAR` is now handled for Lx too by `lx_main.c`'s `on_request()` (calling `lx_fsm_on_request_fault_clear()`), and `c_operator.c`'s `f` key now asks for `node type` (0=Lx, 1=RLx) before the node number — enter `f` → `0` → `1` to target L1. On C1: `f` → node type `0` → Lx number `1`.
- **Expected Result Part 2**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`. `lx_fsm_on_request_fault_clear()` is unconditional/idempotent (no physical condition needs re-verifying, unlike RLx's `gates_confirmed_open()`): always ACKs, clears `fsm->faults`, and moves L1's SUPERVISORY out of `FAULT_SAFE`. Related re-audit safety fix (see `last_crossing_state` in `lx_fsm.h`): if `fsm->last_crossing_state != CROSSING_OPEN` at clear time (adjacent crossing still closed/occupied), SUPERVISORY must resume `RAILWAY_PREEMPTION` (`1`), **not** `NORMAL_OPERATION` (`3`) — verify this by repeating Part 1 while RL1 is WARNING/CLOSED (railway preemption active) before tripping the watchdog, then clearing the fault while the crossing is still not OPEN: after ACK, L1's SUPERVISORY must be `1`, not `3`, and CONNECTOR_GREEN must stay suppressed until RL1 actually reports `OPEN`.

### TC-SC03A-7 (Critical regression, CC-03): Drain phase granted exactly once per railway reopening
- **Type**: Positive / Regression
- **Related**: SC-03A (RAILWAY_PREEMPTION→NORMAL_OPERATION edge) combined with CC-03, implemented in `lx_fsm_on_crossing_status()` (sets `drain_pending`) and `lx_fsm_advance_phase_locked()`/`lx_fsm_on_phase_timer()` (consumes `drain_pending`, runs `drain_active`/`drain_extending`)
- **Environment**: (B) or (C)
- **Setup**: L1 `MODE_PEAK_FIXED`, RL1 adjacent to L1. Press `w` on L1 (`queue_warning_active=1`) **while** RL1 is WARNING/CLOSED (RAILWAY_PREEMPTION active on L1).
- **Steps**:
  1. With `queue_warning_active=1`, wait for RL1 to reopen (`CROSSING_OPEN`) → L1 receives it, `drain_pending=1` set in `lx_fsm_on_crossing_status()`.
  2. Watch the **first** `CONNECTOR_GREEN` afterward: it must run its full normal 30s (`LX_PEAK_CONNECTOR_GREEN_MS`), then — since `drain_active=1` — instead of going straight to `CONNECTOR_YELLOW`, extend in 4s increments (`LX_EXTENSION_MS`) **as long as** `queue_warning_active` stays 1.
  3. Do **not** press `W` (turn off queue warning) — let it run to the cap.
  4. Measure total extension time: it must stop exactly at `LX_DRAIN_MAX_EXTENSION_MS`=60000ms (15 increments of 4s), after which `CONNECTOR_GREEN -> CONNECTOR_YELLOW` occurs even though `queue_warning_active` is still 1 (the `drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS` branch wins regardless of warning state).
  5. After the drain ends, let L1 run through another full cycle and (optionally) a **second** CONNECTOR_GREEN with no new RAILWAY_PREEMPTION→NORMAL_OPERATION transition in between.
- **Expected Result**:
  - Total CONNECTOR_GREEN duration for the drain cycle = 30s (base) + up to 60s (drain) = up to 90s, growing in exact 4s increments.
  - The **next** CONNECTOR_GREEN after that (step 5, no new RAILWAY_PREEMPTION in between) must go back to a normal 30s — **no** further extension, since `drain_pending` is only set once per `RAILWAY_PREEMPTION -> NORMAL_OPERATION` edge (not re-set during ongoing NORMAL_OPERATION even if `queue_warning_active` is still 1) — this confirms the "exactly once per railway reopening" property.
  - Optional variant (time permitting): repeat the whole test but press `W` (turn off warning) mid-drain (e.g. after 12s of extension) — expect `CONNECTOR_GREEN -> CONNECTOR_YELLOW` at the very next 4s increment after turning it off (not waiting the full 60s), matching the `!fsm->queue_warning_active` branch.

---

## 6. SC-03B — Clear-Route Override Validation, Pending, and Renewal Detail

### TC-SC03B-1: override_validation → ACTIVE immediately (bounded, safe, no pedestrian clearance)
- **Type**: Positive
- **Related**: SC-03B, `override_validation --> ACTIVE : [bounded, safe, and no pedestrian clearance active] / ACK, apply at safe boundary`
- **Environment**: (B)
- **Setup**: L1 NORMAL_OPERATION, no `ped_clearance_active`.
- **Steps**: Press `o`, Lx=`1`, target=`0` (arterial), duration=`15000`.
- **Expected Result**: `lx_fsm_on_request_override()` returns `RESULT_ACK` (not `ACK_PENDING`) immediately; `override_substate` = `OVR_ACTIVE`; if L1 is currently in `PHASE_CONNECTOR_GREEN`, the phase timer keeps running to the nearest `ALL_RED_B_TO_A`/`ALL_RED_A_TO_B` boundary before forcing `PHASE_ARTERIAL_GREEN` (never cutting off a running phase — "apply at safe boundary").

### TC-SC03B-2: override_validation → OVERRIDE_PENDING during pedestrian clearance, then auto-activates when clearance ends
- **Type**: Positive (contested path: override request arrives exactly during ped clearance)
- **Related**: SC-03B, `override_validation --> OVERRIDE_PENDING` then `OVERRIDE_PENDING --> ACTIVE : pedestrian clearance completes [request remains safe and valid]`
- **Environment**: (B)
- **Setup**: Press a pedestrian button compatible with the current phase (e.g. `1` while L1 is ARTERIAL_GREEN) to have a WALK running (`ped_clearance_active=1`).
- **Steps**:
  1. During WALK/FDW (t≈2s into the 6s WALK), press `o`, Lx=`1`, target=`0`, duration=`20000`.
  2. Confirm the reply is `RESULT_ACK_PENDING` (not `ACK`/`NACK`).
  3. Wait for `DONT_WALK` to appear (marks `ped_clearance_active` returning to 0).
- **Expected Result**: On the tick right after `DONT_WALK` appears, `override_substate` switches `OVR_PENDING_CLEARANCE → OVR_ACTIVE` (a branch in `lx_fsm_on_phase_timer()`, running **after** `lx_fsm_ped_service_tick_locked()` in the same tick — a verifier-audit fix to call order). No second reply is sent to C1 (the initial ACK already covers it, per SC-03B's note "not repeated when the queued request later activates"). C1's OVERRIDE column goes `0 → 1`.

### TC-SC03B-3: override_validation → NACK on invalid duration
- **Type**: Negative
- **Related**: SC-03B, `override_validation --> [*] : [unsafe, unbounded, or conflicts with railway pre-emption] / NACK`
- **Environment**: (B)
- **Setup**: L1 NORMAL_OPERATION.
- **Steps**:
  1. Press `o`, Lx=`1`, target=`0`, duration=`0`.
  2. Press `o`, Lx=`1`, target=`0`, duration=`300001`.
- **Expected Result**: Both are rejected **at the Central layer** (`c_mode_eng_validate_override_request()` in `c_operator.c`, checking `duration_ms==0 || >300000` — exactly matching the condition `lx_fsm_on_request_override()` would apply if the request reached Lx) — C1's console prints "rejected by Central pre-check, reason=INVALID_DURATION", the request is **never** forwarded to L1 (`c_comm_send_request_override()` never runs). This is defense-in-depth: the identical limit also exists independently in `lx_fsm_on_request_override()` (`payload->duration_ms == 0 || > LX_OVERRIDE_DURATION_CAP_MS`), but through the real operator console, the Central layer always catches it first.

### TC-SC03B-4: OVERRIDE_PENDING discarded when it expires while still waiting (duration shorter than remaining ped clearance)
- **Type**: Edge case (two near-simultaneous conditions: countdown expiry vs. clearance still running)
- **Related**: SC-03B, `OVERRIDE_PENDING --> [*] : request no longer valid, cancelled, or expires before application / discard request`
- **Environment**: (B)
- **Setup**: Trigger the longest possible ped clearance sequence (WALK 6s + FDW 4s = 10s) right at the current phase — press a compatible pedestrian button right as ARTERIAL_GREEN begins.
- **Steps**:
  1. Right after WALK starts (t≈0.5s), send `o`, Lx=`1`, target=`0`, duration=`3000` (much shorter than the ~9.5s of ped sequence remaining).
  2. Confirm the reply is `RESULT_ACK_PENDING`.
  3. Track `override_remaining_ms` indirectly via logs: since `lx_fsm_on_phase_timer()` counts down `override_remaining_ms` during both `OVR_ACTIVE` **and** `OVR_PENDING_CLEARANCE` (a compliance-audit fix), the 3000ms countdown hits 0 at t≈3.0s — **before** `DONT_WALK` appears (t≈10.0s).
- **Expected Result**: At t≈3.0s, `lx_fsm_terminate_override_locked()` is called even while still `OVR_PENDING_CLEARANCE` — log `Lx 1: override cleared/expired - running safe clearance sequence` appears, `supervisory` returns to `NORMAL_OPERATION`, C1's OVERRIDE returns to `0` — **while the pedestrian WALK/FDW keeps running uninterrupted** (per the principle "pedestrian sequence itself is never truncated"). No second reply is sent to C1.

### TC-SC03B-5: renewal_validation — both valid/invalid branches, and NACK when there's no active override to renew
- **Type**: Positive + Negative (combined)
- **Related**: SC-03B, `renewal_validation --> ACTIVE : [bounded and safe] / ACK, restart override timer` and `[invalid, unsafe, or over limit] / NACK, retain current expiry`
- **Environment**: (B)
- **Setup — branch (a) valid**: L1 currently `OVR_ACTIVE` with `duration_ms=20000`, ~10s elapsed (`override_remaining_ms≈10000`).
- **Steps (a)**: Press `r`, Lx=`1`, extend_duration_ms=`30000`.
- **Expected Result (a)**: `RESULT_ACK`; both `override_duration_ms` and `override_remaining_ms` reset to `30000` (full restart, not additive) — the override now expires 30s from the **renewal time**, not from the original start.
- **Steps (b) — invalid branch**: Right after (a), press `r`, Lx=`1`, extend_duration_ms=`400000` (>300000).
- **Expected Result (b)**: `RESULT_NACK`, `NACK_REASON_INVALID_DURATION`; `override_remaining_ms` **stays** at the value counting down from (a) (not reset to 0 or altered) — per "retain current expiry".
- **Steps (c) — no override to renew**: On another Lx with no prior override (e.g. L2), press `r`, Lx=`2`, extend_duration_ms=`10000`.
- **Expected Result (c)**: `lx_fsm_on_renew_override()` checks `fsm->supervisory != SUPERVISORY_CENTRAL_OVERRIDE || fsm->override_substate != OVR_ACTIVE` → `RESULT_NACK`, `NACK_REASON_UNKNOWN_TARGET`. (Renewing an override still in `OVR_PENDING_CLEARANCE` must also NACK for the same reason — can additionally be tested by renewing during the TC-SC03B-2 scenario before clearance finishes.)

### TC-SC03B-6 (Critical regression): override actually forces the correct green direction, holds it for the full duration, then returns to the normal cycle through proper yellow/red
- **Type**: Positive / Regression
- **Related**: SC-03B "ACTIVE" note ("Bounded and auto-expiring") combined with SC-01B/C — implemented in the `SUPERVISORY_CENTRAL_OVERRIDE && OVR_ACTIVE` branches of `lx_fsm_on_phase_timer()` (cases `PHASE_ARTERIAL_GREEN`/`PHASE_CONNECTOR_GREEN`, holding green) and `lx_fsm_advance_phase_locked()` (forcing the correct `override_target_movement` at the `ALL_RED_A_TO_B`/`ALL_RED_B_TO_A` boundaries)
- **Environment**: (B)
- **Setup**: Wait until L1 is in `PHASE_CONNECTOR_GREEN` (the "wrong" direction relative to the override about to be sent).
- **Steps**:
  1. During `CONNECTOR_GREEN`, press `o`, Lx=`1`, target=`0` (**arterial** — opposite the current direction), duration=`15000`.
  2. Watch: `CONNECTOR_GREEN` must run its full normal course to `CONNECTOR_YELLOW` (4s) → `ALL_RED_B_TO_A` (2s) — the override must not cut off the currently displayed phase/clearance.
  3. At the end of `ALL_RED_B_TO_A`, observe the next phase.
  4. Time how long L1 continuously holds `ARTERIAL_GREEN`.
  5. 15s after the override was ACKed (not from when ARTERIAL_GREEN began — `override_remaining_ms` counts from ACK time), observe what happens.
- **Expected Result**:
  - Step 3: `lx_fsm_advance_phase_locked()`'s `PHASE_ALL_RED_B_TO_A` case always picks `PHASE_ARTERIAL_GREEN` — this is **always true** at this boundary whether or not an override is active (nothing distinguishing here), so the real test is at the `PHASE_ALL_RED_A_TO_B` boundary afterward.
  - `ARTERIAL_GREEN` does not self-exit at 48s as normal PEAK_FIXED would — the `if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && ... && override_target_movement == OVERRIDE_MOVEMENT_ARTERIAL) { break; }` branch skips the normal exit-check entirely, holding ARTERIAL_GREEN **until** `override_remaining_ms` hits 0.
  - Exactly 15s after ACK, log `Lx 1: override cleared/expired - running safe clearance sequence` appears, SUPERVISORY returns to `3`, and **immediately after**, the phase is still `ARTERIAL_GREEN` (lx_fsm doesn't force a phase change when an override ends — only supervisory changes) — it continues the normal PEAK_FIXED exit-check (reaching 48s total since ARTERIAL_GREEN began, **not** from when the override expired), then goes to `ARTERIAL_YELLOW → ALL_RED_A_TO_B → CONNECTOR_GREEN` as a normal cycle — i.e. always through proper yellow/red, never a direct jump.
  - Required additional variant (edge case): repeat the whole test but send two consecutive `REQUEST_OVERRIDE`s to the same L1 without cancelling/expiring the first — the second must be `RESULT_NACK`/`NACK_REASON_OUT_OF_RANGE` (the `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE` check at the top of `lx_fsm_on_request_override()` — a compliance-audit fix preventing a second override from silently overwriting the first one's target/duration).

---

## 7. SC-04A — Railway Crossing Approach and Closure

### TC-SC04A-1: OPEN → WARNING on train approach
- **Type**: Positive
- **Related**: SC-04A, `OPEN --> WARNING : TRAIN_APPROACHING(direction) / activate flashers, notify Lx and C1, register occupancy window`
- **Environment**: (A) `rlx_main 1` alone is enough to see internal logs; use (B) to confirm Lx/C1 notification.
- **Setup**: RL1 in `RLX_OPEN`.
- **Steps**: Press `0`.
- **Expected Result**: Log `RLx: flashers ON (train approaching, direction 0)`. In (B): L1 and L2 receive `MSG_CROSSING_STATUS(WARNING)`; if `c_main` is running, RL1's CROSSING_STATE on C1's table goes `0 → 1`.

### TC-SC04A-2: WARNING self-loop — second direction approaches while already WARNING
- **Type**: Positive
- **Related**: SC-04A, `WARNING --> WARNING : TRAIN_APPROACHING(other direction) / register additional occupancy window`
- **Environment**: (A)
- **Setup**: RL1 just entered WARNING via `0` (t=0).
- **Steps**: At t≈2s, press `1`.
- **Expected Result**: `register_window()` creates a second window (direction 1) **without** resetting `state_elapsed_ms` (the counter toward `RLX_WARNING_TO_CLOSING_MS` still runs from t=0, not t=2s) — `CLOSING` must still start exactly at t≈5.0s (not delayed to t≈7.0s).

### TC-SC04A-3: WARNING → CLOSING at the exact 5000ms boundary
- **Type**: Edge case
- **Related**: SC-04A, `WARNING --> CLOSING : after 5 s / command both gates down`
- **Environment**: (A)
- **Setup**: RL1 OPEN.
- **Steps**: Press `0`, time until log `RLx: commanding gates DOWN (simulated motion, 3000 ms)` appears.
- **Expected Result**: Difference ≈5.0s (±1 tick, 1000ms, since the railway tick cycle is 1s, unlike Lx's 100ms). *Known-gap note:* the transition `WARNING --> FAULT : approach input remains active beyond diagnostic timeout` (60s, `RLX_WARNING_DIAGNOSTIC_TIMEOUT_MS`) **cannot occur** in the current setup — a comment in `rlx_fsm_on_tick()` confirms this is dead code, since `RLX_WARNING_TO_CLOSING_MS`(5s) always fires first and resets `state_elapsed_ms` every time `enter_closing()` runs; the discrete simulated `TRAIN_APPROACHING` event also cannot represent a sensor "stuck continuously active." No positive test case can (or should) be written for this branch — confirming WARNING always goes to CLOSING at 5s, as above, is sufficient as a "negative control" for this FAULT branch.

### TC-SC04A-4: CLOSING → CLOSED when both gates confirm closed before deadline
- **Type**: Positive
- **Related**: SC-04A, `gate_confirmation --> CLOSED : [both gates confirmed CLOSED before deadline] / set train signal(s) for registered approaches to PROCEED`
- **Environment**: (A)
- **Setup**: RL1 OPEN, `x` not pressed (no demo fault armed).
- **Steps**: Press `0`, wait through WARNING(5s)→CLOSING.
- **Expected Result**: At t≈5+3=8.0s (5s warning + `RLX_GATE_MOTION_MS`=3000ms simulated), logs appear in order: `RLx: commanding gates DOWN...` (t≈5.0s) then `RLx: train signal PROCEED for direction 0 (gates confirmed closed)` (t≈8.0s) — well before the 15s deadline (`RLX_CLOSING_DEADLINE_MS`, counted from entering CLOSING, so the real deadline is t≈20.0s).

### TC-SC04A-5 (Regression): CLOSING → FAULT when gate confirmation is missing at deadline — fault forces a real gate close
- **Type**: Negative / Regression
- **Related**: SC-04A, `gate_confirmation --> FAULT : [confirmation missing or contradictory at deadline] / hold STOP and report fault`; `enter_fault()` calls `rlx_gate_command_close()` (audit fix — fault used to only latch a flag, without forcing a real gate close)
- **Environment**: (A)
- **Setup**: RL1 OPEN.
- **Steps**:
  1. Press `x` (arm demo fault for the next motion).
  2. Press `0` → enters WARNING → after 5s enters CLOSING, `rlx_gate_command_close()` runs (log `commanding gates DOWN`), but since the fault is armed, `rlx_gate_on_tick()` never sets `g_confirmed_closed=1` at 3000ms — instead logs `RLx: gate FAILED TO CONFIRM (simulated fault) ...`.
  3. Wait until exactly `RLX_CLOSING_DEADLINE_MS`=15000ms from entering CLOSING (t≈5+15=20.0s from pressing `0`).
- **Expected Result**: Exactly at t≈20.0s, `check_closing_or_reclosing_complete()` sees `state_elapsed_ms >= 15000` and `gates_confirmed_closed()==0` → calls `enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING)`. The log must show `RLx: commanding gates DOWN (simulated motion, 3000 ms)` printed **a second time** (since `enter_fault()` unconditionally calls `rlx_gate_command_close()` again — this is the exact regression to confirm: before the fix, fault didn't force a real gate-close command, potentially leaving the gate in limbo). Followed by `RLx: FAULT latched (fault bit 0x1) - holding STOP on all train signals, commanding gates DOWN`. C1's CROSSING_STATE (if running) = `3` (FAULT).

---

## 8. SC-04B — Railway Crossing Occupancy and Reopening

### TC-SC04B-1: CLOSED → TRAIN_PRESENT at expected arrival time, gates still confirmed closed
- **Type**: Positive
- **Related**: SC-04B, `CLOSED --> TRAIN_PRESENT : expected arrival time reached [gates remain confirmed CLOSED] / mark occupancy window active`
- **Environment**: (A)
- **Setup**: Repeat TC-SC04A-4 to get RL1 into `RLX_CLOSED` (t≈8.0s after pressing `0`).
- **Steps**: Take no further action, wait `RLX_EXPECTED_ARRIVAL_MS`=20000ms from entering CLOSED.
- **Expected Result**: There's no explicit log for this transition (`enter_train_present()` only sets internal state, prints nothing), but it can be inferred indirectly: the externally visible CROSSING_STATE (via `map_to_crossing_state()`) **stays** `CROSSING_CLOSED` (both `RLX_CLOSED` and `RLX_TRAIN_PRESENT` map to `CROSSING_CLOSED`) — verify via occupancy window: press `0` again right after the 20s mark (t≈28s) and confirm `RLx: train signal PROCEED for direction 0 ...` appears again immediately (the `RLX_TRAIN_PRESENT` branch in `rlx_fsm_simulate_train_approaching()` calls `register_window(..., RLX_OCCUPANCY_WINDOW_MS)` directly and prints PROCEED right away, same as the `RLX_CLOSED` branch — so to distinguish them reliably you need an extra wait interval and timing log).
  *Note*: this transition is hard to observe directly via logs — the team should consider adding a temporary debug line for this test if more conclusive evidence than timing inference is needed.

### TC-SC04B-2: TRAIN_PRESENT → OPENING only when ALL occupancy windows expire (not just one)
- **Type**: Edge case (two staggered occupancy windows)
- **Related**: SC-04B, `TRAIN_PRESENT --> OPENING : all occupancy windows expire`; RC-04 invariant in `rlx_fsm_on_tick()` (`if (fsm->active_window_count == 0)`)
- **Environment**: (A)
- **Setup**: Get RL1 into TRAIN_PRESENT with **one** direction-0 window running (per TC-SC04B-1, the direction-0 window starts counting 20s from entering TRAIN_PRESENT).
- **Steps**:
  1. Right at TRAIN_PRESENT (t=0 for this test), at t≈10s press `1` (register a second, direction-1 window — since already `RLX_TRAIN_PRESENT`, it's immediately assigned `remaining_ms=RLX_OCCUPANCY_WINDOW_MS`=20000ms, expiring at t≈30s).
  2. Watch at t≈20s (direction-0 window expires) — RL1 must **not** enter OPENING.
  3. Keep watching until t≈30s.
- **Expected Result**: At t≈20s, `active_window_count` drops from 2 to 1 (only the direction-0 window closes), state **stays** `RLX_TRAIN_PRESENT` — no `RLx: commanding gates UP...` log at this point. Only at t≈30s, when the direction-1 window also expires and `active_window_count==0`, does `enter_opening()` run → logs `RLx: all train signals -> STOP (crossing reopening)` then `RLx: commanding gates UP (simulated motion, 3000 ms)` appear exactly at t≈30s, not earlier.

### TC-SC04B-3: OPENING → RECLOSING when a new train approaches mid-open
- **Type**: Positive (two near-simultaneous conditions: gate mid-open + new train)
- **Related**: SC-04B, `OPENING --> RECLOSING : TRAIN_APPROACHING(direction) / stop opening, keep flashers active, register occupancy window, command gates down`
- **Environment**: (A)
- **Setup**: Get RL1 into `RLX_OPENING` (per TC-SC04B-2, gates start opening at t≈30s, needing 3000ms to confirm open — expected confirmation at t≈33s).
- **Steps**: At t≈31s (gate mid-open, not yet confirmed open), press `0`.
- **Expected Result**: Log `RLx: reclosing - aborting gate-open motion, flashers remain active` appears immediately; right after, `RLx: commanding gates DOWN (simulated motion, 3000 ms)` (calling `rlx_gate_command_close()` again, aborting the in-progress open motion). No `RLx: flashers OFF` appears in between (flashers must stay active throughout, per "keep flashers active"). About 3s later (t≈34s), `reclose_confirmation` → `RLx: train signal PROCEED for direction 0 ...` → state returns to CLOSED (mapping to CROSSING_CLOSED, same as after normal CLOSING).

### TC-SC04B-4: OPENING → FAULT when gates fail to confirm open in time, then recovery via FAULT → OPEN
- **Type**: Negative (entering FAULT) + Positive (exiting FAULT — a working recovery path, unlike Lx)
- **Related**: SC-04B, `OPENING --> FAULT : gates fail to confirm OPEN / hold last confirmed safe outputs and report fault`; `FAULT --> OPEN : verified repair and accepted local fault-clear request [crossing safe]`
- **Environment**: (A) is enough (using demo key `f`); repeat in (B) to test the real `MSG_REQUEST_FAULT_CLEAR` IPC path from C1 if needed.
- **Setup**: Get RL1 to right before entering OPENING (e.g. stop at the end of TC-SC04B-2, right before t≈30s).
- **Steps**:
  1. Before the last occupancy window expires, press `x` (arm a demo fault for the upcoming open motion).
  2. Wait for the window to expire → `enter_opening()` runs, `rlx_gate_command_open()` is called, but due to the armed fault, open is never confirmed (`g_confirmed_open` stays 0).
  3. Wait the full `RLX_OPENING_DEADLINE_MS`=15000ms from entering OPENING.
  4. After FAULT appears (`enter_fault()` again calls `rlx_gate_command_close()` — same regression as TC-SC04A-5), **try** `f` (demo fault-clear) immediately, without waiting for the gate to confirm closed.
  5. Wait the full 3000ms for the gate to finish closing (since `enter_fault()` just issued a close command), then try `f` again — but note `rlx_fsm_on_fault_clear()` only accepts when `gates_confirmed_open()==1`, which requires a prior successful `rlx_gate_command_open()` (not blocked by `x`).
  6. Do **not** press `x` this time; there's no valid way to bring the gate to a genuinely confirmed-open state before fault-clear can be accepted — the correct expectation is: `f` will check `gates_confirmed_open()`; since the gate is currently closed (forced by the fault), fault-clear **must be rejected** at steps 4/5.
- **Expected Result**:
  - Step 3: log `RLx: FAULT latched (fault bit 0x1) ...` appears exactly at t≈15s from entering OPENING.
  - Steps 4/5 (gate currently CLOSED due to fault, never actually OPENed): `rlx_fsm_on_fault_clear()` returns `RESULT_NACK`, `NACK_REASON_FAULT_ACTIVE` (log `[rlx_sensor] fault-clear result=... reason=...`) — **by design**, since RC-10 requires verifying the gate is actually safe (confirmed open) before accepting fault-clear, and a FAULT crossing with a closed gate obviously hasn't had "verified repair."
  - Important distinction from Lx's TC-SC03A-6 Part 2: RLx **does** have a working recovery path (`MSG_REQUEST_FAULT_CLEAR`/key `f`), it just requires a genuine safety condition (gate confirmed open) — unlike Lx, where there was "no path at all." To fully exercise the successful-ACK branch of this transition, the team needs a scenario simulating "the gate has genuinely been repaired and returned to the open position" — since `rlx_gate.c` has no "force confirmed open" API independent of `rlx_gate_command_open()`, the only feasible approach in the current PoC is: after FAULT appears, **don't** press `x` again, and wait for a new OPENING cycle to naturally trigger — but since the state is `RLX_FAULT` (latched, ignoring all new `TRAIN_APPROACHING`), the current code has **no** mechanism to automatically retry `rlx_gate_command_open()` after entering FAULT. => **Additional known gap**: the `FAULT --> OPEN` branch can only really be verified by a test if FAULT was triggered by a cause **unrelated** to gate-confirm-open (e.g. a watchdog trip while the gate is already genuinely confirmed open) — see the variant below.
- **Variant for a real ACK branch (additional)**: From `RLX_OPEN` (gate already confirmed open, `g_confirmed_open=1` from `rlx_gate_init()`), trigger a fault via watchdog instead of the gate: suspend `rlx_main 1` with `kill -STOP`/`kill -CONT`, same approach as for Lx (RLx also has `rlx_fsm_report_watchdog_trip()` called from a similar watchdog thread — check `app/railway/src/rlx_watchdog.c` if it exists). Since `enter_fault()` always calls `rlx_gate_command_close()` regardless of the fault's cause, the gate switches from open to closed (3s) then confirmed closed — i.e. **the same problem recurs**: `gates_confirmed_open()` becomes 0 right after. So in practice, fault-clear ACK is only feasible if the operator waits for... **there is no path** in the current code that ever returns the gate to `g_confirmed_open=1` while in FAULT (no `rlx_gate_command_open()` call happens until FAULT is exited). => Clearly note in the report: **the `FAULT --> OPEN` branch currently CANNOT reach `RESULT_ACK` through any sequence of actions available in the interface**, since the precondition `gates_confirmed_open()==1` never naturally becomes true once in FAULT (every path into FAULT forces the gate closed, and nothing reopens it while in FAULT). This is an **important known gap to report to the instructor/team**, similar to but independent of Lx's gap in TC-SC03A-6.

### TC-SC04B-5 (Known gap, brief note): CLOSED/TRAIN_PRESENT → FAULT due to "gate state contradicts CLOSED" cannot be triggered with the current demo tools
- **Type**: Negative / Known gap
- **Related**: SC-04B, `CLOSED --> FAULT : gate state contradicts CLOSED`, `TRAIN_PRESENT --> FAULT : gate state contradicts CLOSED`
- **Environment**: (A)
- **Note in place of steps**: `check_gate_contradiction_closed()` only returns a fault when `gates_confirmed_closed()==0` while state is CLOSED/TRAIN_PRESENT. But `rlx_gate.c` only changes `g_confirmed_closed` via `rlx_gate_command_close()`/`rlx_gate_command_open()`, both of which are only ever called from `rlx_fsm.c`'s known logic (no gate-open command ever runs while CLOSED/TRAIN_PRESENT). So **no key/IPC sequence in the current build** makes the gate "naturally" contradict CLOSED while the state is still CLOSED — the same kind of gap as SC-04A's "WARNING→FAULT via timeout". Recommendation: if this branch really needs testing, a demo API like `rlx_gate_force_open_for_test()` would be needed — it doesn't currently exist, so **no fake positive test case should be written for this branch**.

---

## 9. SC-05 — Central Connectivity and Local Autonomy

### TC-SC05-1: PA-07 — exactly 3 consecutive missed heartbeats before marking UNAVAILABLE
- **Type**: Positive + Edge case (boundary at exactly the 3rd miss, not the 2nd)
- **Related**: SC-05, `CENTRAL_CONNECTED --> DEGRADED_LOCAL : three consecutive 1 s heartbeats missed`
- **Environment**: (B)
- **Setup**: `c_main` and `lx_main 1` running normally, L1 already shows `AVAILABLE` on C1's table.
- **Steps**:
  1. Note `lx_main 1`'s pid, send `kill -STOP <pid>` (fully suspends the process — its 1Hz heartbeat stops entirely).
  2. Watch C1's table each second (auto-refreshes at 1Hz): right after 1 second (`missed_heartbeat_ticks=1`) and 2 seconds (`=2`), L1's AVAILABILITY column **must still be** `AVAILABLE`.
  3. Exactly at the 3rd second (`missed_heartbeat_ticks==3`), watch for the Central log: `Controller 1 marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)` (also written to `central_log.txt`), and the AVAILABILITY column switches to `UNAVAILABLE`.
  4. `kill -CONT <pid>` to clean up the process for later tests.
- **Expected Result**: As above — the transition boundary is exactly the 3rd miss, not earlier (edge case: if UNAVAILABLE appears at the 2nd second, that's a bug violating PA-07's "three consecutive").

### TC-SC05-2: DEGRADED_LOCAL — Lx/RLx keep operating fully normally when Central isn't receiving heartbeats
- **Type**: Positive / Regression note
- **Related**: SC-05, note "Connectivity loss alone never forces all-red, FLASHING_RED, or frozen timing — traffic, pedestrian, railway, and fault logic (SC-01 through SC-04) all continue locally"
- **Environment**: (B)
- **Setup**: Similar to TC-SC05-1 — `lx_main 1` under `kill -STOP` looks like "disconnected"/UNAVAILABLE from Central's view, but **this time don't actually STOP it**, since L1 needs to keep running to observe: instead, simply **don't run `c_main` at all** for the whole test (environment (A) alone also proves the point, since `lx_fsm.c` never reads `fsm->link_state` in any SC-01/02/03/04 transition).
- **Steps**: Run `lx_main 1` alone (no `c_main`), press normal sensor keys (`a`, `1`, `w`, etc.) and watch the full ARTERIAL/CONNECTOR cycle and WALK/FDW sequence run with the same durations as the tests in sections 1-4 of this document.
- **Expected Result**: No behavioral difference at all compared to running with `c_main` — confirmed directly from source: no `if (fsm->link_state == ...)` statement anywhere in `lx_fsm.c` governs phase/pedestrian/override/railway-preemption logic. This satisfies SC-05's note "by construction," not via any dedicated fallback mechanism.

### TC-SC05-3: Instant resync when heartbeat resumes — and the RESYNCHRONISING display gap
- **Type**: Positive + Known gap
- **Related**: SC-05, `DEGRADED_LOCAL --> RESYNCHRONISING --> CENTRAL_CONNECTED`
- **Environment**: (B)
- **Setup**: Repeat TC-SC05-1 until L1 = `UNAVAILABLE`.
- **Steps**:
  1. `kill -CONT <pid of lx_main 1>` so L1 resumes and automatically resends `MSG_HEARTBEAT` (1Hz, no action needed from L1's side).
  2. Watch C1's table at the next 1Hz refresh after the first heartbeat arrives.
- **Expected Result**: `c_server_record_status()` (called from the `MSG_HEARTBEAT` case in `c_main.c`) resets `missed_heartbeat_ticks=0` and `marked_unavailable=0` **on the very first heartbeat received** — AVAILABILITY jumps straight `UNAVAILABLE → AVAILABLE` in one tick, no visible intermediate "resyncing" step. *Known gap*: SC-05 draws an explicit `RESYNCHRONISING` state ("send complete current state to C1" / "C1 accepts complete state exchange"), but the current code has no distinct "full state" STATUS message sent on reconnect — every `MSG_HEARTBEAT`/`MSG_STATUS` carries the same `status_report_payload_t`, and its `link_state` field is **always hard-coded** to `LINK_CENTRAL_CONNECTED` by both `lx_comm.c` (`req.payload.heartbeat.summary.link_state = (uint32_t)LINK_CENTRAL_CONNECTED;`) and `rlx_comm.c`, regardless of actual connectivity — meaning this displayed column (if the HMI ever printed `link_state`) would never reflect real DEGRADED_LOCAL/RESYNCHRONISING states, even though Central currently considers the controller UNAVAILABLE. Note this as a known PoC limitation, not a newly discovered bug, to be raised at acceptance review.

---

## Appendix — Summary of Known Gaps Found While Writing This Test Plan

| Gap | Chart | Location in code | Impact |
|---|---|---|---|
| ~~No recovery path from `FAULT_SAFE -> NORMAL_OPERATION` for Lx via keyboard/IPC~~ (FIXED) | SC-03A | `MSG_REQUEST_FAULT_CLEAR` is now handled by `lx_main.c`'s `on_request()`, calling `lx_fsm_on_request_fault_clear()`; `c_operator.c`'s `f` key asks for node type (0=Lx/1=RLx) | No longer a gap — see TC-SC03A-6 Part 2. Additional re-audit fix: resumes `RAILWAY_PREEMPTION` correctly instead of always `NORMAL_OPERATION` if the adjacent crossing is still not `OPEN` at clear time (`fsm->last_crossing_state`) |
| RLx's `FAULT --> OPEN` branch cannot reach `RESULT_ACK` through any sequence of actions | SC-04B | `rlx_fsm_on_fault_clear()` requires `gates_confirmed_open()==1`, but every path into FAULT (`enter_fault()`) forces the gate closed and nothing reopens it while in FAULT | See TC-SC04B-4 variant |
| `WARNING --> FAULT` (60s diagnostic timeout) cannot be triggered | SC-04A | `RLX_WARNING_TO_CLOSING_MS` (5s) always fires first, resetting `state_elapsed_ms`; the discrete simulated event can't represent a sensor "stuck continuously active" | Dead code per the comment in `rlx_fsm.c` itself |
| `CLOSED/TRAIN_PRESENT --> FAULT` due to gate contradiction cannot be triggered with current demo tools | SC-04B | `rlx_gate.c` only changes confirm state via commands issued by `rlx_fsm.c` itself | Needs a demo API to actually test |
| `REQUEST_LATCHED` "stuck-active beyond diagnostic timeout" (PA-03) is never detected | SC-02 | `lx_fsm_latch_pedestrian_request()` has a "KNOWN LIMITATION" comment confirming `FAULT_PED_BUTTON_STUCK` is never set | No positive test case written for this branch in this document |
| `link_state` is always hard-coded to `LINK_CENTRAL_CONNECTED` in every outgoing heartbeat | SC-05 | `lx_comm.c`, `rlx_comm.c` | RESYNCHRONISING can't be observed directly via this field; only inferred indirectly via AVAILABILITY on C1 |

Total test cases in this document: **42** (including variants/edge cases nested within some TCs).
