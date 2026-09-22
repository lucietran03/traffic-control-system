# Test Plan — Functional Test Cases by Use Case (UC-01 .. UC-10)

This document lists the functional test cases for each use case described in
`usecase.md` (section 3.2), cross-checked directly against the code
currently in `app/` (branch `main`, commit `b8c1ac8`). Every action
(keypress, operator command, numeric prompt input) is taken verbatim from:

- `app/intersection/src/lx_sensor.c` — Lx sensor keyboard simulator.
- `app/railway/src/rlx_sensor.c` — RLx sensor keyboard simulator.
- `app/central/src/c_operator.c` — C1's operator command console.
- `app/central/src/c_hmi.c`, `app/central/src/c_logger.c`,
  `app/central/src/c_comm.c` — log/status-table formatting used to confirm
  expected results.
- `app/intersection/includes/lx_timer.h`, `app/railway/includes/rlx_timer.h`,
  `app/railway/includes/rlx_gate.h`, `app/shared/includes/sys_types.h`,
  `app/shared/includes/ipc_msg.h` — the exact timing constants and enum
  codes used in "Expected Result".

The team has a real QNX machine, so test cases requiring multiple
processes/nodes are written in full, without avoidance — including cases
that require real waiting (e.g. the 48s/30s signal cycle, or the ~50s
railway cycle).

## 1. Three test-environment conventions

| Symbol | Meaning | Example command |
|---|---|---|
| **(A)** | A single QNX node — runs only 1 binary, no other binary needs to be running. | `/tmp/lx_main 1` (L1 only, no other C1/RLx running) |
| **(B)** | Multiple nodes on the **same** QNX machine — multiple binaries run at once on the same target, Qnet same-node (`name_open()` resolves locally by default). **No need to set** `TRAFFIC_NODE_MAP`. | open several shells on the same target: `/tmp/c_main`, `/tmp/lx_main 1`, `/tmp/rlx_main 1` |
| **(C)** | Multiple nodes across **multiple** real QNX machines/VMs over the Qnet network — must export `TRAFFIC_NODE_MAP` on each shell before running the corresponding binary. See `app/shared/README.md` section "Cross-node resolution" and `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` section 2.2. | `export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,...,rl1=VM_x86_Target03,..."` then run the matching binary on the corresponding VM |

Note on (B): the actual demo deployment uses 10 separate QNX VMs, one
controller per VM — no two controllers ever really run on the same
machine. So every test case labeled (B) in this document is really a
scaled-down substitute for a genuine (C) environment (with
`TRAFFIC_NODE_MAP`) — stated explicitly here rather than left implicit.

Every test case below states clearly which environment (A)/(B)/(C) is
required. Most use (A)/(B), which is enough to observe the behavior under
test; (C) is only used when the test is fundamentally about losing the
Qnet connection between different physical machines — under (B), "losing
connection" is simulated by **not starting** or **killing** the relevant
process, and the observable effect (missed heartbeat, `name_open()`
failure) is identical to (C).

Recommended startup order per `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` section
2.3: **C1 first → RLx → Lx last**. Binaries built with `make` live at
`build/bin/{c_main,lx_main,rlx_main}`; on a deployed target they live at
`/tmp/{c_main,lx_main,rlx_main}` — the steps below use the `/tmp/...` path
but it can be swapped for `build/bin/...` when running locally.

## 2. How to read a test case

```
### TC-EXAMPLE-N: <short name> (illustrative template, NOT a real case — not counted in the total case count)
- **Type**: Positive / Negative / Edge case
- **Related**: UC-xx <name>, which step of the main/alt flow (per usecase.md)
- **Environment**: (A)/(B)/(C) — see section 1
- **Setup**: initial state, which node must run first, where
- **Steps**: keypresses / operator commands / numeric prompt input in exact order
- **Expected Result**: exact log line / state to observe
```

Keyboard shortcut conventions (for quick reference while reading "Steps",
taken verbatim from code — do not invent extra keys):

- **Lx sensor** (`lx_sensor.c`, runs inside the `lx_main` process): `a`/`A`
  = arterial-lane vehicle arrives/leaves, `c`/`C` = connector-lane vehicle
  arrives/leaves, `1`/`2`/`3`/`4` = pedestrian push-button on side 0/1/2/3,
  `w`/`W` = queue warning on/off, `h`/`?` = help, `q` = stop the
  keyboard-reading thread (does not stop the process).
- **RLx sensor** (`rlx_sensor.c`, runs inside the `rlx_main` process):
  `0`/`1` = train approaching direction 0/1 (`TRAIN_APPROACHING`), `x` =
  arm a gate-confirmation fault for the next close/open cycle (RC-06
  demo), `r` = simulate the gate mechanism having just been repaired,
  forcing confirmation to `CONFIRMED OPEN` immediately
  (`rlx_gate_force_confirmed_open()` in `rlx_gate.c`: sets
  `g_confirmed_open=1`, `g_confirmed_closed=0`, cancels any armed demo
  fault — demo for the fault-clear branch RC-09/RC-10), `f` = trigger a
  local fault-clear (demo only, does not go through the real
  `MSG_REQUEST_FAULT_CLEAR` wire), `h`/`?` = help, `q` = stop the
  keyboard-reading thread.
- **C1 operator console** (`c_operator.c`, runs inside the `c_main`
  process): `m` = `SET_MODE`, `t` = broadcast `SET_TIMING_PROFILE` to
  R1/R2, `o` = `REQUEST_OVERRIDE`, `r` = `RENEW_OVERRIDE`, `c` =
  `CANCEL_OVERRIDE`, `f` = `REQUEST_FAULT_CLEAR` (can target either Lx or
  RLx — `handle_request_fault_clear()` first asks `node type`: 0=Lx 1-6,
  1=RLx 1-3; wired to `lx_fsm_on_request_fault_clear()` on the Lx side),
  `d` = force a simulated hour as the "current" mark (`handle_demo_hour()`,
  prompt "  simulated hour to force (0-23): ") — sets
  `demo_hour_override_active=1` then broadcasts `SET_MODE` to every Lx per
  the mode implied by that hour, used to test the peak/off-peak boundary
  (DP-01/DP-02) on demand instead of waiting for the real clock, `a` =
  return to automatic mode driven by the real clock
  (`handle_resume_automatic()`) — only clears the
  `demo_hour_override_active` flag, does not broadcast anything itself;
  `c_main`'s next 1Hz tick reconciles and resyncs automatically if the
  mode implied by the real hour differs from the last mode sent, `h`/`?` =
  help, `q` = stop the console.
  Each letter command prints 1-2 numeric prompts in the exact order wired
  into the corresponding `handle_*()` — recorded precisely in each test
  case.

Adjacency table (which RLx sits next to which Lx), taken from
`app/railway/src/rlx_comm.c` (`ADJACENCY[]`), used in the UC-04/UC-05/UC-08/UC-10 tests:

| RLx | Adjacent Lx (both sides) |
|---|---|
| RL1 | L1, L2 |
| RL2 | L3, L4 |
| RL3 | L5, L6 |

Timing constants table used throughout (not re-explained per test case):

| Constant | Value | Source |
|---|---|---|
| `LX_PEAK_ARTERIAL_GREEN_MS` | 48000 ms | `lx_timer.h` |
| `LX_PEAK_CONNECTOR_GREEN_MS` | 30000 ms | `lx_timer.h` |
| `LX_YELLOW_MS` | 4000 ms | `lx_timer.h` |
| `LX_ALL_RED_MS` | 2000 ms | `lx_timer.h` |
| `LX_MIN_GREEN_MS` / `LX_MAX_GREEN_MS` | 8000 / 40000 ms | `lx_timer.h` |
| `LX_EXTENSION_MS` | 4000 ms | `lx_timer.h` |
| `LX_WALK_MS` / `LX_FLASHING_DONT_WALK_MS` | 6000 / 4000 ms | `lx_timer.h` |
| `LX_DRAIN_MAX_EXTENSION_MS` | 60000 ms | `lx_timer.h` |
| `LX_OVERRIDE_DURATION_CAP_MS` | 300000 ms | `lx_timer.h` |
| `LX_CYCLE_LENGTH_MS` | 90000 ms (= 48+4+2+30+4+2 seconds) | `lx_timer.h` |
| `RLX_WARNING_TO_CLOSING_MS` | 5000 ms | `rlx_timer.h` |
| `RLX_CLOSING_DEADLINE_MS` / `RLX_OPENING_DEADLINE_MS` | 15000 ms | `rlx_timer.h` |
| `RLX_EXPECTED_ARRIVAL_MS` | 20000 ms | `rlx_timer.h` |
| `RLX_OCCUPANCY_WINDOW_MS` | 20000 ms | `rlx_timer.h` |
| `RLX_GATE_MOTION_MS` | 3000 ms | `rlx_gate.h` |
| Lx/RLx → C1 heartbeat | every 1000 ms | `lx_main.c`/`rlx_main.c` |
| UNAVAILABLE threshold (PA-07) | 3 consecutive ticks with no report (~3s) | `c_watchdog_mon.c` |

`controller_id_t` integers appearing in logs (`sys_types.h`): C1=0,
L1..L6=1..6, RL1..RL3=7..9. `msg_result_t`: ACK=1, ACK_PENDING=2, NACK=3,
ERROR=4. `supervisory_state_t` (SUPERVISORY column in C1's HMI table):
FAULT_SAFE=0, RAILWAY_PREEMPTION=1, CENTRAL_OVERRIDE=2, NORMAL_OPERATION=3.

---

## UC-01 — Serve Vehicle Demand

### TC-UC01-1: Default PEAK_FIXED cycle runs on the exact fixed schedule
- **Type**: Positive
- **Related**: UC-01 main flow steps 1-5, alt 2.1 (Peak Fixed operation)
- **Environment**: (A) `lx_main 1` only
- **Setup**: nothing special — L1 starts in `MODE_PEAK_FIXED`, initial phase is `PHASE_ARTERIAL_GREEN` (default in `lx_fsm_init()`).
- **Steps**:
  1. Run `/tmp/lx_main 1`.
  2. Press no keys, just watch the log for at least 90 seconds (exactly 1 `LX_CYCLE_LENGTH_MS` cycle).
- **Expected Result**: log lines print in exact order at the exact relative
  timestamps from start: `t=0` "Lx 1: signal phase now ARTERIAL GREEN"
  (printed immediately in `lx_fsm_init()`), `t=48s` "... -> ARTERIAL
  YELLOW", `t=52s` "... -> ALL RED (A to B)", `t=54s` "... -> CONNECTOR
  GREEN", `t=84s` "... -> CONNECTOR YELLOW", `t=88s` "... -> ALL RED (B to
  A)", `t=90s` back to "... -> ARTERIAL GREEN". No other line interleaves,
  since no demand/ped/queue key was pressed.

### TC-UC01-2: OFF_PEAK_SENSOR with no demand rests at the arterial phase (alt 2.2)
- **Type**: Positive
- **Related**: UC-01 alt 2.2 "No Off-Peak demand is present"
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: both processes running, no demand key pressed on L1 yet.
- **Steps**:
  1. On `c_main`, press `m` → prompt "  Lx number (1-6): " enter `1` → prompt "  mode (0=PEAK_FIXED, 1=OFF_PEAK_SENSOR): " enter `1`.
  2. Wait up to 54 seconds (for L1 to finish the current ARTERIAL_GREEN + YELLOW + ALL_RED, since the new mode only applies at the next ALL_RED boundary — UC-07 BR-2).
  3. Once L1 re-enters `PHASE_ARTERIAL_GREEN` under `MODE_OFF_PEAK_SENSOR`, press none of `a`/`c`/`1`..`4` — just observe for another 60 seconds.
- **Expected Result**: step 1 prints "C1: SET_MODE to 1 -> ACK_PENDING"
  (differs from current mode, so `lx_fsm_on_set_mode()` returns
  `RESULT_ACK_PENDING`, see `lx_fsm.c` lines 660-676). After the mode
  change, L1 **never** exits `ARTERIAL_GREEN` on its own (no "signal phase
  now ARTERIAL YELLOW" line appears in the 60-second observation window) —
  because `lx_fsm_on_phase_timer()` only checks the exit condition at the
  4s mark (`green_elapsed_ms % 4000 == 0`) and the exit condition requires
  `own_demand` or `maxed`; with no demand present it never exits (rests
  indefinitely at arterial per DP-04/BR-3).

### TC-UC01-3: Edge — exits green at exactly the 8000 ms minimum with no own demand but opposing demand present
- **Type**: Edge case
- **Related**: UC-01 main flow step 6, BR-2 ("Off-Peak green must be 8-40s")
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: L1 already in `MODE_OFF_PEAK_SENSOR` (as in TC-UC01-2 steps 1-2), resting at `PHASE_ARTERIAL_GREEN` with `green_elapsed_ms` freshly reset to 0 (right after the latest "signal phase now ARTERIAL GREEN" line).
- **Steps**:
  1. Right when "signal phase now ARTERIAL GREEN" appears, press `c` on L1's `lx_sensor` (connector demand = 1). **Do not** press `a` (arterial demand stays 0).
  2. Time it, watching continuously around the 7.9s-8.5s mark from that ARTERIAL GREEN line (don't wait for "exactly second 8" — this log has no timestamp so an absolute mark can't be confirmed by hand; see TL-TIME-02 in `04-timing-assumptions.md` for why this tolerance is used instead of an absolute mark).
- **Expected Result**: at the 7.9s mark, the phase must still be
  `ARTERIAL GREEN` (not yet reached `LX_MIN_GREEN_MS`=8000ms, so it's held
  — the earlier `t=4000ms` mark is certainly also not enough, no need to
  check separately). Within the 8.0s-8.5s window (±300-500ms tolerance for
  the 100ms system tick plus manual timing/reading lag), L1 must print
  "Lx 1: signal phase now ARTERIAL YELLOW" — not before 7.9s and not after
  8.5s (since `own_demand=arterial_vehicle_demand(0)||ped(0)=0`, so
  `lx_timer_should_exit_green()` returns true as soon as min-green is
  passed, no need to wait until 40000ms).

### TC-UC01-4: Edge — continuous demand is still forced out at the 40000 ms maximum (anti-starvation of connector)
- **Type**: Edge case
- **Related**: UC-01 alt 6.2 ("Connector demand is waiting"), BR-2, BR-4 (DP-06)
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: same as TC-UC01-3 but keep arterial demand on this time.
- **Steps**:
  1. Right when "signal phase now ARTERIAL GREEN" appears (under `MODE_OFF_PEAK_SENSOR`), press `a` (arterial demand = 1) then `c` (connector demand = 1). Do not press `A`/`C` to clear during this phase.
  2. Watch continuously around the 39.9s-40.5s mark from that ARTERIAL GREEN line (don't wait for "exactly second 40" — this log has no timestamp so an absolute mark can't be confirmed by hand; see TL-TIME-02 in `04-timing-assumptions.md` for why this tolerance is used instead of an absolute mark).
- **Expected Result**: at every 4s mark (t=8000, 12000, ..., 36000) L1
  **does not** exit the phase (since `own_demand=1` and not yet `maxed`).
  At the 39.9s mark, the phase must still be `ARTERIAL GREEN` (not yet
  reached `LX_MAX_GREEN_MS`=40000ms). Within the 40.0s-40.5s window
  (±300-500ms tolerance for the 100ms system tick plus manual timing/reading
  lag), L1 must print "Lx 1: signal phase now ARTERIAL YELLOW" — since
  `maxed = (elapsed >= 40000)` forces `!own_demand||maxed` to true
  regardless of `arterial_vehicle_demand` still being 1, per BR-2/DP-06
  (a waiting connector must not be starved).

### TC-UC01-5: Negative — connector demand is safely held while railway pre-emption takes over (alt 3.1)
- **Type**: Negative
- **Related**: UC-01 alt 3.1 ("A higher-priority condition arises")
- **Environment**: (B) `c_main` + `lx_main 1` + `rlx_main 1` (RL1 adjacent to L1)
- **Setup**: all 3 processes running, L1 in default `MODE_PEAK_FIXED`.
- **Steps**:
  1. On L1's `lx_sensor`, press `c` (connector demand = 1) — this demand only matters in `OFF_PEAK_SENSOR` but is still latched internally; the point is to prove connector is **never** served during pre-emption regardless of demand.
  2. On RL1's `rlx_sensor`, press `0` (TRAIN_APPROACHING direction 0).
  3. Watch L1's log continuously for 60 seconds from the `0` keypress.
- **Expected Result**: within ~1 second of step 2, L1 receives
  `MSG_CROSSING_STATUS(WARNING)` from RL1 and switches to
  `supervisory=SUPERVISORY_RAILWAY_PREEMPTION`; from then until RL1 reports
  `OPEN` again (~52 seconds later, see UC-04), L1's log **only** repeats
  "signal phase now ARTERIAL GREEN" → "ARTERIAL YELLOW" → "ALL RED (A to
  B)" → "ARTERIAL GREEN" — "signal phase now CONNECTOR GREEN" **never**
  appears in this window (because at the `PHASE_ALL_RED_A_TO_B` boundary,
  the `SUPERVISORY_RAILWAY_PREEMPTION` branch forces it back to
  `PHASE_ARTERIAL_GREEN` — `lx_fsm.c` lines 262-277). The connector demand
  remains latched (`connector_vehicle_demand=1`), served safely only after
  pre-emption ends.

---

## UC-02 — Serve Pedestrian Crossing Request

### TC-UC02-1: WALK → FLASHING_DONT_WALK → DONT_WALK sequence with correct durations
- **Type**: Positive
- **Related**: UC-02 main flow steps 1-8, BR-1
- **Environment**: (A) `lx_main 1` only
- **Setup**: L1 just started, at `PHASE_ARTERIAL_GREEN` (side 0/1 are compatible with arterial green per `lx_fsm.c` lines 136-138).
- **Steps**:
  1. Right after the first "signal phase now ARTERIAL GREEN" line, press `1` (pedestrian button side 0).
  2. Observe the log for the next 11 seconds.
- **Expected Result**: within 100ms of the keypress, prints "Lx 1: PED
  SIGNAL side 0 -> WALK"; exactly `t=6000ms` later prints "... side 0 ->
  FLASHING_DONT_WALK"; exactly `t=10000ms` (6000+4000) prints "... side 0
  -> DONT_WALK". After the DONT_WALK line, `ped_latched[0]` is cleared —
  no repeat WALK unless `1` is pressed again.
- **Note**: the specific "within 100ms" bound **cannot** be verified
  strictly by manual console/eye observation alone — `lx_signal.c`'s log
  lines carry no timestamp, and human reaction time for pressing a key
  then reading the log (~150-300ms) already exceeds the bound being
  measured. This 100ms bound can only be confirmed precisely by an
  automated runner with a script under `tools/test-automation/` (using
  `time.monotonic()` to timestamp each captured output line). Manually,
  only confirm the qualitative requirement — WALK appears quickly, clearly
  under 1 second after the keypress — not the exact 100ms threshold.

### TC-UC02-2: Negative/alt 2.1 — repeated button presses before service produce exactly 1 request
- **Type**: Negative
- **Related**: UC-02 alt 2.1 ("A request is already pending"), BR-3
- **Environment**: (A) `lx_main 1` only
- **Setup**: L1 just started, at `PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. Press `1` three times rapidly in a row (within the same 100ms tick, before any WALK line can print).
  2. Observe the next 11 seconds.
- **Expected Result**: **exactly one** WALK→FDW→DONT_WALK sequence appears
  for side 0 (not 3 parallel/sequential chains) — because
  `lx_fsm_latch_pedestrian_request()` only sets `ped_latched[0]=1`
  (idempotent, `lx_fsm.c` lines 425-438), it does not count keypresses.

### TC-UC02-3: Negative/alt 3.1 equivalent — request is held (not lost) while the compatible side is blocked by railway pre-emption
- **Type**: Negative
- **Related**: UC-02 alt 3.1 ("Railway restriction prevents immediate service"), BR-4 (PA-02)
- **Environment**: (B) `lx_main 1` + `rlx_main 1` (RL1 adjacent to L1)
- **Setup**: L1 in `MODE_PEAK_FIXED`, at any phase. Side 2/3 is only
  compatible with `PHASE_CONNECTOR_GREEN` (`lx_fsm.c` lines 139-141), and
  `CONNECTOR_GREEN` is fully blocked during `RAILWAY_PREEMPTION` (same as
  TC-UC01-5).
- **Steps**:
  1. On RL1's `rlx_sensor`, press `0` to start pre-emption.
  2. Right after (within a few seconds), on L1's `lx_sensor` press `3` (pedestrian button side 2).
  3. Watch L1's log continuously until RL1 reports `OPEN` again (~52 seconds after step 1, see UC-04) and at least 10 more seconds after that.
- **Expected Result**: throughout pre-emption, no "PED SIGNAL side 2 ->
  WALK" line ever appears (since `CONNECTOR_GREEN` is never entered). The
  request stays latched (`ped_latched[2]` is not cleared). Only after RL1
  reports `OPEN`, L1's `supervisory` returns to `NORMAL_OPERATION`, and at
  the first `CONNECTOR_GREEN` afterward, "Lx 1: PED SIGNAL side 2 -> WALK"
  finally appears — per PA-02 "a received request must not be cancelled
  during pre-emption".

### TC-UC02-4: Edge — pressing again mid-FLASHING_DONT_WALK does not lose the request (bug fix)
- **Type**: Edge case
- **Related**: UC-02 main flow step 5 combined with step 2 (regression test for the `ped_recall` fix)
- **Environment**: (A) `lx_main 1` only
- **Setup**: L1 just started, at `PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. Right after "signal phase now ARTERIAL GREEN", press `1` (side 0) → WALK begins.
  2. Wait exactly 8 seconds from the keypress (i.e. mid-FLASHING_DONT_WALK window, since WALK lasts 6000ms then FDW begins — 8s is 2s into FDW, with 2s left).
  3. Press `1` again (side 0) at exactly this 8th second.
  4. Keep observing until second 12 (10000ms end of FDW plus margin).
- **Expected Result**: at step 3, since `ped_serving_mask` still contains
  side 0 (being served), `lx_fsm_latch_pedestrian_request()` sets
  `ped_recall[0]=1` instead of creating a new request right away
  (`lx_fsm.c` lines 429-434). At `t=10000ms`, the log still prints "...
  side 0 -> DONT_WALK" as normal, **but because `ped_recall[0]==1`,
  `ped_latched[0]` is NOT cleared** (`lx_fsm.c` lines 177-186) — on the
  very next tick (t=10100ms), a **new** WALK sequence for side 0 starts
  immediately: "Lx 1: PED SIGNAL side 0 -> WALK" appears a second time
  without pressing `1` again. This is exactly the fixed behavior (before
  the fix, a mid-cycle press was lost because `ped_latched[0]` wasn't
  re-set since it was already 1).

---

## UC-03 — Coordinate Arterial Traffic Progression

### TC-UC03-1: Positive — broadcasting a timing profile for chain R1 (L1/L3/L5) is ACKed by all 3 controllers
- **Type**: Positive
- **Related**: UC-03 main flow steps 1-8, BR-1, BR-2
- **Environment**: (B) `c_main` + `lx_main 1` + `lx_main 3` + `lx_main 5`
- **Setup**: all 4 processes running, no timing profile sent yet (`next_profile_id` is 1).
- **Steps**:
  1. On `c_main`, press `t` → prompt "  chain (1=R1 L1/L3/L5, 2=R2 L2/L4/L6): " enter `1`.
- **Expected Result**: C1's log prints "Operator: SET_TIMING_PROFILE
  broadcast (chain=R1, profile_id=1) submitted", then 3 separate lines
  "C1: SET_TIMING_PROFILE to 1 -> ACK", "... to 3 -> ACK", "... to 5 ->
  ACK" (offsets sent: L1=0ms, L3=21000ms, L5=45000ms — all < 90000ms so
  always valid, see `c_mode_eng.h`). Since the offset only applies at the
  next fresh entry into `PHASE_ARTERIAL_GREEN` (`lx_fsm.c` lines 339-345,
  `offset_apply_pending`), L1/L3/L5 do not change phase immediately —
  wait until each controller naturally re-enters ARTERIAL_GREEN (up to 90
  seconds later) to see their `green_elapsed_ms` adjusted per the assigned
  offset.

### TC-UC03-2: Negative — an invalid chain number is aborted right at the console, nothing sent
- **Type**: Negative
- **Related**: UC-03 — Trigger's input area ("operator submits a validated ... profile")
- **Environment**: (B) `c_main` + `lx_main 1` (not required, but confirms nothing is received)
- **Setup**: c_main running.
- **Steps**:
  1. Press `t` → chain prompt, enter `3` (neither 1 nor 2).
- **Expected Result**: prints immediately "c_operator: 3 is not a valid
  chain (1 or 2) - command aborted"; **no** "Operator: SET_TIMING_PROFILE
  broadcast..." line is logged, and L1 receives no
  `MSG_SET_TIMING_PROFILE` (no unusual "SIGNAL ->" lines, no ACK in C1's log).

### TC-UC03-3: Negative/alt 2.1 — L2 that never received a profile still runs standalone normally
- **Type**: Negative
- **Related**: UC-03 alt 2.1 ("The coordination profile is missing or stale")
- **Environment**: (A) `lx_main 2` only
- **Setup**: only L2 running, no C1, no `SET_TIMING_PROFILE` ever sent.
- **Steps**:
  1. Run `/tmp/lx_main 2`, do nothing else.
  2. Observe for 90 seconds.
- **Expected Result**: L2 runs the exact default `PEAK_FIXED` cycle just
  like TC-UC01-1 (48s/4s/2s/30s/4s/2s), without waiting or hanging
  anywhere for Central — since `assigned_offset_ms`/`offset_apply_pending`
  default to 0 (`lx_fsm_init()`), equivalent to "no offset", not an error.

### TC-UC03-4: Edge — pressing `t` twice in a row for the same chain increments profile_id without breaking the earlier one
- **Type**: Edge case
- **Related**: UC-03 main flow step 2 (source of `profile_id`)
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: `next_profile_id` at any value N (e.g. N=1 if testing standalone, not run right after TC-UC03-1 on the same c_main process).
- **Steps**:
  1. Press `t` → chain `1`. Note the `profile_id` in the log (e.g. N).
  2. Immediately press `t` → chain `1` again.
- **Expected Result**: the second log line shows `profile_id=N+1` (per
  `c_mode_eng_next_profile_id()` — post-increment, `c_mode_eng.c` lines
  81-84), and L1 receives **both** `MSG_SET_TIMING_PROFILE` messages back
  to back, both `ACK` (offset unchanged so
  `payload->offset_ms(0) < LX_CYCLE_LENGTH_MS` is always true) —
  L1's final `active_profile_id`/`assigned_offset_ms` reflects the second
  send (N+1), not stuck in an intermediate state.

---

## UC-04 — Protect a Railway Crossing for an Approaching Train

### TC-UC04-1: Positive — full sequence WARNING → CLOSING → CLOSED → TRAIN_PRESENT → OPENING → OPEN
- **Type**: Positive
- **Related**: UC-04 main flow steps 1-11, BR-1, BR-2, BR-3
- **Environment**: (B) `rlx_main 1` + `lx_main 1` + `lx_main 2` (RL1 adjacent to L1, L2)
- **Setup**: all 3 processes running, crossing is `OPEN`.
- **Steps**:
  1. On RL1's `rlx_sensor`, press `0` (TRAIN_APPROACHING direction 0) at `t=0`.
  2. No further action; watch RL1's log and L1/L2's log continuously for 55 seconds.
- **Expected Result** (marks measured from `t=0`):
  - `t=0`: RL1 prints "RLx: flashers ON (train approaching, direction 0)".
  - `t≈0-1s`: L1 and L2 each receive `MSG_CROSSING_STATUS(WARNING)` (broadcast on the next 1s tick) — no separate log line on the Lx side for this event, but from this point `supervisory` on both L1 and L2 switches to `RAILWAY_PREEMPTION`.
  - `t=5s` (`RLX_WARNING_TO_CLOSING_MS`): RL1 prints "RLx: commanding gates DOWN (simulated motion, 3000 ms)".
  - `t=8s` (5000+`RLX_GATE_MOTION_MS`): gate confirms closed, RL1 prints "RLx: train signal PROCEED for direction 0 (gates confirmed closed)".
  - From `t≈0` to `t≈52s`: L1/L2's log has **no** "SIGNAL -> CONNECTOR GREEN" line (same as TC-UC01-5).
  - `t=28s` (8000+`RLX_EXPECTED_ARRIVAL_MS`=8000+20000): crossing switches to `TRAIN_PRESENT` internally (no separate log line, only inferred from the occupancy window starting its 20000ms count).
  - `t=48s` (28000+20000): active_window_count reaches 0, RL1 prints "RLx: all train signals -> STOP (crossing reopening)" then "RLx: commanding gates UP (simulated motion, 3000 ms)".
  - `t=51s` (48000+3000): RL1 prints "RLx: flashers OFF (gates confirmed open)".
  - `t≈51-52s`: L1/L2 receive `MSG_CROSSING_STATUS(OPEN)`, `supervisory` returns to `NORMAL_OPERATION`.

### TC-UC04-2: Negative/alt 6.1 — gate confirmation failure leads to FAULT, train signal stays STOP
- **Type**: Negative
- **Related**: UC-04 alt 6.1 ("Gate closure is not confirmed"), BR-2
- **Environment**: (B) `rlx_main 1` + `lx_main 1` + `lx_main 2`
- **Setup**: crossing `OPEN`.
- **Steps**:
  1. On RL1's `rlx_sensor`, press `x` (arm a confirmation fault for the next close).
  2. Right after, press `0` (TRAIN_APPROACHING direction 0) at `t=0`.
  3. Observe for 21 seconds.
- **Expected Result**:
  - `t=5s`: "RLx: commanding gates DOWN (simulated motion, 3000 ms)".
  - `t=8s`: gate motion "completes" but the armed fault triggers — RL1 prints "RLx: gate FAILED TO CONFIRM (simulated fault) - rlx_fsm.c's own deadline will raise FAULT_GATE_CONFIRM_MISSING". **No** "train signal PROCEED" line appears.
  - `t=20s` (5000+`RLX_CLOSING_DEADLINE_MS`=5000+15000): RL1 prints "RLx: FAULT latched (GATE_CONFIRM_MISSING, bit 0x1) - holding STOP on all train signals, commanding gates DOWN".
  - During and after `t=20s`, the train signal never shows PROCEED;
    L1/L2 stay in `RAILWAY_PREEMPTION` (since the crossing state reports
    `FAULT` ≠ `OPEN`), meaning connector remains suppressed indefinitely
    until the fault is handled (see UC-06).

### TC-UC04-3: Edge/alt 8.1 — a second train arriving during TRAIN_PRESENT extends the gate-closed time
- **Type**: Edge case
- **Related**: UC-04 alt 8.1 ("A second train is detected before reopening"), BR-3
- **Environment**: (B) `rlx_main 1` (Lx not required for this test, may add `lx_main 1`/`lx_main 2` to observe both sides)
- **Setup**: crossing `OPEN`.
- **Steps**:
  1. Press `0` at `t=0` (as in TC-UC04-1, no fault armed).
  2. At exactly `t=35s` (13 seconds into TRAIN_PRESENT which began at `t=28s`, direction 0's window still has 13s left until `t=48s`), press `1` (TRAIN_APPROACHING direction 1).
  3. Observe until `t=60s`.
- **Expected Result**: step 2 immediately registers a new occupancy window
  for direction 1 with `remaining_ms=20000` (since already in
  `RLX_TRAIN_PRESENT`, `rlx_fsm_simulate_train_approaching()` case
  `RLX_TRAIN_PRESENT` skips the "expected arrival" wait). Direction 0's
  window ends at `t=48s` but `active_window_count` **does not** reach 0
  (direction 1's window is still active), so RL1 **does not** print "gates
  UP" at `t=48s`. Only at `t=55s` (35000+20000), when direction 1's window
  also ends, does `active_window_count` reach 0 and RL1 print "RLx: all
  train signals -> STOP (crossing reopening)" then open the gates —
  7 seconds later than the single-train scenario, per RC-04 "gates remain
  closed while either occupancy window remains active".

### TC-UC04-4: Negative — a repeated approach during the same WARNING step does not reset the 5-second clock
- **Type**: Negative
- **Related**: UC-04 main flow step 2 (self-loop in `rlx_fsm_simulate_train_approaching()`), verifying no unintended WARNING extension bug
- **Environment**: (A) `rlx_main 1` only
- **Setup**: crossing `OPEN`.
- **Steps**:
  1. Press `0` at `t=0`.
  2. At `t=2s`, press `1` (other direction, still within `WARNING`).
  3. Observe until `t=6s`.
- **Expected Result**: step 2 only registers one more occupancy window for
  direction 1 (self-loop, `rlx_fsm.c` lines 247-253), **without** printing
  another "flashers ON" line and **without** resetting
  `state_elapsed_ms` — the gate still switches to CLOSING exactly at
  `t=5s` (measured from the first `0` press, not `t=7s` as it would be if
  the clock were wrongly reset).

---

## UC-05 — Manage Road Traffic During and After a Railway Closure

### TC-UC05-1: Positive — drain phase extends in 4-second steps, then ends safely once the queue warning clears
- **Type**: Positive
- **Related**: UC-05 main flow steps 6-10, BR-3 (CC-03)
- **Environment**: (B) `rlx_main 1` + `lx_main 1`
- **Setup**: L1 in `MODE_PEAK_FIXED`. Reuses the full railway cycle from TC-UC04-1 (~52 seconds from the train press to crossing `OPEN`).
- **Steps**:
  1. Press `0` on RL1 at `t=0` to start pre-emption (as in TC-UC04-1).
  2. While still in `RAILWAY_PREEMPTION` (e.g. at `t=30s`), on L1's `lx_sensor` press `w` (queue warning = 1).
  3. Wait for the crossing to report `OPEN` (~`t=52s`) — at this point `drain_pending=1` is armed since `queue_warning_active` is on exactly at the `RAILWAY_PREEMPTION → NORMAL_OPERATION` transition (`lx_fsm.c` lines 821-823).
  4. L1 finishes its current arterial round (up to 48s + 4s + 2s more if it just entered arterial), then naturally enters `CONNECTOR_GREEN` — this is the drain phase (armed in `lx_fsm_advance_phase_locked()` lines 287-292).
  5. When this `CONNECTOR_GREEN` reaches 30000ms (`LX_PEAK_CONNECTOR_GREEN_MS`) with `queue_warning_active` still on, it starts "extending" (no dedicated log line, only inferred from not switching to YELLOW at exactly 30s).
  6. Exactly 8 seconds after that 30s mark (2 rounds of 4s extension), press `W` (queue warning = 0) on L1.
- **Expected Result**: L1 stays in `CONNECTOR_GREEN` until 30s + 8s = 38s
  from entering connector green, then — at the next 4s checkpoint that
  detects `!queue_warning_active` — L1 prints "Lx 1: SIGNAL -> CONNECTOR
  YELLOW" and safely ends the drain (not exceeding the 60000ms cap). Total
  connector green time for this drain is roughly 36-40 seconds, longer
  than the normal 30s by a multiple of 4s.

### TC-UC05-2: Negative/alt 6.1 — no queue warning means drain is skipped entirely
- **Type**: Negative
- **Related**: UC-05 alt 6.1 ("No queue warning exists after reopening")
- **Environment**: (B) `rlx_main 1` + `lx_main 1`
- **Setup**: same as TC-UC05-1 but **never** press `w`.
- **Steps**:
  1. Press `0` on RL1 at `t=0`, never press `w`/`W` on L1 throughout.
  2. After crossing `OPEN` (~`t=52s`), wait for L1 to enter the next `CONNECTOR_GREEN` and observe exactly 31 seconds from then.
- **Expected Result**: since `queue_warning_active=0` at the
  `RAILWAY_PREEMPTION → NORMAL_OPERATION` transition, `drain_pending` is
  never set. `CONNECTOR_GREEN` ends exactly at the 30000ms mark like a
  normal PEAK_FIXED cycle — prints "signal phase now CONNECTOR YELLOW"
  right at `t=30s` from entering connector green, with no extension at all.

### TC-UC05-3: Edge — queue warning never clears, drain hard-cuts exactly at the 60000 ms cap
- **Type**: Edge case
- **Related**: UC-05 alt 9.1 ("Queue warning remains active at 60 seconds"), BR-3
- **Environment**: (B) `rlx_main 1` + `lx_main 1`
- **Setup**: same as TC-UC05-1 through step 4 (drain armed and already in `CONNECTOR_GREEN`).
- **Steps**:
  1. Press `w` before the crossing reopens (as in TC-UC05-1 step 2) and **never** press `W` afterward.
  2. From when L1 enters `CONNECTOR_GREEN` (drain phase), watch continuously up to about (30+60)/60 ≈ 1.5 minutes after entering this phase.
- **Expected Result**: drain keeps extending in 4-second increments
  (`drain_extension_total_ms` accumulating) regardless of
  `queue_warning_active` still being 1, until
  `drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS` (60000ms) —
  exactly at the 30000+60000=90000ms mark from entering connector green,
  L1 immediately prints "signal phase now CONNECTOR YELLOW" even though
  `queue_warning_active` is still on (`lx_fsm.c` line 1022:
  `drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS` wins over the
  warning condition), per the hard 60-second cap.

### TC-UC05-4: Negative/alt 4.1 — crossing entering FAULT keeps the connector direction held red indefinitely
- **Type**: Negative
- **Related**: UC-05 alt 4.1 ("Crossing enters FAULT"), BR-1 (CC-02)
- **Environment**: (B) `rlx_main 1` + `lx_main 1`
- **Setup**: crossing `OPEN`.
- **Steps**:
  1. On RL1, press `x` then `0` (triggering the FAULT scenario as in TC-UC04-2), wait until `t=20s` for crossing to enter `FAULT`.
  2. Keep observing L1's log for another 60 seconds (no further action).
- **Expected Result**: since the crossing's reported state is `FAULT` (≠
  `OPEN`), L1 stays at `supervisory=RAILWAY_PREEMPTION` indefinitely — no
  "signal phase now CONNECTOR GREEN" line appears throughout the full
  60-second extra observation, while "signal phase now ARTERIAL
  GREEN/YELLOW/ALL RED..." keeps cycling normally (cross-traffic
  unaffected, per CC-02 "compatible cross-traffic ... continue"). Recovery
  can only happen once the fault is handled on the RLx side (see UC-06) and
  the crossing reports `OPEN` again.

### TC-UC05-5 (Regression, CC-01/CC-02): a train arriving mid-way through a connector green cuts it short to the minimum immediately, instead of running the full cycle
- **Type**: Regression (fixed) + Edge case (exact `LX_MIN_GREEN_MS` boundary)
- **Why it was a bug**: before the latest audit/fix,
  `lx_fsm_on_phase_timer()`'s `PHASE_CONNECTOR_GREEN` case and
  `lx_fsm_advance_phase_locked()`'s boundary check only prevented a
  **new** connector-green cycle from starting during
  `RAILWAY_PREEMPTION`, but did not cut short one **already running** — if
  a train arrived right after L1 entered `CONNECTOR_GREEN`, that green
  could run its full 30s (`PEAK_FIXED`) or up to 40s
  (`OFF_PEAK_SENSOR`/draining), eating into the ~25s margin meant for the
  adjacent intersection to clear vehicles (RC-03/Appendix B4). This was a
  real safety gap, not cosmetic.
- **Related**: `lx_fsm_on_phase_timer()`'s `PHASE_CONNECTOR_GREEN` case
  (`lx_fsm.c`) — new check, runs every 100ms tick (does not wait for the
  4s mark like the normal check): if
  `supervisory == SUPERVISORY_RAILWAY_PREEMPTION && green_elapsed_ms >= LX_MIN_GREEN_MS`,
  calls `lx_fsm_advance_phase_locked()` immediately, cutting to `YELLOW`.
- **Environment**: (B) `rlx_main 1` + `lx_main 1` + `lx_main 2`
- **Setup**: get L1 into `PHASE_CONNECTOR_GREEN` exactly — easiest way: wait for the natural cycle to reach `CONNECTOR GREEN` (log `signal phase now CONNECTOR GREEN`), act right when that line appears (within 1-2 seconds).
- **Steps**:
  1. Right when L1 enters `CONNECTOR GREEN` (well before its normal 30s/40s end), on RL1 press `0` to simulate a train arriving.
  2. Wait out the 5s warning (`RLX_WARNING_TO_CLOSING_MS`) for RL1 to switch to `CLOSING` and send `CROSSING_STATUS(WARNING)` to L1 — L1 enters `RAILWAY_PREEMPTION` as soon as it receives this (no need to wait for the gate to finish closing).
  3. From when L1 enters `RAILWAY_PREEMPTION` (step 2), count exactly 8 seconds (`LX_MIN_GREEN_MS`) from when `CONNECTOR GREEN` began in step 1 — watch L1's log closely around this mark.
- **Expected Result**: L1's log must print `signal phase now CONNECTOR
  YELLOW` **exactly at/just after the 8-second mark from CONNECTOR GREEN's
  start** (not the normal 30s/40s mark) — i.e. the light is cut to the
  safe minimum green as soon as allowed, not running out the full cycle.
  If the log still shows `CONNECTOR GREEN` running past 8-9 seconds after
  `RAILWAY_PREEMPTION` is confirmed, this is a regression of the fixed bug.
- **Note**: if the train arrives while `green_elapsed_ms < LX_MIN_GREEN_MS`
  (less than 8s into green), TL-01's minimum-green floor must still be
  honored — the light only cuts exactly at the 8s mark, never earlier (see
  the code check: `green_elapsed_ms >= LX_MIN_GREEN_MS`, not `> 0`).

---

## UC-06 — Respond to a Railway Equipment Fault

### TC-UC06-1: Positive — a local fault holds STOP immediately and is independently reported to Central
- **Type**: Positive
- **Related**: UC-06 main flow steps 1-6, BR-1, BR-2
- **Environment**: (B) `c_main` + `rlx_main 1`
- **Setup**: crossing `OPEN`.
- **Steps**:
  1. On RL1's `rlx_sensor`, press `x` then `0` at `t=0`.
  2. Watch RL1's log and C1's log until `t=21s`.
- **Expected Result**: at `t=20s`, RL1 prints "RLx: FAULT latched
  (GATE_CONFIRM_MISSING, bit 0x1) - holding STOP on all train signals,
  commanding gates DOWN" (as in TC-UC04-2). Within 1 second after that
  (next `IPC_PULSE_RAILWAY_WARNING` tick calls
  `rlx_comm_send_fault_report()`), C1 logs:
  `FAULT_REPORT from 7: fault_code=0x00000001 severity=1 detail="RLx fault - see fault_code bitmask"`
  (7 = `CTRL_RL1`). If `c_hmi` is also running (its existing 1Hz tick),
  C1's status table shows RL1's row with `FAULTS=0x1` and
  `CROSSING_STATE=3` (`CROSSING_FAULT`).

### TC-UC06-2: Negative/alt 5.1 — local safety does not depend on Central
- **Type**: Negative
- **Related**: UC-06 alt 5.1 ("Central communication is unavailable")
- **Environment**: (A) `rlx_main 1` only (no `c_main` running)
- **Setup**: crossing `OPEN`, no C1 running.
- **Steps**:
  1. Press `x` then `0` at `t=0`, same as TC-UC06-1.
  2. Observe for 21 seconds.
- **Expected Result**: exactly at `t=20s`, RL1 still prints "RLx: FAULT
  latched ..." identical to TC-UC06-1 — no delay, no hang waiting for C1
  (since `rlx_comm_send_fault_report()` only calls `ipc_client_post()`,
  non-blocking). With no C1 running, `ipc_client_post()` fails to send
  (`name_open("traffic/c1")` finds nothing) but only logs an error on the
  RLx side ("RLx: message to 0 failed to send" from `on_reply_log_failure`
  in `rlx_comm.c`) — this does not affect the local safe state (gates
  already closed, signals already STOP), which needs no confirmation from
  C1 at all.

### TC-UC06-3: Negative/alt 7.2 — a fault-clear request while the physical fault hasn't actually cleared is NACKed
- **Type**: Negative
- **Related**: UC-06 alt 7.2 ("Fault-clear request is premature"), BR-4 (PA-09)
- **Environment**: (B) `c_main` + `rlx_main 1`
- **Setup**: RL1 already in `FAULT` as in TC-UC06-1 (`t=20s` after pressing `x`+`0`).
- **Steps**:
  1. Right after seeing "FAULT latched" on RL1, on RL1's `rlx_sensor` press `f` (local demo fault-clear).
  2. Separately, on `c_main` press `f` → prompt "  RLx number (1-3): " enter `1`.
- **Expected Result**: step 1 prints "[rlx_sensor] fault-clear result=3
  reason=4" (`RESULT_NACK`=3, `NACK_REASON_FAULT_ACTIVE`=4) — since
  `rlx_fsm_on_fault_clear()` checks `gates_confirmed_open()` and finds it
  false (gate still mid-close/faulted, not open). Step 2 (the real Central
  path) logs C1 printing "C1: REQUEST_FAULT_CLEAR to 7 -> NACK
  reason=FAULT_ACTIVE". Crossing stays `FAULT`, train signal stays `STOP`.

### TC-UC06-4: Edge — a known limitation: the fault-clear ACK path (alt 7.1) cannot currently be reproduced via the demo gate simulator
- **Type**: Edge case
- **Related**: UC-06 alt 7.1 ("Operator requests fault clearance after repair"), cross-checked against `rlx_gate.c`
- **Environment**: (B) `c_main` + `rlx_main 1`
- **Setup**: RL1 already in `FAULT` as in TC-UC06-3.
- **Steps**:
  1. Press `f` on RL1's `rlx_sensor` three times in a row, a few seconds apart.
  2. Press `f` from C1's operator console (target RLx=1) once more.
- **Expected Result**: **all four** attempts return NACK/`FAULT_ACTIVE`
  like TC-UC06-3, none ACK. This is a direct consequence of `enter_fault()`
  in `rlx_fsm.c` always calling `rlx_gate_command_close()` (resetting
  `g_confirmed_open=0` in `rlx_gate.c`), and **no other code path** ever
  calls `rlx_gate_command_open()` while `state==RLX_FAULT` (only
  `enter_opening()` calls it, and only from `RLX_TRAIN_PRESENT`, not from
  `RLX_FAULT`). So with the current gate simulator, **there is no way
  through the available keys/commands to bring `gates_confirmed_open()` to
  true while in `FAULT`** — meaning the ACK branch of alt 7.1 currently
  cannot be tested end-to-end. Noted as a gap to report back to the dev
  team (not a test-case defect), and not incorrect FSM behavior relative
  to spec.

---

## UC-07 — Configure Traffic Operating Parameters

### TC-UC07-1: Positive — SET_MODE to a different mode gets ACK_PENDING then applies at a safe boundary
- **Type**: Positive
- **Related**: UC-07 main flow steps 1-6, BR-2
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: L1 in `MODE_PEAK_FIXED` (default), mid-`PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. Press `m` → prompt "  Lx number (1-6): " enter `1` → prompt "  mode (0=PEAK_FIXED, 1=OFF_PEAK_SENSOR): " enter `1`.
- **Expected Result**: C1 logs "Operator: SET_MODE(target=1,
  mode=OFF_PEAK_SENSOR) submitted" then "C1: SET_MODE to 1 -> ACK_PENDING"
  (differs from current mode → `else` branch of `lx_fsm_on_set_mode()`,
  `lx_fsm.c` lines 670-676). L1 **does not** change behavior immediately
  (still runs the current 48s arterial green as PEAK_FIXED would) — only
  at the next `ALL_RED` boundary (`lx_fsm.c` lines 239-242 or 306-309)
  does `fsm->mode` actually change to `OFF_PEAK_SENSOR`.

### TC-UC07-2: Negative — an out-of-range Lx number is aborted right at the console
- **Type**: Negative
- **Related**: UC-07 alt 3.1 equivalent (invalid request rejected before reaching the controller)
- **Environment**: (B) `c_main` (no Lx required)
- **Setup**: c_main running.
- **Steps**:
  1. Press `m` → Lx number prompt, enter `7`.
- **Expected Result**: prints immediately "c_operator: 7 is not a valid Lx
  (1-6) - command aborted"; the function returns **before** asking for the
  mode prompt — no "Operator: SET_MODE..." line is logged, nothing sent.

### TC-UC07-3: Negative — an invalid mode value is aborted without touching bookkeeping
- **Type**: Negative
- **Related**: UC-07 BR-1 ("Normal mode selection uses only PEAK_FIXED and OFF_PEAK_SENSOR")
- **Environment**: (B) `c_main` + `lx_main 2`
- **Setup**: c_main and L2 running.
- **Steps**:
  1. Press `m` → Lx number `2` → mode enter `2` (neither 0 nor 1).
- **Expected Result**: prints "c_operator: 2 is not a valid mode (0 or 1) -
  command aborted" — the function returns **before** the
  `last_commanded_mode` update line (see code order in
  `handle_set_mode()`), so no "Operator: SET_MODE..." is logged and L2
  receives nothing.
  Note: this is only a Central-side (console) pre-check — after the
  latest re-audit fix, `lx_fsm_on_set_mode()` on the Lx side also
  independently validates `payload->mode` and returns
  `NACK_REASON_OUT_OF_RANGE` for values outside {0,1}, per UC-07 main flow
  step 3 "validates the request against supported ranges" — Lx is the
  authoritative validator, not `c_operator.c`'s pre-filter; see
  `03-protocol-contract.md` TC-MSG-8b to test that branch separately via
  test_client (required, since the console won't let you send an invalid value).

### TC-UC07-4: Edge — SET_MODE to the same mode already in effect gets ACK immediately (not PENDING)
- **Type**: Edge case
- **Related**: UC-07 — the "genuinely idle" branch (SC-01A) of `lx_fsm_on_set_mode()`
- **Environment**: (B) `c_main` + `lx_main 3`
- **Setup**: L3 in `MODE_PEAK_FIXED` (default, never changed).
- **Steps**:
  1. Press `m` → Lx number `3` → mode `0` (PEAK_FIXED — same as current mode).
- **Expected Result**: C1 logs "C1: SET_MODE to 3 -> ACK" (not
  `ACK_PENDING`) — since `(operating_mode_t)payload->mode == fsm->mode`
  holds true, falling into the immediate-apply branch of
  `lx_fsm_on_set_mode()` (`lx_fsm.c` lines 660-669), which does not set
  `mode_change_pending`.

---

## UC-08 — Apply a Bounded Clear-Route Override

### TC-UC08-1: Positive — an arterial override really holds green past the normal PEAK_FIXED duration
- **Type**: Positive
- **Related**: UC-08 main flow steps 1-8, BR-1, BR-5
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: L1 just started, at `PHASE_ARTERIAL_GREEN` with
  `green_elapsed_ms` near 0 (issue the command as soon as possible after
  the "signal phase now ARTERIAL GREEN" line).
- **Steps**:
  1. Press `o` → prompt "  Lx number (1-6): " enter `1` → prompt "  target movement (0=arterial, 1=connector): " enter `0` → prompt "  duration_ms (1-300000): " enter `60000`.
  2. Watch L1's log continuously until second 62.
- **Expected Result**: C1 logs "Operator: REQUEST_OVERRIDE(target=1,
  movement=0, duration_ms=60000) submitted" then "C1: REQUEST_OVERRIDE to
  1 -> ACK" (no ped clearance running, no railway, no fault → direct ACK
  branch, `lx_fsm.c` lines 729-736). **No** "signal phase now ARTERIAL
  YELLOW" line appears at the normal 48-second mark — because the guard at
  `lx_fsm.c` lines 956-965 holds `ARTERIAL_GREEN` for as long as
  `override_substate==OVR_ACTIVE` and `target_movement==ARTERIAL`. Exactly
  at second 60 (`override_remaining_ms` expires), L1 prints "Lx 1:
  override cleared/expired - running safe clearance sequence" and only
  then "signal phase now ARTERIAL YELLOW" as normal.

### TC-UC08-2: Positive — a connector override genuinely forces the green to switch direction (per the latest review requirement)
- **Type**: Positive
- **Related**: UC-08 main flow steps 5-6 (the light genuinely changes direction, not just bookkeeping)
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: L1 just started, at `PHASE_ARTERIAL_GREEN`, `green_elapsed_ms≈0`.
- **Steps**:
  1. Press `o` → Lx `1` → target movement `1` (connector) → duration_ms `300000` (use the max cap to guarantee it's still active by the time connector phase arrives).
  2. Watch L1's log until "signal phase now CONNECTOR GREEN" appears (expected at `t≈54s`: 48s normal arterial green + 4s yellow + 2s all-red, override doesn't shorten these steps).
  3. Once "CONNECTOR GREEN" has held past the normal 30-second mark (wait at least 10 more seconds to confirm it doesn't auto-switch to yellow), end it early: press `c` → prompt "  Lx number (1-6): " enter `1`.
- **Expected Result**: at `t≈54s`, the `PHASE_ALL_RED_A_TO_B` boundary
  applies the override branch (`lx_fsm.c` lines 243-260): since
  `override_target_movement==OVERRIDE_MOVEMENT_CONNECTOR`, the next phase
  is forced to `PHASE_CONNECTOR_GREEN` — logs "Lx 1: signal phase now
  CONNECTOR GREEN" at this point (not distinguishable from normal
  PEAK_FIXED here, since PEAK_FIXED **also** reaches CONNECTOR_GREEN at
  this boundary — the real observable difference is the next step: this
  phase **does not** switch to "CONNECTOR YELLOW" at the normal 30s mark,
  due to the guard at lines 997-1006 holding it). At step 3, C1 logs
  "Operator: CANCEL_OVERRIDE(target=1) submitted" then "C1:
  CANCEL_OVERRIDE to 1 -> ACK"; L1 immediately prints "Lx 1: override
  cleared/expired - running safe clearance sequence" then "signal phase
  now CONNECTOR YELLOW" — proving the connector green was genuinely held
  only by the override, not coincidental with the PEAK_FIXED schedule.

### TC-UC08-3: Negative/alt 3.3 — a duration of 0 is rejected by Central before reaching the controller
- **Type**: Negative
- **Related**: UC-08 alt 3.3 ("Requested duration is invalid or unbounded"), BR-3
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: L1 running normally, no override active.
- **Steps**:
  1. Press `o` → Lx `1` → target movement `0` → duration_ms `0`.
- **Expected Result**: C1 logs "Operator: REQUEST_OVERRIDE(target=1,
  movement=0, duration_ms=0) rejected by Central pre-check,
  reason=INVALID_DURATION - not forwarded to the controller" — the
  request is **never** sent to L1 (`c_mode_eng_validate_override_request()`
  blocks it beforehand, `c_operator.c` lines 248-254). L1 has no override
  log at all.

### TC-UC08-4: Negative/alt 3.2 — an active railway pre-emption causes the override to be NACKed
- **Type**: Negative
- **Related**: UC-08 alt 3.2 ("Railway pre-emption conflicts with the requested movement"), BR-2
- **Environment**: (B) `c_main` + `lx_main 1` + `rlx_main 1`
- **Setup**: trigger pre-emption on L1 first (press `0` on RL1, wait at least 2 seconds for L1 to receive `CROSSING_STATUS(WARNING)`).
- **Steps**:
  1. Once L1 is confirmed `RAILWAY_PREEMPTION`, press `o` → Lx `1` → target movement `0` → duration_ms `10000`.
- **Expected Result**: C1 logs "C1: REQUEST_OVERRIDE to 1 -> NACK
  reason=RAILWAY_CONFLICT" (`lx_fsm.c` lines 705-709, checks
  `SUPERVISORY_RAILWAY_PREEMPTION` before even the ped-clearance check).
  L1 stays in `RAILWAY_PREEMPTION`, no override is activated.

### TC-UC08-5: Negative/alt 3.1 — an override is deferred (ACK_PENDING) while pedestrian service is running, then auto-activates once done
- **Type**: Negative
- **Related**: UC-08 alt 3.1 ("Pedestrian clearance is in progress"), BR-6
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: L1 at `PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. On L1's `lx_sensor`, press `1` (ped side 0) to start WALK/FDW (10 seconds total).
  2. While WALK is still running (e.g. 1 second after pressing `1`), on C1 press `o` → Lx `1` → target movement `0` → duration_ms `15000`.
  3. Watch C1's and L1's logs until second 12.
- **Expected Result**: C1 logs "C1: REQUEST_OVERRIDE to 1 -> ACK_PENDING"
  (since `fsm->ped_clearance_active==1` → `OVR_PENDING_CLEARANCE` branch,
  `lx_fsm.c` lines 713-728). `override_remaining_ms` starts counting down
  from 15000ms **even while pending** (lines 895-902). When WALK/FDW
  finishes at second 10 (from the `1` keypress), `ped_clearance_active`
  goes to 0 and on the very next tick `override_substate` auto-switches to
  `OVR_ACTIVE` (lines 935-938) **with no second reply sent** — this can
  only be observed indirectly, either by `ARTERIAL_GREEN` not exiting at
  the normal 48s mark (if watched that long) or via C1's HMI table
  (SUPERVISORY column switching from 3 to 2 exactly then, if L1's next
  STATUS/HEARTBEAT is captured).

### TC-UC08-6: Edge — the 300000/300001 ms boundary, and sending a second REQUEST_OVERRIDE while one is already ACTIVE
- **Type**: Edge case
- **Related**: UC-08 BR-5 ("bounded and auto-expiring"), BR-7, compliance-audit fix against overwriting an active override
- **Environment**: (B) `c_main` + `lx_main 1`
- **Setup**: L1 running normally, no override active.
- **Steps**:
  1. Press `o` → Lx `1` → target movement `0` → duration_ms `300001`.
  2. Press `o` → Lx `1` → target movement `0` → duration_ms `300000`.
  3. Right after step 2 is ACKed (still `OVR_ACTIVE`), press `o` → Lx `1` → target movement `1` → duration_ms `5000`.
- **Expected Result**: step 1 is blocked by the Central pre-check ("...
  rejected by Central pre-check, reason=INVALID_DURATION - not forwarded
  ...", since 300001 > 300000 = `LX_OVERRIDE_DURATION_CAP_MS`). Step 2 is
  forwarded by Central and L1 returns `ACK` (300000 equals the cap exactly
  — valid, closed boundary, not rejected). Step 3 passes the Central
  pre-check (target/duration both valid) but is rejected by **L1**: "C1:
  REQUEST_OVERRIDE to 1 -> NACK reason=OUT_OF_RANGE" — since
  `fsm->supervisory==SUPERVISORY_CENTRAL_OVERRIDE` is already true (from
  step 2), triggering the guard "no second REQUEST_OVERRIDE may overwrite
  an active one, must use RENEW_OVERRIDE" (`lx_fsm.c` lines 686-700). The
  override from step 2 (target=arterial, 300000ms) keeps running unaffected
  by the NACKed request in step 3.

---

## UC-09 — Monitor Network Status and Faults

### TC-UC09-1: Positive — the status table shows all 9 controllers once every node has reported
- **Type**: Positive
- **Related**: UC-09 main flow steps 1-8, BR-1
- **Environment**: (B) `c_main` + `lx_main 1`..`lx_main 6` + `rlx_main 1`..`rlx_main 3` (10 processes total on the same machine)
- **Setup**: start in the recommended order: `c_main` first, then the 3 `rlx_main`, then the 6 `lx_main` (section 2.3 `QNX_DEPLOYMENT_RUN_GUIDE.md`).
- **Steps**:
  1. Start all 10 processes as above.
  2. Wait 3 seconds (at least 2-3 heartbeat ticks).
  3. Read the "---- C1 network status ----" table printed every second on `c_main`'s console.
- **Expected Result**: the table has exactly 9 rows (L1..L6, RL1..RL3), the
  `AVAILABILITY` column = `AVAILABLE` for all 9, the `ROLE` column is
  correctly `INTERSECTION`/`RAILWAY`, the `MODE`/`PHASE` columns for the
  Lx entries = `0` (`MODE_PEAK_FIXED`) and `0` (`PHASE_ARTERIAL_GREEN`,
  just started), the `CROSSING_STATE` column for the RLx entries = `0`
  (`CROSSING_OPEN`), the `SUPERVISORY` column = `3` (`NORMAL_OPERATION`)
  for the Lx entries.

### TC-UC09-2: Negative/alt 5.1 — a controller that never started is marked UNAVAILABLE exactly after 3 seconds
- **Type**: Negative
- **Related**: UC-09 alt 5.1 ("A controller becomes unreachable"), BR-2 (PA-07)
- **Environment**: (B) `c_main` + `lx_main 1` (the remaining controllers **not** running)
- **Setup**: only start `c_main` then `lx_main 1`.
- **Steps**:
  1. Start `c_main`.
  2. Right after, start `lx_main 1`.
  3. Watch C1's log for 5 seconds.
- **Expected Result**: exactly at the 3rd tick (around `t=3s` from
  `c_main`'s start, since `missed_heartbeat_ticks` increments every second
  and no other controller ever reported to reset it to 0), C1 prints
  exactly 8 lines in a row of the form "Controller N marked UNAVAILABLE -
  missed 3 consecutive heartbeats (PA-07)" for N = 2,3,4,5,6,7,8,9 (L2..L6,
  RL1..RL3) — **no** such line for controller 1 (L1), since L1 sent its
  first HEARTBEAT within 1 second of starting, continuously resetting
  `missed_heartbeat_ticks` to 0. The HMI table confirms L1 = AVAILABLE, the
  other 8 = UNAVAILABLE.

### TC-UC09-3: Positive/alt 2.1 — a reconnecting controller replaces the stale view only once it has fully reported
- **Type**: Positive
- **Related**: UC-09 alt 2.1 ("A previously unavailable controller reconnects"), BR-3 (PA-08)
- **Environment**: (B) continues from TC-UC09-2
- **Setup**: end state of TC-UC09-2 (L2 UNAVAILABLE, never started).
- **Steps**:
  1. Start `lx_main 2` (late).
  2. Watch the log/HMI table for the next 2 seconds.
- **Expected Result**: within 1 second (L2's first heartbeat), L2's row in
  the HMI table switches immediately from `UNAVAILABLE` to `AVAILABLE`
  with fresh data (`MODE`/`PHASE` matching L2's real startup state, not
  stale/garbage data) — since `c_server_record_status()` sets
  `missed_heartbeat_ticks=0` and `marked_unavailable=0` as soon as the
  first `MSG_HEARTBEAT` is received (`c_server.c` lines 10-11), no need to
  wait for further cycles.

### TC-UC09-4: Negative/alt 6.1 — a railway fault report displays on Central independently of the local safety already in place
- **Type**: Negative
- **Related**: UC-09 alt 6.1 ("A railway fault report arrives")
- **Environment**: (B) `c_main` + `rlx_main 1`
- **Setup**: `c_main` and `rlx_main 1` running normally.
- **Steps**:
  1. On RL1, press `x` then `0` (as in TC-UC06-1), wait until `t=20s` to enter `FAULT`.
  2. Watch C1's HMI table for the next 3 seconds.
- **Expected Result**: RL1 has already closed its gates and held STOP
  exactly at `t=20s`, **before** the `FAULT_REPORT` reaches C1 (local
  safety is independent, RC-10). Within 1 second after that, RL1's row on
  C1's HMI table updates `CROSSING_STATE=3` (`CROSSING_FAULT`) and
  `FAULTS=0x1`; C1 acts purely as a display, sending no actuation command
  back to RL1 (per BR-1 "Central only monitors the 9 controllers, it does
  not directly control equipment").

### TC-UC09-5 (Regression, cosmetic): the SENSOR/OVERRIDE columns in the HMI table now align between header and data
- **Type**: Regression (fixed) — purely cosmetic, no logic impact
- **Why it was a bug**: `c_hmi_render()`'s header line (`printf` declaring
  column widths) and its data line (`printf` printing values) used to use
  different widths for the `SENSOR`/`OVERRIDE` columns (`%-9s`/`%-8u` in
  the data row vs. `%-8s`/`%-9s` declared in the header) — misaligning
  these 2 columns, hard to read during a demo even though the data itself
  was correct.
- **Related**: `c_hmi.c`'s `c_hmi_render()` — the 2 `printf` format
  strings (header and data row), column widths now match.
- **Environment**: (B) any, just needs `c_main` + at least 1 Lx/RLx running.
- **Steps**: start `c_main` + `lx_main 1`, wait for the HMI table to print at least once.
- **Expected Result**: visually (or by measuring character position): the
  `SENSOR` and `OVERRIDE` headers must align with their corresponding
  values in L1's data row — no left/right misalignment as before the fix.

---

## UC-10 — Continue Local Operation During Central Link Loss

### TC-UC10-1: Positive — an Lx keeps running its correct schedule without Central
- **Type**: Positive
- **Related**: UC-10 main flow steps 1-6, alt 7.1
- **Environment**: (A) `lx_main 1` only (no `c_main` running)
- **Setup**: do not start `c_main`.
- **Steps**:
  1. Run `/tmp/lx_main 1`.
  2. Watch the log continuously for 90 seconds (exactly 1 `LX_CYCLE_LENGTH_MS`).
- **Expected Result**: the "signal phase now ..." sequence appears exactly
  like TC-UC01-1 (48s/4s/2s/30s/4s/2s), with no stall/hang anywhere even
  though `lx_comm_send_heartbeat()` calls `ipc_client_post()` toward C1
  every second and fails (`name_open("traffic/c1")` finds no process) —
  the secondary log "Lx: HEARTBEAT to 0 failed to send" (from
  `on_heartbeat_reply()` in `lx_comm.c`) may appear every second but
  **does not** slow or block the main phase loop, since heartbeat sends
  always go through the non-blocking queue (`ipc_client_post()`),
  completely decoupled from the server thread running
  `lx_fsm_on_phase_timer()`.

### TC-UC10-2: Positive/alt 6.1 — railway protection still works fully even with no Central present
- **Type**: Positive
- **Related**: UC-10 alt 6.1 ("Railway event occurs while disconnected")
- **Environment**: (B) `lx_main 1` + `rlx_main 1` (RL1 adjacent to L1), **without** running `c_main`
- **Setup**: only these 2 processes, no C1.
- **Steps**:
  1. On RL1, press `0` at `t=0`.
  2. Watch L1's and RL1's logs continuously until `t=55s`.
- **Expected Result**: the full WARNING→CLOSING→CLOSED→TRAIN_PRESENT→
  OPENING→OPEN sequence on RL1 unfolds exactly like TC-UC04-1 (the same
  5s/8s/28s/48s/51s marks), and L1 still receives `MSG_CROSSING_STATUS`
  directly from RL1 (sent straight over Qnet, not through C1 —
  `rlx_comm.c`'s `send_crossing_status()` calls `adjacent_lx[]` directly),
  shown by L1's log having **no** "signal phase now CONNECTOR GREEN" line
  throughout the pre-emption window, same as TC-UC01-5 — proving railway
  protection is fully independent of Central (RC-05/RC-10 apply across
  both UC-04 and UC-10).

### TC-UC10-3: Positive — full state is resent as soon as Central restarts (no need to restart L1)
- **Type**: Positive
- **Related**: UC-10 main flow steps 7-9, BR-3 (PA-08)
- **Environment**: (B) `lx_main 1` starts first, `c_main` starts later
- **Setup**: run `lx_main 1` alone first (as in TC-UC10-1), let it run independently for at least 10 seconds.
- **Steps**:
  1. Once L1 has run independently for ≥10 seconds, start `/tmp/c_main`.
  2. Watch C1's HMI table for 2 seconds after it finishes starting.
- **Expected Result**: since C1's `ipc_attach()` uses
  `NAME_FLAG_ATTACH_GLOBAL` and L1 only succeeds at
  `name_open("traffic/c1")` starting from the heartbeat tick right after
  C1 has finished attaching, L1's row appears on C1's HMI table within 1
  second of C1 being ready, with `AVAILABILITY=AVAILABLE` and data
  reflecting L1's **current** state (e.g. if L1 is in `CONNECTOR_GREEN`
  after 10 seconds of independent running, the HMI must show that exact
  `PHASE`, not a startup default) — per the "send current state before
  accepting new commands" requirement in BR-3 (note: the current
  implementation sends full state in every normal `STATUS`/`HEARTBEAT`
  rather than having a separate "sync" step, so there is no mechanism to
  **block** new commands while syncing — a known simplification versus the
  spec text, not a bug).

### TC-UC10-4: Edge — a REQUEST_OVERRIDE sent to a controller that never started is still recorded "in-flight" by Central despite the send failing
- **Type**: Edge case
- **Related**: UC-10 contrasted with UC-08 when the peer is unavailable — a notable optimistic-bookkeeping finding
- **Environment**: (B) `c_main` running, `lx_main 2` **not** running
- **Setup**: only `c_main` running, L2 never started.
- **Steps**:
  1. Press `o` → Lx number `2` → target movement `0` → duration_ms `10000`.
- **Expected Result**: Central's pre-check accepts it (target=2 valid,
  duration valid) → logs "Operator: REQUEST_OVERRIDE(target=2, movement=0,
  duration_ms=10000) submitted", and **immediately** (before knowing
  whether the send succeeds) sets
  `mode_eng->controllers[1].override_in_flight` = 1 (optimistic
  bookkeeping, `c_operator.c` lines 227-246, comment "Not synchronised
  with the Lx's own eventual ACK/NACK"). Since L2 is not running,
  `ipc_client_post()` fails to send → logs "C1: REQUEST_OVERRIDE to 2 send
  failed (peer unreachable or send error)". C1's HMI table still shows
  L2 = `UNAVAILABLE`, but internally Central still considers the override
  "in flight" until the operator manually presses `c` (CANCEL_OVERRIDE) to
  clear this flag — there is no automatic detection of the send failure to
  roll back `override_in_flight`. This is correct behavior per the current
  code, noted here as an operational caveat (the operator should not fully
  trust this flag when the target controller is unreachable).
