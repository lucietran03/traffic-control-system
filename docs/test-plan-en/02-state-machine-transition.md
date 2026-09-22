# Test Plan 02 — State Machine Transitions

This document lists test cases for each transition in `STATE_CHARTS.md`
(SC-01A/B/C, SC-02, SC-03A/B, SC-04A/B, SC-05), cross-checked directly against
the real source code as of this writing:

- `app/intersection/src/lx_fsm.c`, `app/intersection/includes/lx_timer.h`
- `app/intersection/src/lx_sensor.c`, `app/intersection/src/lx_signal.c`
- `app/railway/src/rlx_fsm.c`, `app/railway/includes/rlx_timer.h`
- `app/railway/src/rlx_sensor.c`, `app/railway/src/rlx_gate.c`, `app/railway/src/rlx_signal.c`
- `app/central/src/c_watchdog_mon.c`, `app/central/src/c_operator.c`,
  `app/central/src/c_hmi.c`, `app/central/src/c_server.c`, `app/central/src/c_logger.c`
- `app/shared/includes/sys_types.h`

All timing constants, key names, and log field names in this document are
taken directly from the files above — nothing is guessed. Where the code has
a known limitation/gap (no way to trigger a transition through the existing
interface), the test case is explicitly marked "Known gap" instead of
inventing a trigger path that doesn't exist.

## 0. General Conventions

### 0.1 Three test environments A/B/C

| Symbol | Description | When to use |
|---|---|---|
| **(A) Single node** | Only **one** process (`lx_main N` or `rlx_main N`) runs on one QNX machine, no `c_main`. Fully controlled via that process's own sensor keyboard (`lx_sensor.c` / `rlx_sensor.c`). | Testing internal transitions of SC-01, SC-02, SC-04 that don't need Central or Lx/RLx communication. |
| **(B) Multiple nodes, same QNX machine** | Multiple processes (`c_main`, `lx_main 1`, `rlx_main 1`, …) run on the **same** QNX target (multiple windows/consoles on one machine). `TRAFFIC_NODE_MAP` need not be exported (defaults to "same node"). | Testing override (SC-03B) — requires `c_main` since only `c_operator.c` can send `REQUEST_OVERRIDE`/`RENEW_OVERRIDE`/`CANCEL_OVERRIDE`. Also the Lx–RLx interaction part of SC-03A (RAILWAY_PREEMPTION), and SC-05. |
| **(C) Multiple machines/VMs, real network** | Each role runs on a different physical VM/PC, connected via Qnet, with `TRAFFIC_NODE_MAP` exported per `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` (Case 1/2/3). | Repeat the most important (B) test cases (especially SC-03A regression test #1, SC-05) on a real network topology before final acceptance, since real Qnet latency can expose race conditions invisible when running same-node. |

Note: the actual demo deployment uses **10 separate QNX VMs, one controller
per VM** — no two controllers ever actually run on the same machine.
Accordingly every test case marked (B) in this document is really a
reduced-hardware substitute for the real (C) environment with
`TRAFFIC_NODE_MAP` exported, and does not reflect the final deployment
topology.

Every test case below states the minimum required environment on the
**Environment** line. A test marked (B) can always be repeated on (C) if
the team has enough machines — recommended for tests labeled "Critical
regression."

### 0.2 Build & process startup

```
make        # build/bin/c_main, build/bin/lx_main, build/bin/rlx_main
/tmp/c_main            # Central, no arguments
/tmp/lx_main <1-6>     # select L1..L6
/tmp/rlx_main <1-3>    # select RL1..RL3
```

Recommended startup order (same as `QNX_DEPLOYMENT_RUN_GUIDE.md`): `c_main`
first, then `rlx_main`, then `lx_main`. For environment (C), export
`TRAFFIC_NODE_MAP` in **every** shell before running any binary, per section
2.2 of that guide. The RLx–Lx adjacency map is fixed in code
(`app/railway/src/rlx_comm.c`, `ADJACENCY` array):

| RLx | Adjacent to |
|---|---|
| RL1 | L1, L2 |
| RL2 | L3, L4 |
| RL3 | L5, L6 |

### 0.3 Key bindings

**`lx_sensor.c` (keyboard for each `lx_main` process)**

| Key | Effect |
|---|---|
| `a` / `A` | Vehicle present / departs on arterial approach |
| `c` / `C` | Vehicle present / departs on connector approach |
| `1`/`2`/`3`/`4` | Press pedestrian button side 0/1/2/3 |
| `w` / `W` | Toggle queue-warning on/off (CC-01) |

**`rlx_sensor.c` (keyboard for each `rlx_main` process)**

| Key | Effect |
|---|---|
| `0` / `1` | TRAIN_APPROACHING direction 0 / direction 1 |
| `x` | Force the **next** gate close/open cycle to never confirm (demo RC-06 fault) |
| `r` | Demo: directly calls `rlx_gate_force_confirmed_open()` (`rlx_gate.c`) — simulates "gate just repaired and confirmed open" instantly, forces `g_confirmed_open=1`, `g_confirmed_closed=0` (and clears any pending motion/fault-armed state), regardless of the real prior physical state (RC-09/RC-10 fault-clear demo) |
| `f` | Demo-only: directly calls `rlx_fsm_on_fault_clear()` locally (bypasses the real `MSG_REQUEST_FAULT_CLEAR` IPC path from Central) |

**`c_operator.c` (console of `c_main`, exists only while Central is running)**

| Key | Effect |
|---|---|
| `m` | `SET_MODE` for an Lx (enter Lx number 1-6, mode 0=PEAK_FIXED/1=OFF_PEAK_SENSOR) |
| `t` | Broadcast `SET_TIMING_PROFILE` for chain R1 or R2 |
| `o` | `REQUEST_OVERRIDE` (enter Lx, `target_movement` 0=arterial/1=connector, `duration_ms`) |
| `r` | `RENEW_OVERRIDE` (enter Lx, `extend_duration_ms`, 0 = keep current duration) |
| `c` | `CANCEL_OVERRIDE` (enter Lx) |
| `f` | `REQUEST_FAULT_CLEAR` (select node type 0=Lx 1-6 or 1=RLx 1-3 — now also targets Lx, see `lx_fsm_on_request_fault_clear()`/TC-SC03A-6 Part 2) |
| `d` | `handle_demo_hour()` — enter a simulated hour (0-23) to force `c_mode_eng` to treat it as "current time" instead of reading the real clock, allowing PEAK/OFF_PEAK switching demos (DP-01/DP-02) without waiting for the real time; immediately broadcasts `SET_MODE` to all Lx per the mode that simulated hour implies |
| `a` | `handle_resume_automatic()` — cancels the `d` key's simulated-hour override, returns to automatically reading the real clock for PEAK/OFF_PEAK (takes effect within 1s on the next tick, does not broadcast immediately) |

### 0.4 How to read the logs

- Each `lx_main`/`rlx_main` prints directly to its own stdout (no
  timestamp) — e.g. `Lx 1: signal phase now ARTERIAL GREEN`,
  `RLx: commanding gates DOWN (simulated motion, 3000 ms)`.
- `c_main` prints to stdout **and** writes to `central_log.txt` (same
  directory `c_main` runs from) in the format `[YYYY-MM-DD HH:MM:SS] <content>`
  (`c_logger_log()`).
- `c_main`'s network status table (`c_hmi_render()`, columns
  `ID ROLE MODE PHASE CROSSING_STATE SUPERVISORY FAULTS SENSOR OVERRIDE
  AVAILABILITY`) auto-refreshes **every 1 second** (from
  `IPC_PULSE_HEARTBEAT_TICK` at 1 Hz) — no key press needed to refresh. These
  columns are raw integers; look them up in the table below.

**Display code table on C1**

| Field | Value / Meaning |
|---|---|
| MODE | `0`=PEAK_FIXED, `1`=OFF_PEAK_SENSOR |
| PHASE | `0`=ARTERIAL_GREEN, `1`=ARTERIAL_YELLOW, `2`=ALL_RED_A_TO_B, `3`=CONNECTOR_GREEN, `4`=CONNECTOR_YELLOW, `5`=ALL_RED_B_TO_A |
| CROSSING_STATE | `0`=OPEN, `1`=WARNING, `2`=CLOSED, `3`=FAULT |
| SUPERVISORY | `0`=FAULT_SAFE, `1`=RAILWAY_PREEMPTION, `2`=CENTRAL_OVERRIDE, `3`=NORMAL_OPERATION |
| OVERRIDE | `1` if `override_substate==OVR_ACTIVE`, else `0` (cannot distinguish `OVR_PENDING_CLEARANCE` from this column alone — must check `c_operator`/Lx console logs) |
| AVAILABILITY | `AVAILABLE` / `UNAVAILABLE` (`marked_unavailable`, PA-07) |

### 0.5 Timing constants used throughout this document

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
(simulated gate motion time) and the railway tick cycle = 1000 ms/tick
(`rlx_fsm_on_tick()` is called from `IPC_PULSE_RAILWAY_WARNING` every 1 s).

---

## 1. SC-01A — Vehicle-Signal Mode Selection Overview

### TC-SC01A-1: SET_MODE deferred until the correct safe phase boundary
- **Type**: Positive
- **Related**: SC-01A, `PEAK_FIXED -> mode_selection : pending mode change [safe phase boundary]`
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: L1 in `MODE_PEAK_FIXED` (default at startup — `lx_fsm_init()`), currently in `PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. On the C1 console, press `m`, enter Lx = `1`, mode = `1` (OFF_PEAK_SENSOR) right as L1 enters ARTERIAL_GREEN (watch for `Lx 1: signal phase now ARTERIAL GREEN` in the L1 terminal).
  2. Observe the reply sent to C1 (`c_comm.c` logs `RESULT_ACK_PENDING`, since the new mode differs from the current mode — `lx_fsm_on_set_mode()`).
  3. Wait until L1 logs `ARTERIAL YELLOW` (48s after step 1) then `ALL RED (A to B)` (+4s) — mode must not change at either point.
  4. Wait 2s more (~54s total from step 1) until L1 crosses the boundary `ALL_RED_A_TO_B -> boundary_after_arterial`.
- **Expected Result**: The mode change only actually takes effect exactly when `ALL_RED_A_TO_B` finishes processing (in `lx_fsm_advance_phase_locked()`, branch `case PHASE_ALL_RED_A_TO_B`) — L1 enters `CONNECTOR_GREEN` normally (since `boundary_after_arterial --> CONNECTOR_GREEN` doesn't depend on the new mode), but the **next** phase after that (when it returns to ARTERIAL_GREEN) already runs on OFF_PEAK_SENSOR timing (no longer a fixed 48s/30s). C1's state (`c_hmi_render`) shows L1's MODE switching from `0` to `1` right at the 54s mark, not earlier.

### TC-SC01A-2: Requesting the mode already running → immediate ACK, no deferral
- **Type**: Edge case
- **Related**: SC-01A, no-op branch in `lx_fsm_on_set_mode()` (request matches current `fsm->mode`)
- **Environment**: (B)
- **Setup**: L1 currently `MODE_PEAK_FIXED`.
- **Steps**: Press `m`, Lx=`1`, mode=`0` (PEAK_FIXED — same as current mode).
- **Expected Result**: `c_operator` logs `RESULT_ACK` (not `ACK_PENDING`). `mode_change_pending` is not set (indirect check: sending another `SET_MODE` with mode=1 right after must work normally as a fresh request, not conflated with the earlier one). L1's current phase is undisturbed — signal logs continue on schedule.

### TC-SC01A-3: SET_MODE NACKed while in FAULT_SAFE
- **Type**: Negative
- **Related**: SC-01A / SC-03A overlap — `lx_fsm_check_fault_locked()` is called at the top of `lx_fsm_on_set_mode()`
- **Environment**: (B)
- **Setup**: Put L1 into FAULT_SAFE via a real watchdog trip: suspend the entire `lx_main 1` process with `kill -STOP <pid>` for more than 2 seconds (threshold `LX_WATCHDOG_CHECK_INTERVAL_S`=2s in `lx_watchdog.c`) then `kill -CONT <pid>` — on resume, `lx_watchdog_thread` detects `phase_tick_counter` hasn't changed and calls `lx_fsm_report_watchdog_trip()`.
- **Steps**:
  1. `kill -STOP <pid lx_main 1>`, wait 3s, `kill -CONT <pid>`.
  2. Watch for logs `Lx: WATCHDOG - no phase-timer activity for 2 s, reporting fault (PA-10)` and `Lx 1: entering FAULT_SAFE mode - holding safe outputs (all-red/dark)`.
  3. On C1, press `m`, Lx=`1`, mode=`1`.
- **Expected Result**: `reply->result = RESULT_NACK`, `reply->reason = NACK_REASON_FAULT_ACTIVE`. L1's SUPERVISORY column on C1 = `0` (FAULT_SAFE) and unchanged.
- **Note**: The conclusion above — that `kill -STOP`/`kill -CONT` trips the PA-10 watchdog (`lx_watchdog.c`) — is disputed by `docs/test-plan/05-fault-safety.md`'s TC-FAULT-16 for this **exact mechanism**: TC-FAULT-16 argues `SIGSTOP` suspends the entire process (all threads, including the watchdog's own checking thread), so in practice it **cannot** trip it. This is a genuine contradiction between the two documents that cannot be resolved by reading code alone — it depends on QNX's real `SIGSTOP`/`SIGCONT` scheduling behavior. Until tested on real hardware, the correct outcome remains **undetermined** — both possibilities must be checked on real QNX hardware. (This does not apply to TC-SC05-1 — the Central-side PA-07/`c_watchdog_mon.c` mechanism is independent and its conclusion is unaffected.) **This case must therefore be recorded as Skip in any results summary (not Pass)** until real `kill -STOP`/`kill -CONT` testing on QNX confirms the precondition (steps 1-2) actually produces `FAULT_SAFE` — marking step 3 Pass from a code read alone would assume the precondition already holds, which is unverified.

---

## 2. SC-01B — PEAK_FIXED Phase Detail

### TC-SC01B-1: Fixed cycle exactly 90s (48+4+2+30+4+2)
- **Type**: Positive
- **Related**: SC-01B, entire chain `ARTERIAL_GREEN -> ... -> ALL_RED_B_TO_A -> ARTERIAL_GREEN`
- **Environment**: (A) single `lx_main 1`
- **Setup**: L1 defaults to `MODE_PEAK_FIXED`, no sensor key presses needed (TL-01/TL-02: sensors don't affect PEAK_FIXED).
- **Steps**: Start a stopwatch at the first `Lx 1: signal phase now ARTERIAL GREEN` log line, record the relative timestamp of each subsequent log line until the next `ARTERIAL GREEN`.
- **Expected Result**: Order and delays between log lines:
  `ARTERIAL GREEN` (t=0) → `ARTERIAL YELLOW` (t≈48.0s) → `ALL RED (A to B)` (t≈52.0s) → `CONNECTOR GREEN` (t≈54.0s) → `CONNECTOR YELLOW` (t≈84.0s) → `ALL RED (B to A)` (t≈88.0s) → `ARTERIAL GREEN` (t≈90.0s). Tolerance ±1 tick (100ms) due to `LX_PHASE_TICK_MS` granularity.

### TC-SC01B-2: Sensors don't shorten/extend PEAK_FIXED
- **Type**: Negative (control test)
- **Related**: SC-01B, note "Ordinary approach-presence sensors ... never extend or shorten this fixed cycle (TL-01, TL-02)"
- **Environment**: (A)
- **Setup**: L1 in `MODE_PEAK_FIXED`, just entered `ARTERIAL_GREEN`.
- **Steps**:
  1. Right after entering ARTERIAL_GREEN, repeatedly press `a A a A c C` (toggle both approaches several times).
  2. Wait a full 48s.
- **Expected Result**: `ARTERIAL_GREEN -> ARTERIAL_YELLOW` still occurs exactly at t≈48.0s — not earlier despite `arterial_vehicle_demand`=0 (last set via `A`), not later despite `connector_vehicle_demand`=1 (last set via `c`). Confirms the `if (fsm->mode == MODE_PEAK_FIXED)` branch in `lx_fsm_on_phase_timer()` (case `PHASE_ARTERIAL_GREEN`) only compares `green_elapsed_ms` to `lx_timer_peak_green_duration_ms()`, ignoring `arterial_vehicle_demand`/`connector_vehicle_demand`.

### TC-SC01B-3: Exact 48000ms boundary (not 100ms early, not late)
- **Type**: Edge case
- **Related**: SC-01B, `ARTERIAL_GREEN --> ARTERIAL_YELLOW : after 48 s`
- **Environment**: (A)
- **Setup**: L1 `MODE_PEAK_FIXED`, just entered ARTERIAL_GREEN.
- **Steps**: Since logs have no millisecond timestamps, use an external clock accurate to ≤50ms; record when the `ARTERIAL YELLOW` log appears relative to the `ARTERIAL GREEN` log.
- **Expected Result**: Difference falls within [47.9s, 48.1s] — consistent with `lx_fsm_on_phase_timer()` accumulating exactly 100ms/tick and checking `green_elapsed_ms >= 48000` (tick 480 is the first to satisfy `>=`, i.e. exactly 48000ms, not 47900ms).

---

## 3. SC-01C — OFF_PEAK_SENSOR Phase Detail

### TC-SC01C-1: Minimum 8s green respected even with immediate opposing demand
- **Type**: Edge case (timing boundary)
- **Related**: SC-01C, guard `lx_timer_should_exit_green()` — "Never returns 1 (exit) below LX_MIN_GREEN_MS"
- **Environment**: (A)
- **Setup**: Switch L1 to OFF_PEAK_SENSOR (simplest (A)-only path: restart `lx_main 1`, wait for ARTERIAL_GREEN, then — since (A) has no C1 to send SET_MODE — use minimal (B): `c_main` + `lx_main 1`, press `m` Lx=1 mode=1, wait for it to apply as in TC-SC01A-1). As soon as L1 enters `ARTERIAL_GREEN` under OFF_PEAK_SENSOR (no `arterial_vehicle_demand`), immediately press `c` (connector demand=1) at t=0.
- **Steps**:
  1. t=0: press `c`.
  2. Watch logs every 4s (`LX_EXTENSION_MS`) — the guard re-check cycle.
- **Expected Result**: `ARTERIAL_GREEN` does **not** exit at t=4000ms even though `own_demand=0` (arterial) and `other_demand=1` (connector) satisfy the logic — because `lx_timer_should_exit_green()` always returns `0` while `elapsed_ms < LX_MIN_GREEN_MS` (8000). `ARTERIAL_GREEN -> ARTERIAL_YELLOW` only occurs at tick t=8000ms (first `% LX_EXTENSION_MS == 0` check where `elapsed_ms >= 8000`).

### TC-SC01C-2: No demand anywhere → rests on arterial green indefinitely (DP-04)
- **Type**: Positive
- **Related**: SC-01C, note "With no demand anywhere ... rests on arterial green indefinitely (DP-04)"
- **Environment**: (B) (use the mode-switch path from TC-SC01C-1)
- **Setup**: L1 in OFF_PEAK_SENSOR, just entered ARTERIAL_GREEN, no `a/c/1/2/3/4` keys pressed.
- **Steps**: Watch logs for at least 60s (past `LX_MAX_GREEN_MS`=40000ms).
- **Expected Result**: No `SIGNAL ->` log line appears — L1 keeps showing `ARTERIAL GREEN` past the 40s mark, since the exit condition "connector demand pending" (`requires_other_demand=1`) is never true when neither approach has demand — even at `green >= 40s`, `lx_timer_should_exit_green()` (reading `own_demand`/`other_demand`, with no unconditional-expiry branch for arterial when `requires_other_demand=1`) still returns `0`.

### TC-SC01C-3: DP-06 anti-starvation — arterial forced to yield at exactly 40s despite continued demand
- **Type**: Edge case (2 competing conditions: continuous arterial demand vs. 40s cap)
- **Related**: SC-01C, `ARTERIAL_GREEN --> ARTERIAL_YELLOW : [green >= 8 s and connector demand pending and (no arterial demand or green = 40 s)]`
- **Environment**: (B)
- **Setup**: L1 OFF_PEAK_SENSOR, entering ARTERIAL_GREEN.
- **Steps**:
  1. t=0: press `a` (arterial demand=1) and `c` (connector demand=1), hold both on for the whole test (don't press `A`/`C`).
  2. Watch logs every 4s.
- **Expected Result**: `ARTERIAL_GREEN` self-extends (self-loop, no new log since same phase) at 8s/12s/…/36s (since `own_demand=1` so "no arterial demand" is false, and `green=40s` isn't reached yet so the guard still returns 0). Exactly at t=40000ms, `green_elapsed_ms >= LX_MAX_GREEN_MS` makes `(no arterial demand or green = 40s)` true regardless of `own_demand` — `ARTERIAL_GREEN -> ARTERIAL_YELLOW` fires exactly at t≈40.0s, not earlier or later — even though `arterial_vehicle_demand` is still =1.

### TC-SC01C-4: Connector green also forced to exit at 40s despite continuous demand (no reverse anti-starvation)
- **Type**: Edge case
- **Related**: SC-01C, `CONNECTOR_GREEN --> CONNECTOR_YELLOW : [green >= 8 s and (no connector demand or green = 40 s)]`
- **Environment**: (B)
- **Setup**: L1 OFF_PEAK_SENSOR, already at `CONNECTOR_GREEN` (wait out a full ARTERIAL round first, or let natural demand lead there).
- **Steps**: Hold `c` on (connector demand=1) throughout the phase, never turn it off.
- **Expected Result**: `CONNECTOR_GREEN -> CONNECTOR_YELLOW` still occurs exactly at t≈40.0s from entering CONNECTOR_GREEN — unlike arterial, the call `lx_timer_should_exit_green(..., other_demand=0u, requires_other_demand=0u)` in `lx_fsm_on_phase_timer()`'s `PHASE_CONNECTOR_GREEN` case has no extra condition besides "no connector demand or green=40s", so the 40s cap always wins regardless of demand — confirming the `lx_timer.h` comment "arterial gets service again unconditionally next cycle regardless of its own demand".

---

## 4. SC-02 — Generic Pedestrian-Signal State Chart

### TC-SC02-1: Request latched during an incompatible phase, served when a compatible phase begins
- **Type**: Positive
- **Related**: SC-02, `DONT_WALK -> REQUEST_LATCHED -> WALK`
- **Environment**: (A) `lx_main 1`, `MODE_PEAK_FIXED`
- **Setup**: Wait for L1 to reach `CONNECTOR_GREEN` (side 0/1 compatible with ARTERIAL, not with CONNECTOR — `lx_fsm_arterial_ped_compatible_locked()`).
- **Steps**:
  1. During `CONNECTOR_GREEN`/`CONNECTOR_YELLOW`/`ALL_RED_B_TO_A`, press `1` (side 0).
  2. Observe: no `PED SIGNAL side 0 -> WALK` line appears immediately.
  3. Wait until L1 logs `signal phase now ARTERIAL GREEN`.
- **Expected Result**: In the very tick L1 enters `PHASE_ARTERIAL_GREEN` (technically the next tick of `lx_fsm_ped_service_tick_locked()`, since it reads the current `fsm->phase`), the log `Lx 1: PED SIGNAL side 0 -> WALK` appears. The request is never discarded while waiting (per PA-02/"not discarded").

### TC-SC02-2: WALK 6000ms and FLASHING_DONT_WALK 4000ms durations exact, latch cleared at the right time
- **Type**: Positive
- **Related**: SC-02, `WALK --> FLASHING_DONT_WALK : after 6s`, `FLASHING_DONT_WALK --> DONT_WALK : after 4s`
- **Environment**: (A)
- **Setup**: Continue from TC-SC02-1, or press `1` right as ARTERIAL_GREEN begins.
- **Steps**: Measure time between 3 log lines: `WALK` → `FLASHING_DONT_WALK` → `DONT_WALK`.
- **Expected Result**: `WALK` (t=0) → `FLASHING_DONT_WALK` (t≈6.0s) → `DONT_WALK` (t≈10.0s). After `DONT_WALK`, pressing `1` again must restart a **new** WALK/FDW sequence from scratch at the next ARTERIAL_GREEN (confirms `ped_latched[0]` was cleared to 0 in `lx_fsm_ped_service_tick_locked()`'s `else { fsm->ped_latched[side] = 0; }` branch).

### TC-SC02-3: Coalescing repeated presses while still REQUEST_LATCHED
- **Type**: Positive
- **Related**: SC-02, `REQUEST_LATCHED --> REQUEST_LATCHED : PED_REQUEST(side) [request already pending]`
- **Environment**: (A)
- **Setup**: L1 currently `CONNECTOR_GREEN` (side 0 not yet compatible).
- **Steps**: Press `1` five times, ~1s apart, while still in CONNECTOR_GREEN/YELLOW/ALL_RED.
- **Expected Result**: No extra logs from the redundant presses (`lx_fsm_latch_pedestrian_request()` just sets `ped_latched[0]=1`; re-setting is harmless/idempotent). When ARTERIAL_GREEN arrives, only **one** WALK/FDW sequence runs for side 0 — not repeated or extended.

### TC-SC02-4: Coalescing a press during an active WALK (doesn't restart the duration)
- **Type**: Positive
- **Related**: SC-02, `WALK --> WALK : PED_REQUEST(side) / coalesce repeated request`
- **Environment**: (A)
- **Setup**: side 0 currently in WALK (t≈2s since WALK began).
- **Steps**: Press `1` again at t≈2s (within the 0–6s WALK window).
- **Expected Result**: `FLASHING_DONT_WALK` still appears exactly at t≈6.0s from the **original** WALK start (not pushed to t≈8s) — confirms mid-window presses don't reset `ped_phase_elapsed_ms`. (In code, pressing during WALK hits the `ped_recall[side]=1` branch because `ped_serving_mask` already has bit 0 set — see TC-SC02-5 for the consequences of this flag.)

### TC-SC02-5 (Critical regression): `ped_recall` — a mid-sequence press never loses the request
- **Type**: Positive / Regression
- **Related**: SC-02 note "remains latched, not discarded (PA-02)" + the `ped_recall` mechanism in `lx_fsm_ped_service_tick_locked()`/`lx_fsm_latch_pedestrian_request()`
- **Environment**: (A)
- **Setup**: side 0 currently being served (WALK or FDW running, `ped_serving_mask` has bit 0 set).
- **Steps**:
  1. During FLASHING_DONT_WALK (e.g. t≈2s within its 4s window), press `1` again.
  2. Wait for FDW to end → `DONT_WALK` appears (t≈4s after FDW began).
  3. Keep watching L1 through the rest of CONNECTOR_GREEN/…/ALL_RED_B_TO_A, until ARTERIAL_GREEN returns.
- **Expected Result**: Right after `DONT_WALK` prints, `ped_latched[0]` is **still 1** (not cleared) because the `if (fsm->ped_recall[side])` branch only clears `ped_recall[side]`, deliberately leaving `ped_latched[side]` untouched. So at the very next ARTERIAL_GREEN, a **new** WALK/FDW sequence for side 0 must start automatically without pressing `1` again — direct proof that a mid-sequence press is never dropped.

### TC-SC02-6: Pressing a different button (not yet in the current compatible_mask) while another side of the same phase is being served
- **Type**: Edge case
- **Related**: SC-02, comment "A side that latches mid-sequence for the SAME phase is picked up the next time a sequence starts ... not folded into one already in progress"
- **Environment**: (A)
- **Setup**: side 0 in WALK (side 1 was **not** pressed when WALK started, i.e. `compatible_mask` at start only had bit 0).
- **Steps**:
  1. At t≈2s into side 0's WALK, press `2` (side 1 — also ARTERIAL_GREEN-compatible but arriving late).
  2. Observe: no `PED SIGNAL side 1 -> WALK` appears immediately (since `ped_serving_mask` currently only has bit 0, side 1 isn't folded in).
  3. Watch side 0's full sequence (WALK→FDW→DONT_WALK) then the rest of CONNECTOR/ALL_RED, until the next ARTERIAL_GREEN begins.
- **Expected Result**: side 1's WALK only starts at the **next** ARTERIAL_GREEN (one 90s cycle later under PEAK_FIXED), never folded into or shortening side 0's running sequence — matching the design "only one WALK/FDW sequence instance at a time."

---

## 5. SC-03A — Intersection Supervisory Authority Overview

### TC-SC03A-1 (CRITICAL REGRESSION): RAILWAY_PREEMPTION doesn't freeze the whole intersection at ALL_RED — arterial keeps cycling normally, only connector is suppressed
- **Type**: Positive / Regression (this is the most important fixed bug — a bare `break` in `lx_fsm_advance_phase_locked()` previously left `fsm->phase` stuck permanently at `PHASE_ALL_RED_A_TO_B`)
- **Related**: SC-03A, `NORMAL_OPERATION --> RAILWAY_PREEMPTION` and its internal behavior (CC-02)
- **Environment**: (B) or (C) — needs `rlx_main 1` (RL1, adjacent to L1/L2) + `lx_main 1`. `c_main` not required (crossing status goes straight RLx→Lx via `rlx_comm_broadcast_crossing_status_if_changed()`), but running it too makes it easier to watch the SUPERVISORY column via `c_hmi_render()`.
- **Setup**: L1 in `MODE_PEAK_FIXED`, cycling normally. RL1 in `RLX_OPEN`.
- **Steps**:
  1. On the RL1 console, press `0` (TRAIN_APPROACHING direction 0) → RL1 enters `RLX_WARNING`, sending `MSG_CROSSING_STATUS(WARNING)` to L1 and L2 almost immediately (broadcast every 1s tick if changed).
  2. On C1 (if running), watch L1's SUPERVISORY column go from `3` (NORMAL_OPERATION) to `1` (RAILWAY_PREEMPTION) within ≤1s.
  3. **Watch L1's log continuously for at least 3 minutes** (enough for RL1 to naturally progress WARNING(5s)→CLOSING(~3s)→CLOSED→wait 20s→TRAIN_PRESENT→wait 20s→OPENING(~3s), i.e. ~51s minimum with no second train — but since we won't let the crossing reopen at this step, just watch through the extended CLOSED period).
  4. Count how many times `Lx 1: signal phase now ARTERIAL GREEN` appears while RAILWAY_PREEMPTION is active (RL1 not yet OPEN again).
- **Expected Result**:
  - L1 must show **more than one** full `ARTERIAL_GREEN → ARTERIAL_YELLOW → ALL_RED_A_TO_B → ARTERIAL_GREEN` cycle while RAILWAY_PREEMPTION is active — i.e. arterial keeps its normal 48+4+2=54s/cycle rhythm (no 90s connector phase interleaved).
  - `Lx 1: signal phase now CONNECTOR GREEN` must **never** appear during RAILWAY_PREEMPTION — every time `PHASE_ALL_RED_A_TO_B` is reached it must loop straight back to `PHASE_ARTERIAL_GREEN` (branch `if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) { fsm->phase = PHASE_ARTERIAL_GREEN; break; }` in `lx_fsm_advance_phase_locked()`).
  - Specifically: L1 must **never** sit at `ALL RED (A to B)` longer than 2s at a time — if the log shows L1 stuck at `ALL RED (A to B)` indefinitely while RL1 is still WARNING/CLOSED, that's the old fixed bug regressing.

### TC-SC03A-2: NORMAL_OPERATION → CENTRAL_OVERRIDE (REQUEST_OVERRIDE accepted)
- **Type**: Positive
- **Related**: SC-03A, `NORMAL_OPERATION --> CENTRAL_OVERRIDE : REQUEST_OVERRIDE(CLEAR_ROUTE, target, duration)`
- **Environment**: (B)
- **Setup**: L1 NORMAL_OPERATION, no pedestrian clearance running (`ped_clearance_active=0` — avoid pressing pedestrian buttons beforehand).
- **Steps**: On C1 press `o`, Lx=`1`, target movement=`0` (arterial), duration_ms=`20000`.
- **Expected Result**: `c_operator` logs "REQUEST_OVERRIDE(...) submitted"; L1's SUPERVISORY on C1 becomes `2` (CENTRAL_OVERRIDE), OVERRIDE column=`1` within ≤1s.

### TC-SC03A-3: CENTRAL_OVERRIDE → RAILWAY_PREEMPTION when the adjacent crossing becomes active mid-override
- **Type**: Positive (contested path: override active + train arriving simultaneously)
- **Related**: SC-03A, `CENTRAL_OVERRIDE --> RAILWAY_PREEMPTION : adjacent crossing becomes active / cancel or terminate override, suppress toward-crossing movement`
- **Environment**: (B), needs `rlx_main 1`, `lx_main 1`, and `c_main`.
- **Setup**: Run TC-SC03A-2 first so L1 is in CENTRAL_OVERRIDE (target=connector, long duration, e.g. 60000ms to allow time to act).
- **Steps**:
  1. While the override is still active (e.g. at second 10/60), press `0` on RL1.
  2. Watch L1's log: `Lx 1: override cleared/expired - running safe clearance sequence` (from `lx_signal_show_override_clearance()`, called in `lx_fsm_terminate_override_locked()`) must appear **before or at the same time as** SUPERVISORY switching to RAILWAY_PREEMPTION.
- **Expected Result**: L1's SUPERVISORY on C1: `2` → `1` (no intermediate `3`). OVERRIDE returns to `0`. Behavior afterward matches TC-SC03A-1 (arterial keeps running, connector suppressed) — the override does **not** auto-resume once the crossing reopens (per SC-03A's note "an override interrupted by a train is never automatically resumed afterward").

### TC-SC03A-4: RAILWAY_PREEMPTION self-loop — a conflicting REQUEST_OVERRIDE is NACKed
- **Type**: Negative
- **Related**: SC-03A, `RAILWAY_PREEMPTION --> RAILWAY_PREEMPTION : conflicting REQUEST_OVERRIDE(CLEAR_ROUTE) / NACK`
- **Environment**: (B)
- **Setup**: L1 in RAILWAY_PREEMPTION (RL1 WARNING/CLOSING/CLOSED).
- **Steps**: On C1 press `o`, Lx=`1`, target=`1` (connector), duration=`10000`.
- **Expected Result**: `lx_fsm_on_request_override()` returns `RESULT_NACK`, `reply->reason = NACK_REASON_RAILWAY_CONFLICT` — C1 console shows the NACK (via `c_comm.c`'s reply logging). L1's SUPERVISORY unchanged, still `1`.

### TC-SC03A-5: RAILWAY_PREEMPTION → NORMAL_OPERATION on crossing OPEN, no queue-warning → no drain
- **Type**: Positive
- **Related**: SC-03A, `RAILWAY_PREEMPTION --> NORMAL_OPERATION : crossing reports OPEN and connector drain completes`; UC-05 alt 6.1 "no warning -> skip drain"
- **Environment**: (B) or (C)
- **Setup**: L1 in RAILWAY_PREEMPTION due to RL1. Make sure `w` was **not** pressed on L1 beforehand (`queue_warning_active=0`).
- **Steps**: Take no further action; wait for RL1 to naturally run through `WARNING→CLOSING→CLOSED→TRAIN_PRESENT→OPENING→OPEN` (~51s with no second train) → RL1 broadcasts `CROSSING_STATUS(OPEN)`.
- **Expected Result**: As soon as L1 receives `CROSSING_OPEN`, SUPERVISORY switches `1 → 3` (NORMAL_OPERATION) in `lx_fsm_on_crossing_status()`. `drain_pending` is **not** set (since `fsm->queue_warning_active==0`) — the next `CONNECTOR_GREEN` must run its normal duration (30s under PEAK_FIXED), no extension — confirmed by no extension logs and `CONNECTOR_YELLOW` appearing exactly 30s after `CONNECTOR_GREEN`.

### TC-SC03A-6: NORMAL_OPERATION/CENTRAL_OVERRIDE → FAULT_SAFE via watchdog; recovery via REQUEST_FAULT_CLEAR
- **Type**: Positive (entering FAULT_SAFE, and — now that `MSG_REQUEST_FAULT_CLEAR` is wired for Lx — exiting FAULT_SAFE is also Positive, no longer a Known gap)
- **Related**: SC-03A, `NORMAL_OPERATION --> FAULT_SAFE`, `CENTRAL_OVERRIDE --> FAULT_SAFE`, `FAULT_SAFE --> NORMAL_OPERATION : verified repair and accepted local fault-clear request`, `lx_fsm_on_request_fault_clear()`
- **Environment**: (B)
- **Setup/Steps — Part 1 (from CENTRAL_OVERRIDE)**:
  1. Put L1 into CENTRAL_OVERRIDE (as in TC-SC03A-2).
  2. Suspend `lx_main 1` with `kill -STOP <pid>` for >2s then `kill -CONT <pid>` to trigger the real watchdog (`lx_watchdog_thread`).
  3. Watch for `Lx: WATCHDOG - no phase-timer activity for 2 s, reporting fault (PA-10)`, then `Lx 1: override cleared/expired - running safe clearance sequence` (override terminated **before** entering FAULT_SAFE — a compliance-audit fix in `lx_fsm_report_watchdog_trip()`/`lx_fsm_check_fault_locked()`), then `Lx 1: entering FAULT_SAFE mode - holding safe outputs (all-red/dark)`.
- **Expected Result Part 1**: L1 SUPERVISORY: `2 → 0` directly (skipping `3`), OVERRIDE back to `0`.
- **Note**: As in TC-SC01A-3, whether `kill -STOP`/`kill -CONT` trips the PA-10 watchdog in Part 1's setup is disputed by `docs/test-plan/05-fault-safety.md`'s TC-FAULT-16 for the same mechanism (`lx_watchdog.c`) — TC-FAULT-16 argues `SIGSTOP` suspends every thread at once (including the watchdog thread), so it cannot trip. The correct answer depends on real QNX `SIGSTOP`/`SIGCONT` behavior, unverified on real hardware — both possibilities must be checked once hardware is available; this case's conclusion should not be treated as settled. **Part 1 must be recorded as Skip (not Pass)** in any results summary until real-hardware testing confirms the precondition — **Part 2 depends on Part 1 too** (needs L1 genuinely in `FAULT_SAFE` before testing recovery), so it must also be Skip until Part 1 is confirmed.
- **Steps — Part 2 (recovery via REQUEST_FAULT_CLEAR)**: **Update (fixed, no longer a known gap)** — `MSG_REQUEST_FAULT_CLEAR` is now handled for Lx too by `lx_main.c`'s `on_request()` (calls `lx_fsm_on_request_fault_clear()`), and `c_operator.c`'s `f` key now asks for `node type` (0=Lx, 1=RLx) before the node number — enter `f` → `0` → `1` to target L1. On C1: `f` → node type `0` → Lx number `1`.
- **Expected Result Part 2**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`. `lx_fsm_on_request_fault_clear()` is unconditional/idempotent (no physical condition to re-verify, unlike RLx's `gates_confirmed_open()`): always ACKs, clears `fsm->faults`, and L1's SUPERVISORY leaves `FAULT_SAFE`. Related re-audit safety fix (`last_crossing_state` in `lx_fsm.h`): if `fsm->last_crossing_state != CROSSING_OPEN` at clear time (adjacent crossing still closed/occupied), SUPERVISORY must resume `RAILWAY_PREEMPTION` (`1`), **not** `NORMAL_OPERATION` (`3`) — verify this variant by repeating Part 1 while RL1 is WARNING/CLOSED (railway preemption active) before tripping the watchdog, then clearing the fault while the crossing is still not OPEN: L1's SUPERVISORY after ACK must be `1`, not `3`, and CONNECTOR_GREEN stays suppressed until RL1 actually reports `OPEN`.

### TC-SC03A-7 (Critical regression, CC-03): Drain phase granted exactly once per railway reopening
- **Type**: Positive / Regression
- **Related**: SC-03A (RAILWAY_PREEMPTION→NORMAL_OPERATION edge) combined with CC-03, implemented in `lx_fsm_on_crossing_status()` (sets `drain_pending`) and `lx_fsm_advance_phase_locked()`/`lx_fsm_on_phase_timer()` (consumes `drain_pending`, runs `drain_active`/`drain_extending`)
- **Environment**: (B) or (C)
- **Setup**: L1 `MODE_PEAK_FIXED`, RL1 adjacent to L1. On L1 press `w` (set `queue_warning_active=1`) **while** RL1 is WARNING/CLOSED (RAILWAY_PREEMPTION active on L1).
- **Steps**:
  1. With `queue_warning_active=1`, wait for RL1 to reopen naturally (`CROSSING_OPEN`) → L1 receives it, `drain_pending=1` is set in `lx_fsm_on_crossing_status()`.
  2. Watch the **first** `CONNECTOR_GREEN` after that: it must run the normal full 30s (`LX_PEAK_CONNECTOR_GREEN_MS`), then — since `drain_active=1` — instead of switching to `CONNECTOR_YELLOW` immediately, it must extend in 4s increments (`LX_EXTENSION_MS`) **as long as** `queue_warning_active` stays 1.
  3. Do **not** press `W` (turn off queue warning) — let it run to the cap.
  4. Measure total extension time: it must stop exactly at `LX_DRAIN_MAX_EXTENSION_MS`=60000ms (15 increments of 4s), after which `CONNECTOR_GREEN -> CONNECTOR_YELLOW` fires even though `queue_warning_active` is still =1 (the `drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS` branch wins regardless of warning state).
  5. After the drain ends, let L1 run through the next cycle and (optionally) a **second** CONNECTOR_GREEN phase with **no** further RAILWAY_PREEMPTION→NORMAL_OPERATION transition in between.
- **Expected Result**:
  - Total CONNECTOR_GREEN duration for the drain cycle = 30s (base) + up to 60s (drain) = up to 90s, growing in 4s increments.
  - The **next** CONNECTOR_GREEN after that (step 5, with no intervening new RAILWAY_PREEMPTION) must return to the normal 30s — **no** further extension, since `drain_pending` is only ever set once at the `RAILWAY_PREEMPTION -> NORMAL_OPERATION` edge (never re-set during ordinary NORMAL_OPERATION even if `queue_warning_active` stays 1) — this is exactly the "exactly once per railway reopening" property to confirm.
  - Extra variant (time permitting): repeat the whole test but press `W` (turn off warning) mid-drain (e.g. after 12s of extension) — expect `CONNECTOR_GREEN -> CONNECTOR_YELLOW` at the very next 4s tick after turning it off (not waiting the full 60s), matching the `!fsm->queue_warning_active` branch.

---

## 6. SC-03B — Clear-Route Override Validation, Pending, and Renewal Detail

### TC-SC03B-1: override_validation → ACTIVE immediately (bounded, safe, no pedestrian clearance)
- **Type**: Positive
- **Related**: SC-03B, `override_validation --> ACTIVE : [bounded, safe, and no pedestrian clearance active] / ACK, apply at safe boundary`
- **Environment**: (B)
- **Setup**: L1 NORMAL_OPERATION, no `ped_clearance_active`.
- **Steps**: Press `o`, Lx=`1`, target=`0` (arterial), duration=`15000`.
- **Expected Result**: `lx_fsm_on_request_override()` returns `RESULT_ACK` (not `ACK_PENDING`) immediately; `override_substate` = `OVR_ACTIVE`; if L1 is currently in `PHASE_CONNECTOR_GREEN`, the phase timer waits for the nearest `ALL_RED_B_TO_A`/`ALL_RED_A_TO_B` boundary before forcing `PHASE_ARTERIAL_GREEN` (doesn't cut the current phase short — "apply at safe boundary").

### TC-SC03B-2: override_validation → OVERRIDE_PENDING during pedestrian clearance, auto-activates once clearance finishes
- **Type**: Positive (contested path: override request arrives exactly during ped clearance)
- **Related**: SC-03B, `override_validation --> OVERRIDE_PENDING` then `OVERRIDE_PENDING --> ACTIVE : pedestrian clearance completes [request remains safe and valid]`
- **Environment**: (B)
- **Setup**: Press a pedestrian button compatible with the current phase (e.g. `1` while L1 is ARTERIAL_GREEN) to get a running WALK (`ped_clearance_active=1`).
- **Steps**:
  1. During WALK/FDW (t≈2s into the 6s WALK), press `o`, Lx=`1`, target=`0`, duration=`20000`.
  2. Observe the reply: must be `RESULT_ACK_PENDING` (not `ACK`/`NACK`).
  3. Wait for `DONT_WALK` to appear (marks `ped_clearance_active` back to 0).
- **Expected Result**: On the tick right after `DONT_WALK` appears, `override_substate` switches `OVR_PENDING_CLEARANCE → OVR_ACTIVE` (in `lx_fsm_on_phase_timer()`, running **after** `lx_fsm_ped_service_tick_locked()` in the same tick — a verifier-audit fix to the call order). No second reply is sent to C1 (the initial ACK already covers it, per SC-03B's note "not repeated when the queued request later activates"). C1's OVERRIDE switches `0 → 1`.

### TC-SC03B-3: override_validation → NACK for an invalid duration
- **Type**: Negative
- **Related**: SC-03B, `override_validation --> [*] : [unsafe, unbounded, or conflicts with railway pre-emption] / NACK`
- **Environment**: (B)
- **Setup**: L1 NORMAL_OPERATION.
- **Steps**:
  1. Press `o`, Lx=`1`, target=`0`, duration=`0`.
  2. Press `o`, Lx=`1`, target=`0`, duration=`300001`.
- **Expected Result**: Both are rejected **at the Central layer** (`c_mode_eng_validate_override_request()` in `c_operator.c`, checking `duration_ms==0 || >300000` — exactly the condition `lx_fsm_on_request_override()` would also apply if the request reached Lx) — C1 console prints "rejected by Central pre-check, reason=INVALID_DURATION", the request is **never** forwarded to L1 (`c_comm_send_request_override()` never runs). This is defense-in-depth: the identical limit also exists independently in `lx_fsm_on_request_override()` (`payload->duration_ms == 0 || > LX_OVERRIDE_DURATION_CAP_MS`), but via the real operator console the Central layer always blocks first.

### TC-SC03B-4: OVERRIDE_PENDING discarded when it expires while still waiting (duration shorter than remaining ped clearance)
- **Type**: Edge case (2 near-simultaneous conditions: countdown expiry vs. clearance still running)
- **Related**: SC-03B, `OVERRIDE_PENDING --> [*] : request no longer valid, cancelled, or expires before application / discard request`
- **Environment**: (B)
- **Setup**: Trigger the longest possible ped clearance chain (WALK 6s + FDW 4s = 10s) right at the start of the current phase — press a compatible pedestrian button right as ARTERIAL_GREEN begins.
- **Steps**:
  1. Right after WALK starts (t≈0.5s), send `o`, Lx=`1`, target=`0`, duration=`3000` (much shorter than the ~9.5s of ped sequence remaining).
  2. Observe the reply: `RESULT_ACK_PENDING`.
  3. Track `override_remaining_ms` indirectly via logs: since `lx_fsm_on_phase_timer()` counts down `override_remaining_ms` in both `OVR_ACTIVE` **and** `OVR_PENDING_CLEARANCE` (a compliance-audit fix), the 3000ms countdown hits 0 at t≈3.0s — **before** `DONT_WALK` appears (t≈10.0s).
- **Expected Result**: At t≈3.0s, `lx_fsm_terminate_override_locked()` is called even while still `OVR_PENDING_CLEARANCE` — log `Lx 1: override cleared/expired - running safe clearance sequence` appears, `supervisory` returns to `NORMAL_OPERATION`, C1's OVERRIDE returns to `0` — **while the pedestrian WALK/FDW keeps running uninterrupted** (per the rule "pedestrian sequence itself is never truncated"). No second reply is sent to C1.

### TC-SC03B-5: renewal_validation — both valid/invalid branches, and NACK when there's no active override to renew
- **Type**: Positive + Negative (combined)
- **Related**: SC-03B, `renewal_validation --> ACTIVE : [bounded and safe] / ACK, restart override timer` and `[invalid, unsafe, or over limit] / NACK, retain current expiry`
- **Environment**: (B)
- **Setup — branch (a) valid**: L1 in `OVR_ACTIVE` with `duration_ms=20000`, ~10s elapsed (`override_remaining_ms≈10000`).
- **Steps (a)**: Press `r`, Lx=`1`, extend_duration_ms=`30000`.
- **Expected Result (a)**: `RESULT_ACK`; both `override_duration_ms` and `override_remaining_ms` are reset to `30000` (full restart, not additive) — the override now expires 30s from the **renewal moment**, not from the original start.
- **Steps (b) — invalid branch**: Right after (a), press `r`, Lx=`1`, extend_duration_ms=`400000` (>300000).
- **Expected Result (b)**: `RESULT_NACK`, `NACK_REASON_INVALID_DURATION`; `override_remaining_ms` **stays** at whatever it was counting down from in (a) (not reset to 0 or altered) — per "retain current expiry".
- **Steps (c) — no override to renew**: On another Lx with no active override (e.g. L2), press `r`, Lx=`2`, extend_duration_ms=`10000`.
- **Expected Result (c)**: `lx_fsm_on_renew_override()` checks `fsm->supervisory != SUPERVISORY_CENTRAL_OVERRIDE || fsm->override_substate != OVR_ACTIVE` → `RESULT_NACK`, `NACK_REASON_UNKNOWN_TARGET`. (Renewing an override that's still `OVR_PENDING_CLEARANCE` must also NACK with the same reason — can additionally test by renewing during the TC-SC03B-2 scenario before clearance finishes.)

### TC-SC03B-6 (Critical regression): override forces the real green light in the right direction, holds it for the full duration, then returns to the normal cycle through proper yellow/red
- **Type**: Positive / Regression
- **Related**: SC-03B "ACTIVE" note ("Bounded and auto-expiring") combined with SC-01B/C — implemented in the `SUPERVISORY_CENTRAL_OVERRIDE && OVR_ACTIVE` branches of `lx_fsm_on_phase_timer()` (cases `PHASE_ARTERIAL_GREEN`/`PHASE_CONNECTOR_GREEN`, holding green) and `lx_fsm_advance_phase_locked()` (forcing the correct `override_target_movement` at the `ALL_RED_A_TO_B`/`ALL_RED_B_TO_A` boundaries)
- **Environment**: (B)
- **Setup**: Wait for L1 to be in `PHASE_CONNECTOR_GREEN` (the "wrong" direction relative to the override about to be sent).
- **Steps**:
  1. During `CONNECTOR_GREEN`, press `o`, Lx=`1`, target=`0` (**arterial** — opposite the current direction), duration=`15000`.
  2. Watch: `CONNECTOR_GREEN` must run its normal course to `CONNECTOR_YELLOW` (4s) → `ALL_RED_B_TO_A` (2s) — the override doesn't interrupt the phase/clearance already showing.
  3. At the end of `ALL_RED_B_TO_A`, observe the next phase.
  4. Time how long L1 stays continuously in `ARTERIAL_GREEN`.
  5. 15s after the override was ACKed (not from when ARTERIAL_GREEN started — `override_remaining_ms` counts from ACK), observe what happens.
- **Expected Result**:
  - Step 3: `lx_fsm_advance_phase_locked()`'s `PHASE_ALL_RED_B_TO_A` case always picks `PHASE_ARTERIAL_GREEN` — true both with and without an override at this boundary (nothing distinguishing here), so the real test is at the `PHASE_ALL_RED_A_TO_B` boundary afterward.
  - `ARTERIAL_GREEN` does not self-exit at 48s as normal PEAK_FIXED would — the branch `if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && ... && override_target_movement == OVERRIDE_MOVEMENT_ARTERIAL) { break; }` skips the normal exit-check entirely, holding ARTERIAL_GREEN **until** `override_remaining_ms` hits 0.
  - Exactly 15s after ACK, log `Lx 1: override cleared/expired - running safe clearance sequence` appears, SUPERVISORY returns to `3`, and **right after that** the phase is still `ARTERIAL_GREEN` (lx_fsm doesn't force a phase change on override expiry — only supervisory changes) — it then resumes the normal PEAK_FIXED exit-check (up to 48s total from when ARTERIAL_GREEN began, **not** from override expiry) before going `ARTERIAL_YELLOW → ALL_RED_A_TO_B → CONNECTOR_GREEN` as a normal cycle — i.e. always through proper yellow/red, never a direct jump.
  - Extra required variant: repeat the whole test but send two consecutive `REQUEST_OVERRIDE`s for the same L1 without cancelling/expiring the first — the second must get `RESULT_NACK`/`NACK_REASON_OUT_OF_RANGE` (the `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE` branch at the top of `lx_fsm_on_request_override()` — a compliance-audit fix preventing a second override from silently overwriting the first's target/duration).

---

## 7. SC-04A — Railway Crossing Approach and Closure

### TC-SC04A-1: OPEN → WARNING on train approach
- **Type**: Positive
- **Related**: SC-04A, `OPEN --> WARNING : TRAIN_APPROACHING(direction) / activate flashers, notify Lx and C1, register occupancy window`
- **Environment**: (A) single `rlx_main 1` is enough to see internal logs; use (B) to confirm Lx/C1 notification.
- **Setup**: RL1 in `RLX_OPEN`.
- **Steps**: Press `0`.
- **Expected Result**: Log `RLx: flashers ON (train approaching, direction 0)`. In (B): L1 and L2 receive `MSG_CROSSING_STATUS(WARNING)`; if `c_main` is running, RL1's CROSSING_STATE on C1's table goes `0 → 1`.

### TC-SC04A-2: WARNING self-loop — a second direction approaches while already WARNING
- **Type**: Positive
- **Related**: SC-04A, `WARNING --> WARNING : TRAIN_APPROACHING(other direction) / register additional occupancy window`
- **Environment**: (A)
- **Setup**: RL1 just entered WARNING via `0` (t=0).
- **Steps**: At t≈2s, press `1`.
- **Expected Result**: `register_window()` creates a second window (direction 1) **without** resetting `state_elapsed_ms` (the counter toward `RLX_WARNING_TO_CLOSING_MS` still counts from t=0, not t=2s) — `CLOSING` must still begin exactly at t≈5.0s (not pushed to t≈7.0s).

### TC-SC04A-3: WARNING → CLOSING at exactly the 5000ms boundary
- **Type**: Edge case
- **Related**: SC-04A, `WARNING --> CLOSING : after 5 s / command both gates down`
- **Environment**: (A)
- **Setup**: RL1 OPEN.
- **Steps**: Press `0`, time until the log `RLx: commanding gates DOWN (simulated motion, 3000 ms)` appears.
- **Expected Result**: Difference ≈5.0s (±1 tick of 1000ms, since the railway tick cycle is 1s, not 100ms like Lx). *Known-gap note:* the transition `WARNING --> FAULT : approach input remains active beyond diagnostic timeout` (60s, `RLX_WARNING_DIAGNOSTIC_TIMEOUT_MS`) **cannot occur** in the current setup — a comment in `rlx_fsm_on_tick()` confirms this is dead code, since `RLX_WARNING_TO_CLOSING_MS` (5s) always fires first and resets `state_elapsed_ms` every time `enter_closing()` runs; the discrete simulated `TRAIN_APPROACHING` event also can't represent a sensor "stuck continuously active." No positive test case is possible (or needed) for this branch — confirming WARNING always goes to CLOSING at 5s as above suffices as a "negative control" for this FAULT branch.

### TC-SC04A-4: CLOSING → CLOSED when both gates confirm closed before the deadline
- **Type**: Positive
- **Related**: SC-04A, `gate_confirmation --> CLOSED : [both gates confirmed CLOSED before deadline] / set train signal(s) for registered approaches to PROCEED`
- **Environment**: (A)
- **Setup**: RL1 OPEN, `x` not pressed (no demo fault armed).
- **Steps**: Press `0`, wait through WARNING(5s)→CLOSING.
- **Expected Result**: At t≈5+3=8.0s (5s warning + `RLX_GATE_MOTION_MS`=3000ms simulated motion), logs appear in order: `RLx: commanding gates DOWN...` (t≈5.0s) then `RLx: train signal PROCEED for direction 0 (gates confirmed closed)` (t≈8.0s) — well before the 15s deadline (`RLX_CLOSING_DEADLINE_MS`, measured from entering CLOSING, so the real deadline is t≈20.0s).

### TC-SC04A-5 (Regression): CLOSING → FAULT on missing gate confirmation at deadline — fault forces gates closed for real
- **Type**: Negative / Regression
- **Related**: SC-04A, `gate_confirmation --> FAULT : [confirmation missing or contradictory at deadline] / hold STOP and report fault`; `enter_fault()` calls `rlx_gate_command_close()` (audit fix — previously fault only latched a flag without forcing the gate closed)
- **Environment**: (A)
- **Setup**: RL1 OPEN.
- **Steps**:
  1. Press `x` (arm demo fault for the next motion cycle).
  2. Press `0` → WARNING → after 5s CLOSING, `rlx_gate_command_close()` runs (log `commanding gates DOWN`), but because the fault is armed, `rlx_gate_on_tick()` never sets `g_confirmed_closed=1` after 3000ms — instead logs `RLx: gate FAILED TO CONFIRM (simulated fault) ...`.
  3. Wait until exactly `RLX_CLOSING_DEADLINE_MS`=15000ms from entering CLOSING (t≈5+15=20.0s from pressing `0`).
- **Expected Result**: Exactly at t≈20.0s, `check_closing_or_reclosing_complete()` sees `state_elapsed_ms >= 15000` and `gates_confirmed_closed()==0` → calls `enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING)`. The log `RLx: commanding gates DOWN (simulated motion, 3000 ms)` must appear **one more time** (since `enter_fault()` unconditionally calls `rlx_gate_command_close()` again — this is the regression to confirm: before the fix, fault never forced the gate closed for real, potentially leaving it hanging). Followed by `RLx: FAULT latched (GATE_CONFIRM_MISSING, bit 0x1) - holding STOP on all train signals, commanding gates DOWN`. CROSSING_STATE on C1 (if running) = `3` (FAULT).

---

## 8. SC-04B — Railway Crossing Occupancy and Reopening

### TC-SC04B-1: CLOSED → TRAIN_PRESENT at expected arrival time, gates still confirmed closed
- **Type**: Positive
- **Related**: SC-04B, `CLOSED --> TRAIN_PRESENT : expected arrival time reached [gates remain confirmed CLOSED] / mark occupancy window active`
- **Environment**: (A)
- **Setup**: Repeat TC-SC04A-4 to bring RL1 to `RLX_CLOSED` (t≈8.0s from pressing `0`).
- **Steps**: Take no action, wait `RLX_EXPECTED_ARRIVAL_MS`=20000ms from entering CLOSED.
- **Expected Result**: No clear log marks this transition (`enter_train_present()` prints nothing besides internal state changes), but it can be inferred indirectly: the external CROSSING_STATE (via `map_to_crossing_state()`) **stays** `CROSSING_CLOSED` (both `RLX_CLOSED` and `RLX_TRAIN_PRESENT` map to `CROSSING_CLOSED`) — verify by observing the occupancy window: press `0` again right after the 20s mark (t≈28s) and confirm `RLx: train signal PROCEED for direction 0 ...` appears immediately (the `RLX_TRAIN_PRESENT` branch in `rlx_fsm_simulate_train_approaching()` calls `register_window(..., RLX_OCCUPANCY_WINDOW_MS)` directly and logs PROCEED right away, same as the `RLX_CLOSED` branch — so distinguishing them reliably needs the extra wait and timing check).
  *Note*: this transition is hard to observe directly via logs — consider adding a temporary debug line if more conclusive evidence than timing inference is needed.

### TC-SC04B-2: TRAIN_PRESENT → OPENING only when ALL occupancy windows expire (not a single window)
- **Type**: Edge case (2 staggered occupancy windows)
- **Related**: SC-04B, `TRAIN_PRESENT --> OPENING : all occupancy windows expire`; RC-04 invariant in `rlx_fsm_on_tick()` (`if (fsm->active_window_count == 0)`)
- **Environment**: (A)
- **Setup**: Bring RL1 to TRAIN_PRESENT with **one** direction-0 occupancy window running (per TC-SC04B-1, direction-0 window counts 20s from entering TRAIN_PRESENT).
- **Steps**:
  1. Right at TRAIN_PRESENT (t=0 for this scenario), at t≈10s press `1` (register an additional direction-1 window — since already `RLX_TRAIN_PRESENT`, this window immediately gets `remaining_ms=RLX_OCCUPANCY_WINDOW_MS`=20000ms, so it expires at t≈30s).
  2. Observe at t≈20s (direction-0 window expires) — RL1 must **not** enter OPENING.
  3. Keep watching until t≈30s.
- **Expected Result**: At t≈20s, `active_window_count` drops from 2 to 1 (only direction-0 window closes), state **stays** `RLX_TRAIN_PRESENT` — the log `RLx: commanding gates UP...` does **not** appear at this point. Only at t≈30s, once direction-1's window also expires and `active_window_count==0`, does `enter_opening()` run → logs `RLx: all train signals -> STOP (crossing reopening)` then `RLx: commanding gates UP (simulated motion, 3000 ms)` appear exactly at t≈30s, not earlier.

### TC-SC04B-3: OPENING → RECLOSING when a new train approaches mid-opening
- **Type**: Positive (2 near-simultaneous conditions: gate mid-open + new train appears)
- **Related**: SC-04B, `OPENING --> RECLOSING : TRAIN_APPROACHING(direction) / stop opening, keep flashers active, register occupancy window, command gates down`
- **Environment**: (A)
- **Setup**: Bring RL1 to `RLX_OPENING` (following TC-SC04B-2, gates start opening at t≈30s, needing 3000ms to confirm — expected confirmation at t≈33s).
- **Steps**: At t≈31s (mid-open, not yet confirmed), press `0`.
- **Expected Result**: Log `RLx: reclosing - aborting gate-open motion, flashers remain active` appears immediately; right after, `RLx: commanding gates DOWN (simulated motion, 3000 ms)` (calls `rlx_gate_command_close()` again, aborting the in-progress open motion). No `RLx: flashers OFF` appears in between (flashers must stay active throughout, per "keep flashers active"). About 3s later (t≈34s), `reclose_confirmation` → `RLx: train signal PROCEED for direction 0 ...` → state returns to CLOSED (mapped to CROSSING_CLOSED, same as after a normal CLOSING).

### TC-SC04B-4: OPENING → FAULT on missed open confirmation, then recovery via FAULT → OPEN
- **Type**: Negative (entering FAULT) + Positive (exiting FAULT — a real recovery path, fully testable via keyboard, unlike Lx)
- **Related**: SC-04B, `OPENING --> FAULT : gates fail to confirm OPEN / hold last confirmed safe outputs and report fault`; `FAULT --> OPEN : verified repair and accepted local fault-clear request [crossing safe]`
- **Environment**: (A) is enough to reach FAULT (via the `x` demo key); needs (B) (`c_main` + `rlx_main 1`) for the real recovery in step 6, since `MSG_REQUEST_FAULT_CLEAR` is only sent from `c_operator.c`.
- **Setup**: Bring RL1 to just before OPENING (e.g. stop at the end of TC-SC04B-2, right before t≈30s).
- **Steps**:
  1. Before the last occupancy window expires, press `x` (arm demo fault for the upcoming open motion).
  2. Wait for the window to expire → `enter_opening()` runs, `rlx_gate_command_open()` is called, but due to the armed fault, open is never confirmed (`g_confirmed_open` stays 0).
  3. Wait the full `RLX_OPENING_DEADLINE_MS`=15000ms from entering OPENING.
  4. After FAULT appears (`enter_fault()` calls `rlx_gate_command_close()` again — same regression as TC-SC04A-5), **try** `f` (demo fault-clear) immediately, without waiting for the gate to confirm closed.
  5. Wait the full 3000ms for the gate to finish closing (since `enter_fault()` just commanded it closed), then try `f` again — still must be rejected, since `gates_confirmed_open()` has never actually been 1 since entering FAULT.
  6. On the RL1 console (in `RLX_FAULT`, gate now `confirmed CLOSED`), press `r` to call `rlx_gate_force_confirmed_open()` (`rlx_gate.c`) — simulating "gate mechanism just repaired and confirmed open again," forcing `g_confirmed_open=1`/`g_confirmed_closed=0`. Then, on the C1 console, press `f` → node type `1` (RLx) → number `1` to send a real `MSG_REQUEST_FAULT_CLEAR` via IPC to RL1.
- **Expected Result**:
  - Step 3: log `RLx: FAULT latched (GATE_CONFIRM_MISSING, bit 0x1) ...` appears exactly at t≈15s from entering OPENING.
  - Steps 4/5 (gate currently CLOSED due to fault, never actually OPEN): `rlx_fsm_on_fault_clear()` returns `RESULT_NACK`, `NACK_REASON_FAULT_ACTIVE` (log `[rlx_sensor] fault-clear result=... reason=...`) — **as designed**, since RC-10 requires verifying the gate is genuinely safe (confirmed open) before accepting fault-clear, and a FAULT crossing with a closed gate obviously isn't "verified repaired" yet.
  - Step 6: right after pressing `r`, log `[DEMO] Gate mechanism simulated as physically repaired - now confirmed OPEN (RC-09/RC-10 fault-clear demo path)` appears on the RL1 console. The `MSG_REQUEST_FAULT_CLEAR` sent from C1 afterward must get `RESULT_ACK` (no longer NACK) — since `gates_confirmed_open()` is now 1 — `central_log.txt` logs `C1: REQUEST_FAULT_CLEAR to <RL1> -> ACK`; `fsm->faults` returns to `FAULT_NONE`, RL1 leaves `RLX_FAULT`, RL1's CROSSING_STATE on C1's table returns to `0` (OPEN). This is a **real, positive, fully keyboard-verifiable** `FAULT --> OPEN` transition (via RL1's `r` key + C1's `f` key) — **no longer a "known gap"** as this document previously assessed (the `r` key/`rlx_gate_force_confirmed_open()` is exactly the "force confirmed open" API the earlier assessment claimed didn't exist).

### TC-SC04B-5 (Known gap, brief note): CLOSED/TRAIN_PRESENT → FAULT due to "gate state contradicts CLOSED" cannot be triggered with current demo tools
- **Type**: Negative / Known gap
- **Related**: SC-04B, `CLOSED --> FAULT : gate state contradicts CLOSED`, `TRAIN_PRESENT --> FAULT : gate state contradicts CLOSED`
- **Environment**: (A)
- **Note in place of steps**: `check_gate_contradiction_closed()` only reports a fault when `gates_confirmed_closed()==0` while state is CLOSED/TRAIN_PRESENT. But `rlx_gate.c` only changes `g_confirmed_closed` via `rlx_gate_command_close()`/`rlx_gate_command_open()`, both only ever called from `rlx_fsm.c`'s known logic (no open command ever runs while CLOSED/TRAIN_PRESENT). So **no key/IPC sequence in the current build** can make the gate "naturally" contradict CLOSED while the state is still CLOSED — the same kind of gap as SC-04A's "WARNING→FAULT via timeout." Recommendation: a genuine test would need a new demo API like `rlx_gate_force_open_for_test()` — doesn't currently exist, so **no fabricated positive test case is written for this branch**.

---

## 9. SC-05 — Central Connectivity and Local Autonomy

### TC-SC05-1: PA-07 — exactly 3 consecutive missed heartbeats before marking UNAVAILABLE
- **Type**: Positive + Edge case (boundary at exactly the 3rd miss, not the 2nd)
- **Related**: SC-05, `CENTRAL_CONNECTED --> DEGRADED_LOCAL : three consecutive 1 s heartbeats missed`
- **Environment**: (B)
- **Setup**: `c_main` and `lx_main 1` running normally, L1 already shows `AVAILABLE` on C1's table.
- **Steps**:
  1. Note `lx_main 1`'s pid, send `kill -STOP <pid>` (fully suspends the process — its 1Hz heartbeat stops completely).
  2. Watch C1's table every second (auto-refreshes at 1Hz): right after 1 second (`missed_heartbeat_ticks=1`) and 2 seconds (`=2`), L1's AVAILABILITY column **must still be** `AVAILABLE`.
  3. Exactly at the 3rd second (`missed_heartbeat_ticks==3`), watch for the Central log `Controller 1 marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)` (also written to `central_log.txt`), and the AVAILABILITY column switching to `UNAVAILABLE`.
  4. `kill -CONT <pid>` to clean up the process for later tests.
- **Expected Result**: As above — the transition boundary is the 3rd miss, not earlier (edge case: if UNAVAILABLE appeared at the 2nd miss, that would violate PA-07's "three consecutive").

### TC-SC05-2: DEGRADED_LOCAL — Lx/RLx keep operating fully normally with no heartbeat reaching Central
- **Type**: Positive / Regression note
- **Related**: SC-05, note "Connectivity loss alone never forces all-red, FLASHING_RED, or frozen timing — traffic, pedestrian, railway, and fault logic (SC-01 through SC-04) all continue locally"
- **Environment**: (B)
- **Setup**: Similar to TC-SC05-1 — `lx_main 1` being `kill -STOP`ed looks like a "disconnect"/UNAVAILABLE from Central's view, but **this time don't actually STOP it**, since L1 needs to keep running to observe: instead, simply **don't run `c_main` at all** for the whole test (environment (A) alone also proves the point, since `lx_fsm.c` never reads `fsm->link_state` in any SC-01/02/03/04 transition).
- **Steps**: Run `lx_main 1` alone (no `c_main`), press normal sensor keys (`a`, `1`, `w`, etc.) and watch the full ARTERIAL/CONNECTOR cycle and WALK/FDW run with correct timing, same as the tests in sections 1-4 of this document.
- **Expected Result**: No behavioral difference at all compared to running with `c_main` — directly confirmed by source: no `if (fsm->link_state == ...)` check appears anywhere in `lx_fsm.c` governing phase/pedestrian/override/railway-preemption logic. This meets SC-05's note "by construction," not via any dedicated fallback mechanism.

### TC-SC05-3: Instant resync on heartbeat recovery — RESYNCHRONISING modeled via a single full heartbeat
- **Type**: Positive
- **Related**: SC-05, `DEGRADED_LOCAL --> RESYNCHRONISING --> CENTRAL_CONNECTED`
- **Environment**: (B)
- **Setup**: Repeat TC-SC05-1 until L1 = `UNAVAILABLE`.
- **Steps**:
  1. `kill -CONT <pid lx_main 1>` so L1 resumes and automatically resends `MSG_HEARTBEAT` (1Hz, no extra action needed from L1).
  2. Watch C1's table at the next 1Hz refresh after the first heartbeat arrives.
- **Expected Result**: `c_server_record_status()` (called from the `MSG_HEARTBEAT` case in `c_main.c`) resets `missed_heartbeat_ticks=0` and `marked_unavailable=0` **on the very first heartbeat received** — AVAILABILITY jumps straight `UNAVAILABLE → AVAILABLE` in a single tick, with no intermediate "resyncing" state shown. This is how SC-05's `RESYNCHRONISING` is modeled: since `heartbeat_payload_t` already reuses the full shape of `status_report_payload_t` (PA-08's "complete current state"), the first ACKed heartbeat after a disconnect already IS the "send full state" step — no separate intermediate step is needed. **(Fixed in the most recent audit — previously `link_state` in the payload was hard-coded to `LINK_CENTRAL_CONNECTED` in both `lx_comm.c` and `rlx_comm.c` regardless of actual connection state; see TC-SC05-4 for the test case directly confirming this fix.)**

### TC-SC05-4 (Regression, PA-07/PA-08): `link_state` now reflects reality, no longer hard-coded — confirmed directly via the outgoing payload
- **Type**: Regression (fixed)
- **Why it was a bug**: Before the latest audit/fix, `lx_comm_send_heartbeat()`/`rlx_comm_send_heartbeat()` always overwrote `req.payload.heartbeat.summary.link_state = (uint32_t)LINK_CENTRAL_CONNECTED;` regardless of the real `fsm->link_state` — meaning the on-wire `link_state` field **always lied** as connected, even while Lx/RLx was genuinely in `DEGRADED_LOCAL`. `fsm->link_state` was also only ever set once at init, never updated afterward.
- **Related**: `lx_fsm_on_heartbeat_result()` (`lx_fsm.c`), `rlx_fsm_on_heartbeat_result()` (`rlx_fsm.c`) — new functions called from the heartbeat reply callback (`on_heartbeat_reply()` in `lx_comm.c`/`rlx_comm.c`, running on the client thread); `lx_fsm_fill_status()`/`rlx_fsm_fill_status()` now copy the real `fsm->link_state` into the payload instead of hard-coding it.
- **Environment**: (B)
- **Setup**: `c_main` and `lx_main 1` running normally, already connected (`AVAILABLE`).
- **Steps**:
  1. `kill -STOP <pid c_main>` (fully suspend Central — L1 will get no `RESULT_ACK` for subsequent heartbeats, since `MsgSend()` will fail/get no reply).
  2. Watch `lx_main 1`'s log (not C1's, since C1 is STOPped): after exactly 3 consecutive unacknowledged heartbeats, expect `Lx: 3 consecutive HEARTBEATs unacknowledged - entering DEGRADED_LOCAL (PA-07)`.
  3. `kill -CONT <pid c_main>` to resume Central.
  4. Keep watching `lx_main 1`'s log: at the very next ACKed heartbeat, expect `Lx: HEARTBEAT acknowledged by C1 - reconnected, resuming CENTRAL_CONNECTED (PA-08)`.
- **Expected Result**: Both log lines above must appear exactly as described — direct proof that `fsm->link_state` (and hence the `link_state` field in every `MSG_HEARTBEAT` sent afterward) now reflects the real connection state, no longer hard-coded. If these two lines don't appear at the right time, that's a regression of this very fix.

---

## Appendix — Summary of Known Gaps Found While Writing This Test Plan

| Gap | Chart | Location in code | Impact |
|---|---|---|---|
| ~~No keyboard/IPC recovery path from `FAULT_SAFE -> NORMAL_OPERATION` for Lx~~ (FIXED) | SC-03A | `MSG_REQUEST_FAULT_CLEAR` is now handled by `lx_main.c`'s `on_request()`, calling `lx_fsm_on_request_fault_clear()`; `c_operator.c`'s `f` key now asks for node type (0=Lx/1=RLx) | No longer a gap — see TC-SC03A-6 Part 2. Additional re-audit fix: resumes `RAILWAY_PREEMPTION` correctly instead of always `NORMAL_OPERATION` if the adjacent crossing isn't yet `OPEN` at clear time (`fsm->last_crossing_state`) |
| ~~RLx's `FAULT --> OPEN` branch unreachable with `RESULT_ACK` by any sequence~~ (FIXED/REASSESSED) | SC-04B | `rlx_sensor.c`'s `r` key calls `rlx_gate_force_confirmed_open()` (`rlx_gate.c`) — exactly the "force confirmed open" API the earlier assessment claimed didn't exist | No longer a gap — see TC-SC04B-4 step 6: `r` on RL1 then `f` on C1 (select RLx) yields a real `RESULT_ACK`, fully keyboard-verifiable |
| `WARNING --> FAULT` (60s diagnostic timeout) cannot be triggered | SC-04A | `RLX_WARNING_TO_CLOSING_MS` (5s) always fires first and resets `state_elapsed_ms`; the discrete simulated event can't represent a sensor "stuck continuously active" | Dead code per the comment in `rlx_fsm.c` itself |
| `CLOSED/TRAIN_PRESENT --> FAULT` due to gate contradiction cannot be triggered with current demo tools | SC-04B | `rlx_gate.c` only changes confirm state via commands issued by `rlx_fsm.c` itself | Would need a new demo API to test for real |
| `REQUEST_LATCHED` "stuck-active beyond diagnostic timeout" (PA-03) is never detected | SC-02 | `lx_fsm_latch_pedestrian_request()` has a "KNOWN LIMITATION" comment confirming `FAULT_PED_BUTTON_STUCK` is never set | No positive test written for this branch in this document |
| ~~`link_state` always hard-coded `LINK_CENTRAL_CONNECTED` in every outgoing heartbeat~~ (FIXED) | SC-05 | `lx_fsm_on_heartbeat_result()`/`rlx_fsm_on_heartbeat_result()` now track real ACK/miss state; `lx_comm.c`/`rlx_comm.c`'s `on_heartbeat_reply()` calls into it instead of overwriting a hard-coded value | No longer a gap — see TC-SC05-4 |

Total test cases in this document: **43** (counting nested variants/edge cases within some TCs — incremented by 1 after adding TC-SC05-4 as a regression case for the `link_state` fix).
