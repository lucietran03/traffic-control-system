# 04. Test Plan — Verifying Timing Assumptions

This document tests whether the **numeric/timing values** published in
`system_assumptions_tables.md` (TC-01..05, TL-01..06, DP-01/02, CC-01/03,
RC-03/04/06, PA-07, PA-11/12) actually match the runtime behavior of the
system when running on real QNX Neutrino. Purely physical/planning
assumptions that cannot be observed by running the program (NU-01, RC-08,
TC-01's 350/400/320/380 m distances, RC-08's 80 km/h train speed, etc.)
are **not** in scope for this document.

Every figure below has been cross-checked directly against the source code
at the time of writing (no guesswork) — see the "Code source" column in
table 0.3.

---

## 0. General Conventions

### 0.1 Three Test Environments

| Symbol | Description | When to use |
| --- | --- | --- |
| **(A)** | Single node: only one process (`lx_main`, `rlx_main`, or `c_main`) running on one target/VM QNX. No `TRAFFIC_NODE_MAP` needed. | Testing the internal timing constants of a single FSM (TL-xx, RC-xx's internal timer sections, local PA-11/12). |
| **(B)** | Multiple nodes on the same QNX machine/VM (multiple `lx_main`/`rlx_main`/`c_main` processes running on the same target, communicating over internal Qnet loopback, no `TRAFFIC_NODE_MAP` needed since "same node" is the default). | Testing multi-controller interaction (PA-07 watchdog, RC-xx with both `Lx` and `RLx`, PA-11/12 C1↔Lx round-trip) when 2-3 physical machines aren't available. |
| **(C)** | Multiple real QNX VMs/machines over a real Qnet network (`TRAFFIC_NODE_MAP` required, see `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` section 2.2–2.3 and Case 1/2/3). | **Mandatory for TC-01..05** because the green-wave offset only has real test significance when `L1/L3/L5` (or `L2/L4/L6`) are independent processes, started at staggered times, synchronized **solely** via the wall clock — exactly as assumed by the `lx_fsm_apply_offset_locked()` algorithm. Can be downgraded to (B) if running on the same machine is acceptable (see the "limitation" note in each TC-TIME test case). |

Example declaration for (C), taken from `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`:
```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"
```

### 0.2 Measurement Tolerance — why millisecond-level precision isn't required

- **The signal-phase/railway tick is fixed at 100 ms** (`LX_PHASE_TICK_MS`, `lx_timer.h:53`) for `Lx`, and 1000 ms for `RLx`/heartbeat/watchdog. Any time threshold can only be updated at multiples of that tick — there is no meaning in requiring precision below 100 ms (Lx) or below 1 s (RLx/PA-07).
- **There is no monotonic clock shared across nodes.** `lx_fsm_apply_offset_locked()` (`app/intersection/src/lx_fsm.c:564-614`) synchronizes the green-wave offset using `clock_gettime(CLOCK_REALTIME, ...)` — i.e. each machine/VM's **wall clock**. If the system clocks of the VMs drift apart (no NTP/time sync), that error propagates directly into the measured offset error. **Before running any TC-TIME test, the wall clocks of all participating VMs must be synchronized** (e.g. manual `date` or NTP if outbound network access is available), and the remaining drift recorded (should be < 200 ms).
- **`C1`'s log (`c_logger.c`) has only 1-second resolution** (`strftime("%Y-%m-%d %H:%M:%S", ...)`, `app/central/src/c_logger.c:29-31`) — no milliseconds. Using this log to measure the gap between two events a few seconds apart is acceptable (±1 s rounding error), but it **must not** be used to measure intervals < 2 s.
- **`Lx`/`RLx` logs (`lx_signal.c`, `rlx_signal.c`, `rlx_gate.c`) have NO timestamp at all** — just plain `printf` (e.g. `"Lx %d: signal phase now %s\n"`, `app/intersection/src/lx_signal.c:42`). Therefore, any test case that needs to measure time between two `Lx`/`RLx` log lines **must** use one of two methods:
  1. **Manual stopwatch** (phone): start timing as soon as the log line appears on the SSH console, stop when the next log line appears. This method has an estimated user error of **±300–500 ms** (button-press reaction time) — added on top of the system tolerance.
  2. Redirect stdout through a timestamping wrapper before writing to file/console, e.g. (if the target shell supports it): `/tmp/lx_main 1 | while IFS= read -r line; do echo "$(date +%T.%3N) $line"; done > /tmp/l1.log`. This is more accurate than a stopwatch but depends on whether `date` supports milliseconds (`%3N`) on the QNX shell in use — check beforehand; if unsupported, fall back to method 1.
- **Default suggested tolerance for any test measured with a manual stopwatch:** expected value ± (500 ms + 1 relevant tick). Specific test cases below state the exact figure that applies.

### 0.3 Reference Timing Constants Table — cross-checked directly against code (source of truth)

| Constant | Value | File:line | Matches assumption |
| --- | --- | --- | --- |
| `LX_PEAK_ARTERIAL_GREEN_MS` | 48000 | `lx_timer.h:23` | TL-02 |
| `LX_PEAK_CONNECTOR_GREEN_MS` | 30000 | `lx_timer.h:24` | TL-02 |
| `LX_YELLOW_MS` | 4000 | `lx_timer.h:27` | TL-01 |
| `LX_ALL_RED_MS` | 2000 | `lx_timer.h:28` | TL-01 |
| `LX_MIN_GREEN_MS` | 8000 | `lx_timer.h:31` | TL-01 |
| `LX_MAX_GREEN_MS` | 40000 | `lx_timer.h:32` | TL-01 |
| `LX_EXTENSION_MS` | 4000 | `lx_timer.h:33` | TL-03/CC-03 |
| `LX_OVERRIDE_DURATION_CAP_MS` | 300000 | `lx_timer.h:36` | PA-11 |
| `LX_PHASE_TICK_MS` | 100 | `lx_timer.h:53` | (system tick) |
| `LX_WALK_MS` | 6000 | `lx_timer.h:70` | TL-05 (placeholder, not a spec-mandated value) |
| `LX_FLASHING_DONT_WALK_MS` | 4000 | `lx_timer.h:71` | TL-05 (placeholder) |
| `LX_DRAIN_MAX_EXTENSION_MS` | 60000 | `lx_timer.h:84` | CC-03 |
| `LX_CYCLE_LENGTH_MS` | 90000 (= 48+4+2+30+4+2 s, computed from the constants above) | `lx_timer.h:94-95` | TL-02/TC-02 |
| `R1_L1_OFFSET_MS` / `R1_L3_OFFSET_MS` / `R1_L5_OFFSET_MS` | 0 / 21000 / 45000 | `c_mode_eng.h:53-55` | TC-02 |
| `R2_L2_OFFSET_MS` / `R2_L4_OFFSET_MS` / `R2_L6_OFFSET_MS` | 0 / 19000 / 42000 | `c_mode_eng.h:56-58` | TC-02 |
| `C_MODE_ENG_DEFAULT_PEAK_START_HOUR` / `_END_HOUR` | 6 / 9 | `c_mode_eng.h:49-50` | DP-02 (customizable placeholder) |
| NACK offset threshold | `offset_ms >= LX_CYCLE_LENGTH_MS` (90000) → NACK | `lx_fsm.c:625-636` | PA-09 |
| `RLX_WARNING_TO_CLOSING_MS` | 5000 | `rlx_timer.h:15` | RC-03 |
| `RLX_CLOSING_DEADLINE_MS` | 15000 (measured **from entry into the CLOSING state**, i.e. **5000+15000=20000 ms from TRAIN_APPROACHING** before the fault is raised — matching Appendix B4's "20 s" mark, not 15 s) | `rlx_timer.h:16`; applied at `rlx_fsm.c:139` (`enter_closing()` resets `state_elapsed_ms=0` at `rlx_fsm.c:162-167`) | RC-03 |
| `RLX_OCCUPANCY_WINDOW_MS` | 20000 | `rlx_timer.h:21` | RC-04 |
| `RLX_OPENING_DEADLINE_MS` | 15000 (from entry into the OPENING state) | `rlx_timer.h:22` | RC-06 (internal value, no direct corresponding figure in the main table) |
| `RLX_GATE_MOTION_MS` | 3000 (simulated gate-travel time — used to know the normal path confirms CLOSED/OPEN much faster than the deadline) | `rlx_gate.h:10` | (supports RC-03/RC-06) |
| Heartbeat period | 1000 ms in both directions (`Lx`/`RLx` send, `C1` counts ticks) | `lx_main.c:179`, `c_main.c:174` | PA-07 |
| UNAVAILABLE threshold | `missed_heartbeat_ticks == 3` (1 Hz tick) → **2.0–3.0 s** from the last heartbeat, not exactly 3.000 s (see TC PA-TIME-02) | `c_watchdog_mon.c:4-16` | PA-07 |
| Override duration cap | `duration_ms == 0 \|\| duration_ms > 300000` → NACK (both at `Lx` and at `C1`'s validation layer) | `lx_fsm.c:701`, `c_mode_eng.c:113` | PA-11 |

**Important note found while reading the code (affects how DP-01/02 tests are written):**
`c_mode_eng_select_mode()` (`app/central/src/c_mode_eng.c:63-69`) is still a
pure function that takes `current_hour` as a parameter **supplied by the
caller** — the function itself does not read the system clock. However,
`c_main.c`'s `on_pulse()` (the `IPC_PULSE_HEARTBEAT_TICK` branch,
`app/central/src/c_main.c:149-220`) is exactly that caller and runs at 1 Hz:
if `ctx->mode_eng.demo_hour_override_active` is not set, it calls
`time(NULL)` then `localtime_r()` (`c_main.c:189-193`) to get the real wall
clock hour, then calls `c_mode_eng_auto_check(&ctx->mode_eng, current_hour,
&auto_mode)` (`app/central/src/c_mode_eng.c:71-93`) and broadcasts
`SET_MODE` to all 6 `Lx` via `c_comm_broadcast_set_mode()`
(`c_main.c:215-218`) whenever the schedule-driven mode actually changes from
the last check, along with the log line `"Auto peak-hour switch: hour=%u ->
mode=%s, broadcasting to all Lx"`. In other words: **automatic switching
between `PEAK_FIXED` ↔ `OFF_PEAK_SENSOR` based on the wall-clock hour
(DP-02) IS already wired into the running system** — this is no longer a
real gap. `c_operator.c` also has key `d` (`handle_demo_hour()`,
`c_operator.c:408-433`) to force a simulated hour
(`mode_eng->demo_hour`/`demo_hour_override_active`) to stand in for "the
current hour", and key `a` (`handle_resume_automatic()`,
`c_operator.c:439-447`) to switch back to the real wall clock — allowing a
tester to force the system right up to the peak/off-peak boundary without
waiting for real time to pass. The DP-TIME-xx test cases below reflect this
already-wired behavior and use key `d` to create an observable boundary
within a short test session.

---

## 1. TC-01..05 — Green-Wave Offset

> Required environment: **(C)** to allow real measurement between
> independent nodes; may be downgraded to (B) if identical system clocks
> (same machine) is acceptable — in that case the test record must note
> "run on (B), inter-VM clock synchronization not verified".

### TC-TIME-01: Offset R1 — L1 → L3 = 21 s
- **Type**: Positive
- **Related**: TC-02, `R1_L3_OFFSET_MS = 21000` (`c_mode_eng.h:54`)
- **Environment**: (C) — `L1`, `L3`, `L5` run on 3 different VMs (or at minimum `L1`/`L3` on 2 different VMs), `C1` on its own VM, wall clocks synchronized across VMs (drift measured beforehand < 200 ms via `date` on each VM).
- **Setup**: Start `c_main`, `rlx_main 1..3`, `lx_main 1..6` in the exact order given in `QNX_DEPLOYMENT_RUN_GUIDE.md` section 2.3. Let the system run stably in `MODE_PEAK_FIXED` (the default mode at startup, `lx_fsm.c:360`) for at least 1 full 90 s cycle before starting measurement, so that `assigned_offset_ms` (sent from `C1` via `SET_TIMING_PROFILE`, UC-03) has already been applied to a new ARTERIAL_GREEN phase (see TC-TIME-04 regarding apply delay).
- **Steps**:
  1. On `L1`'s console, wait for the log line `"Lx 1: signal phase now ARTERIAL GREEN"`.
  2. As soon as that line appears, press Start on a phone stopwatch.
  3. Switch to/watch `L3`'s console in parallel, wait for the `"Lx 3: signal phase now ARTERIAL GREEN"` line of the **immediately following cycle** (not some other random cycle).
  4. Press Stop the moment that line appears.
- **Expected result**: The measured time falls within **20.5–21.5 s** (21 s ± 500 ms for the 100 ms tick + user reaction delay + inter-VM clock sync error).

### TC-TIME-02: Offset R1 — L3 → L5 = 24 s (cumulative L1 → L5 = 45 s)
- **Type**: Positive
- **Related**: TC-02, `R1_L5_OFFSET_MS = 45000` (`c_mode_eng.h:55`); the `L3→L5` segment = 45000-21000 = 24000 ms.
- **Environment**: (C), continuing from the stable state established in TC-TIME-01.
- **Setup**: Same as TC-TIME-01.
- **Steps**: Repeat the TC-TIME-01 procedure, but start at `"Lx 3: signal phase now ARTERIAL GREEN"` and stop at `"Lx 5: signal phase now ARTERIAL GREEN"` of the next cycle. Also measure L1 → L5 directly (bridging 2 waiting cycles if needed) to cross-check the cumulative value.
- **Expected result**: L3 → L5 falls within **23.5–24.5 s**; L1 → L5 (measured directly or derived) falls within **44.5–45.5 s**.

### TC-TIME-03: Offset R2 — L2 → L4 = 19 s, L4 → L6 = 23 s (cumulative 42 s)
- **Type**: Positive
- **Related**: TC-02, `R2_L4_OFFSET_MS = 19000`, `R2_L6_OFFSET_MS = 42000` (`c_mode_eng.h:57-58`)
- **Environment**: (C)
- **Setup**: Same as TC-TIME-01, applied to the R2 chain (`L2`→`L4`→`L6`).
- **Steps**: Measure `"Lx 2: signal phase now ARTERIAL GREEN"` → `"Lx 4: signal phase now ARTERIAL GREEN"` (expected ~19 s) and `"Lx 4: ..."` → `"Lx 6: ..."` (expected ~23 s), using the same stopwatch method as above.
- **Expected result**: L2→L4 within **18.5–19.5 s**; L4→L6 within **22.5–23.5 s**.

### TC-TIME-04: Edge case — offset only applies on entering a new ARTERIAL_GREEN, does not interrupt the current phase
- **Type**: Edge case
- **Related**: TC-03, the `lx_fsm_apply_offset_locked()` algorithm (`lx_fsm.c:481-563`, especially the "Compliance-audit fix" section — the offset only sets the `offset_apply_pending` flag in `lx_fsm_on_set_timing_profile()`, `lx_fsm.c:645`, and is only actually applied at `lx_fsm.c:344` when a **new** `PHASE_ARTERIAL_GREEN` begins).
- **Environment**: (B) or (C) — only 1 `L1` + `C1` needed.
- **Setup**: Have `L1` run in `PEAK_FIXED`. Use the operator command tool (`C1` key `t`) to send `SET_TIMING_PROFILE` with a new `offset_ms` **right in the middle of `L1` displaying ARTERIAL GREEN** (watch `L1`'s console, send the command when you know at least > 20 s of green remains).
- **Steps**:
  1. Record the time the command was sent (via stopwatch) and the time the `"Lx 1: signal phase now ARTERIAL YELLOW"` line appears immediately after (end of the current ARTERIAL_GREEN phase).
  2. Measure the time from when the command was sent to when that ARTERIAL_GREEN phase **actually ends** (switches to YELLOW).
  3. Measure the duration of that ARTERIAL_GREEN phase **from its start** (not from when the command was sent) to the switch to YELLOW.
- **Expected result**: The ARTERIAL_GREEN phase **running at the time the command was sent** must have a total duration of exactly **48 s ± 200 ms** (not shortened/lengthened by the new offset) — i.e. the `SET_TIMING_PROFILE` command does not interrupt the current phase. The new offset is only observed starting from the **next** ARTERIAL_GREEN cycle (cross-check by repeating TC-TIME-01/02/03 after sending the command).

### TC-TIME-05: Negative — offset_ms = LX_CYCLE_LENGTH_MS (90000) is NACKed; 89999 is still ACKed
- **Type**: Negative + Edge case (boundary)
- **Related**: PA-09, checking `payload->offset_ms >= LX_CYCLE_LENGTH_MS` at `lx_fsm.c:625-636` (returns `RESULT_NACK` / `NACK_REASON_STALE_OR_UNSAFE_PROFILE`)
- **Environment**: (D) — requires a `test_client` tool (not present in the repo; see `docs/test-plan/03-protocol-contract.md` section "Environment conventions (A/B/C/D)" and "Proposed test_client tool"). `C1`'s operator console (`handle_timing_profile()`, `app/central/src/c_operator.c`) **is not** a valid way to create this case: this function only asks for a single integer, `1` or `2`, to select the fixed `R1`/`R2` chain (`c_mode_eng_get_chain()`, offsets taken from `R1_L1_OFFSET_MS=0`/`R1_L3_OFFSET_MS=21000`/`R1_L5_OFFSET_MS=45000`/`R2_L2_OFFSET_MS=0`/`R2_L4_OFFSET_MS=19000`/`R2_L6_OFFSET_MS=42000` in `app/central/includes/c_mode_eng.h`) — there is no way to enter an arbitrary `offset_ms` such as 89999 or 90000 directly via this console. Only a raw wire message from `test_client` can send an arbitrary `offset_ms`.
- **Setup**: `L1` running normally, no fault.
- **Steps**:
  1. `test_client` sends directly to `L1` an `ipc_request_t{verb=MSG_SET_TIMING_PROFILE, payload.timing_profile={offset_ms=89999}}` → observe `C1`'s log: `"C1: SET_TIMING_PROFILE to 1 -> ACK"`.
  2. `test_client` sends `offset_ms = 90000` (exactly `LX_CYCLE_LENGTH_MS`) → observe `C1`'s log.
  3. (Optional) Send `offset_ms = 90001` as well to confirm the same NACK behavior.
- **Expected result**: Step 1 → `ACK`. Steps 2 and 3 → `"C1: SET_TIMING_PROFILE to 1 -> NACK reason=STALE_OR_UNSAFE_PROFILE"` (name printed by `nack_reason_name()`, `c_comm.c:57`). This is the correct N-1/N boundary: 89999 is valid, 90000 (N) is rejected.
- **Cross-reference**: This case shares the same boundary as `docs/test-plan/03-protocol-contract.md`'s TC-MSG-3 (`offset_ms=90000` → NACK) and TC-MSG-4 (`offset_ms=89999` → ACK), section 1 "MSG_SET_TIMING_PROFILE" — same environment (D), same test_client mechanism.

---

## 2. TL-01..06 — Traffic Signal Phase Timing

### TL-TIME-01: Positive — 8 s minimum green is honored when there is no demand
- **Type**: Positive
- **Related**: TL-01, `LX_MIN_GREEN_MS = 8000` (`lx_timer.h:31`), enforced at `lx_timer_should_exit_green()` (`lx_timer.c:19-30`: `if (elapsed_ms < LX_MIN_GREEN_MS) return 0`)
- **Environment**: (A) — 1 standalone `L1`.
- **Setup**: Switch `L1` to `MODE_OFF_PEAK_SENSOR` (via `SET_MODE` command from `C1`, or wait if the default mode differs — confirm the current mode via log/HMI before testing). Ensure there is no demand (arterial/connector) before ARTERIAL_GREEN begins — use the sensor keys to clear demand (uppercase `C`/`A` = clear).
- **Steps**: Press Start as soon as `"Lx 1: signal phase now ARTERIAL GREEN"` appears, without creating any demand throughout the session. Press Stop when `"Lx 1: signal phase now ARTERIAL YELLOW"` appears.
- **Expected result**: The measured time is **≥ 8.0 s** (must not end earlier), and within **8.0–8.5 s** since the extension check only runs at multiples of 4000 ms and 8000 ms is the first exit-check point when there is no demand (`lx_fsm.c:981` `if ((green_elapsed_ms % LX_EXTENSION_MS) == 0)`).

### TL-TIME-02: Edge case — the exact boundary of 7.9 s vs. 8.0 s
- **Type**: Edge case
- **Related**: TL-01, same mechanism as TL-TIME-01, boundary `elapsed_ms < 8000` (held) vs. `elapsed_ms == 8000` (allowed to exit).
- **Environment**: (A)
- **Setup**: Same as TL-TIME-01, no demand.
- **Steps**: Precisely record the moment the ARTERIAL_GREEN phase starts and watch continuously until it switches to YELLOW. Since the system tick is 100 ms, the real "N-1/N" points to verify are **7.9 s (must not yet exit) and 8.0 s (the first valid exit-check point)**, not fractional milliseconds (the system has no resolution below 100 ms).
- **Expected result**: `ARTERIAL YELLOW` must absolutely not be observed before the 7.9 s mark (measurement tolerance −0/+300 ms due to manual stopwatch); the phase must end within the 8.0–8.5 s window as in TL-TIME-01 if there is no demand.

### TL-TIME-03: Positive — 40 s max green forces a phase exit even with demand still present
- **Type**: Positive
- **Related**: TL-01, `LX_MAX_GREEN_MS = 40000` (`lx_timer.h:32`), `maxed = elapsed_ms >= LX_MAX_GREEN_MS` (`lx_timer.c:27-29`)
- **Environment**: (A)
- **Setup**: `MODE_OFF_PEAK_SENSOR`. Create continuous arterial demand (key `a` held "present" and never press `A` to clear) throughout the phase.
- **Steps**: Press Start at `ARTERIAL GREEN`, hold demand continuously, Stop at `ARTERIAL YELLOW`.
- **Expected result**: The phase ends within **40.0–40.5 s** even though demand is still present (proving the 40 s cap wins over continuous demand), must not exceed 40.5 s.

### TL-TIME-04: Edge case — boundary of 39.9 s (not yet maxed, holds if demand remains) vs. 40.0 s (maxed, forced exit)
- **Type**: Edge case
- **Related**: TL-01, same mechanism as TL-TIME-03.
- **Environment**: (A)
- **Setup**: Same as TL-TIME-03.
- **Steps**: Watch continuously around the 39.9–40.1 s mark (measured via stopwatch, accept ±300 ms error since this is a visual observation via console).
- **Expected result**: At ~39.9 s the phase is still ARTERIAL GREEN (since `elapsed_ms < 40000`, and demand remains so `should_exit` returns 0 while not yet maxed); the phase must switch to YELLOW within the 40.0–40.5 s window regardless of demand.

### TL-TIME-05: Positive — yellow is exactly 4 s
- **Type**: Positive
- **Related**: TL-01, `LX_YELLOW_MS = 4000` (`lx_timer.h:27`), checked via `green_elapsed_ms >= LX_YELLOW_MS` at `lx_fsm.c:944`
- **Environment**: (A), applies to both `PEAK_FIXED` and `OFF_PEAK_SENSOR` (shared constant).
- **Setup**: Any mode, wait for a YELLOW phase to appear.
- **Steps**: Press Start at `"signal phase now ARTERIAL YELLOW"` (or CONNECTOR YELLOW), Stop at the following `"signal phase now ALL RED (...)"`.
- **Expected result**: **3.9–4.1 s** (4 s ± 100 ms tick + ±300 ms manual timing error → rounded acceptable tolerance **3.6–4.4 s**).

### TL-TIME-06: Positive — all-red clearance is exactly 2 s
- **Type**: Positive
- **Related**: TL-01, `LX_ALL_RED_MS = 2000` (`lx_timer.h:28`), checked at `lx_fsm.c:951`
- **Environment**: (A)
- **Setup**: Same as above.
- **Steps**: Press Start at `"signal phase now ALL RED (A to B)"`, Stop at `"signal phase now CONNECTOR GREEN"` (or the corresponding next phase).
- **Expected result**: **1.6–2.4 s** (2 s ± 100 ms tick + ±300 ms manual error).

### TL-TIME-07: Positive — pedestrian sequence WALK (6 s) → FLASHING_DONT_WALK (4 s), total 10 s
- **Type**: Positive
- **Related**: TL-05/TL-06, `LX_WALK_MS = 6000`, `LX_FLASHING_DONT_WALK_MS = 4000` (`lx_timer.h:70-71` — **note this is the team's self-chosen placeholder value, not a value mandated by the spec**, but it must still match the exact figure that was coded).
- **Environment**: (A)
- **Setup**: In a compatible phase (e.g. ARTERIAL_GREEN), press the pedestrian button (key `1`) to create a `PED_REQUEST` on a compatible side.
- **Steps**: Press Start at `"PED SIGNAL side 0 -> WALK"`, lap at `"PED SIGNAL side 0 -> FLASHING_DONT_WALK"`, Stop at `"PED SIGNAL side 0 -> DONT_WALK"`.
- **Expected result**: WALK lasts **5.6–6.4 s**, FLASHING_DONT_WALK lasts **3.6–4.4 s**, total sequence **9.6–10.4 s**.

### TL-TIME-08: Negative/integrity — total PEAK_FIXED cycle must equal exactly 90 s
- **Type**: Negative (checking for no cumulative drift)
- **Related**: TL-02, `LX_CYCLE_LENGTH_MS = 90000` (`lx_timer.h:94-95`, computed from 48+4+2+30+4+2)
- **Environment**: (A)
- **Setup**: `MODE_PEAK_FIXED`, no override/railway pre-emption/fault occurring during measurement (any such interference invalidates this measurement).
- **Steps**: Press Start at any `"signal phase now ARTERIAL GREEN"` occurrence, Stop at the **next** `"signal phase now ARTERIAL GREEN"` occurrence (exactly 1 full cycle: arterial green+yellow+all-red+connector green+yellow+all-red).
- **Expected result**: **89.0–91.0 s** (90 s ± ~1 s, a looser tolerance than the tests above since this is the cumulative error of 6 manually-measured intervals). If the deviation exceeds ±1 s consistently across multiple cycles, suspect drift in `lx_fsm_on_phase_timer()` requiring further investigation (out of scope for this document).

---

## 3. DP-01/02 — PEAK_FIXED / OFF_PEAK_SENSOR Modes

> **Note before testing**: as stated in section 0.3, automatic mode
> switching based on the wall-clock hour **is already wired into the
> runtime** — `c_main.c`'s `on_pulse()` reads the real wall-clock hour (or a
> simulated hour forced via key `d`) every 1 Hz and calls
> `c_mode_eng_auto_check()`, broadcasting `SET_MODE` to all 6 `Lx` whenever
> the scheduled mode changes. The three test cases below exercise exactly
> this mechanism, using `c_operator.c`'s `d`/`a` keys to force the
> peak/off-peak boundary to occur within a short test session instead of
> waiting for real time to pass.

### DP-TIME-01: Positive — default placeholder values are exactly 6 and 9
- **Type**: Positive (checking source code/constants, not dynamic runtime behavior)
- **Related**: DP-02, `C_MODE_ENG_DEFAULT_PEAK_START_HOUR = 6`, `C_MODE_ENG_DEFAULT_PEAK_END_HOUR = 9` (`c_mode_eng.h:49-50`), initialized by `c_mode_eng_init()`.
- **Environment**: (A) — run `c_main` standalone.
- **Setup**: No special setup needed; this checks the values loaded into the struct at startup.
- **Steps**: If a debug/unit-test hook is available, call `c_mode_eng_select_mode(&eng, 6)`, `c_mode_eng_select_mode(&eng, 8)`, and `c_mode_eng_select_mode(&eng, 9)` after `c_mode_eng_init()` — run and print the results. If no such hook exists, verify by re-reading `central_log.txt`/the compiled source to confirm the constants were not altered at build time.
- **Expected result**: `select_mode(6)` → `MODE_PEAK_FIXED`, `select_mode(8)` → `MODE_PEAK_FIXED`, `select_mode(9)` → `MODE_OFF_PEAK_SENSOR` (boundary exactly at hour 9, see DP-TIME-02b).

### DP-TIME-02: Edge case — forcing a simulated hour across the 09:00 boundary via key `d`, confirming SET_MODE is broadcast to all 6 Lx (and the auto-detect log when using the real wall clock)
- **Type**: Edge case (hour boundary) + Positive (confirming the DP-02 auto-switch mechanism is wired and functioning correctly)
- **Related**: DP-02; `c_main.c`'s `on_pulse()` (`app/central/src/c_main.c:149-220`) calls `c_mode_eng_auto_check()` (`c_mode_eng.c:71-93`) every 1 Hz; `c_operator.c`'s `handle_demo_hour()`/`handle_resume_automatic()` (`c_operator.c:408-433`/`439-447`, keys `d`/`a`).
- **Environment**: (B) or (C) — `C1` + all 6 `Lx` (to confirm all 6 controllers receive `SET_MODE`, not just some).
- **Setup**: Start `c_main`, `lx_main 1..6`. On `C1`'s console, type `d`, enter hour `8` (still within the 06-09 peak window) to bring the system to a known `PEAK_FIXED` baseline.
- **Steps**:
  1. **Broadcast-confirmation branch (fast, deterministic)**: Type `d` again, enter hour `9` (exactly `C_MODE_ENG_DEFAULT_PEAK_END_HOUR`, the boundary to `OFF_PEAK_SENSOR`). Observe `central_log.txt` immediately: the line `"Operator: demo hour forced to 9 -> schedule implies mode=OFF_PEAK_SENSOR, broadcasting to all Lx"`, followed by 6 lines `"C1: SET_MODE to <id> -> ACK"` (id = 1..6). Confirm via HMI/each `Lx`'s log that `operating_mode` has switched to `OFF_PEAK_SENSOR` on all 6 controllers.
  2. **Real-automatic-path confirmation branch (using the real wall clock)**: Type `a` to disable the demo override (log `"Operator: resuming automatic (real clock) peak-hour switching"`). Since `handle_demo_hour()` synchronizes `last_auto_mode` before returning — so the next 1 Hz tick doesn't fire a duplicate broadcast — the distinct log line from `on_pulse()` itself, `"Auto peak-hour switch: hour=%u -> mode=%s, broadcasting to all Lx"` (`c_main.c:215-218`), **only** appears when `on_pulse()` itself detects a mode change from the real wall clock, not via key `d`. To observe this log line within a short test session (instead of waiting for real time to reach the boundary), adjust the system clock of the VM running `C1` (`date`, root privileges, test VM only) to just before 09:00:00 and let it pass that mark.
- **Expected result**: Step 1 confirms DP-02's broadcast-to-6-Lx mechanism works correctly via the `d` path (deterministic, fast, reuses the same `c_mode_eng_select_mode()`/`c_comm_broadcast_set_mode()` that `on_pulse()` uses). Step 2 further confirms that when there is NO demo override, `on_pulse()` itself detects the real hour boundary and fires the broadcast + `"Auto peak-hour switch: ..."` log without any operator action at the exact boundary moment. Both PASS results together confirm: DP-02 **has been wired** into the running system — contrary to the "not yet wired" conclusion of the earlier version of this document.

### DP-TIME-03: Positive — alternate path: manual SET_MODE via operator console
- **Type**: Positive
- **Related**: DP-01 (2 modes exist and can be switched), via the `MSG_SET_MODE` command (`lx_fsm_on_set_mode`, `lx_fsm.c:689+` — moved further down in the file after the TC-02/TC-03 fixes above), operator console key `m` (`c_operator.c`).
- **Environment**: (B) or (C)
- **Setup**: `C1` and `L1` running, `L1` in `MODE_PEAK_FIXED` (default).
- **Steps**: On `C1`'s console, press `m`, select `L1`, select mode `1` (OFF_PEAK_SENSOR). Observe `central_log.txt`.
- **Expected result**: The log line `"C1: SET_MODE to 1 -> ACK"` appears almost immediately (within 1 s by the logger's second-resolution clock — see also PA-TIME-06 regarding round-trip); `L1`'s signal-phase behavior switches to sensor-driven starting at the next safe phase boundary (TL-04).

---

## 4. CC-01/03 — Queue Detection & Drain Phase

### CC-TIME-01: Positive — drain phase extends in 4 s steps while QUEUE_WARNING remains active
- **Type**: Positive
- **Related**: CC-03, `LX_EXTENSION_MS = 4000` reused for the drain check cadence (`lx_fsm.c:1021` `if ((drain_extension_total_ms % LX_EXTENSION_MS) == 0)`)
- **Environment**: (B) or (C) — needs `RLx` to trigger the crossing-reopen scenario (drain is only armed via `drain_pending` when the crossing reopens with `queue_warning_active`, `lx_fsm.c:809-822`).
- **Setup**: Put the relevant `RLx` into a closed-gate state (simulating a train arriving), while also enabling `QUEUE_WARNING` on the relevant `Lx`'s approach connector (key `w`). Wait for the gate to reopen (see section 5 for gate timing).
- **Steps**: After the connector-drain phase begins, keep `QUEUE_WARNING` on continuously, measure the interval between "extensions" — since there is no dedicated log for each 4 s extension, measure the total duration of the connector-drain phase from its start until you deliberately turn off `QUEUE_WARNING` (key `W`) and observe it end **right at the next 4 s check point**, not immediately.
- **Expected result**: After turning off `QUEUE_WARNING`, the phase continues for up to nearly 4 more seconds before switching to YELLOW (since the condition is only re-checked at multiples of 4000 ms of `drain_extension_total_ms`) — the observed turn-off delay should fall within **0–4.3 s** from when the flag was cleared.

### CC-TIME-02: Edge case — the 60s hard cap applies ONLY to the EXTENSION PORTION, total actual phase reaches up to 90s
- **Type**: Edge case
- **Related**: CC-03, `LX_DRAIN_MAX_EXTENSION_MS = 60000` (`lx_timer.h:84`, comment "60 s cap on TOTAL granted extension (not on phase duration)"), checked via `drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS` (in `lx_fsm.c`'s `PHASE_CONNECTOR_GREEN` case). **Correcting an important misunderstanding**: `drain_extension_total_ms` is a SEPARATE counter, reset to 0 exactly at the point `green_elapsed_ms >= lx_timer_peak_green_duration_ms()` (30000ms in `PEAK_FIXED`), and only then does it start counting — it is NOT the total phase duration. The 60000ms cap only limits the **extension added after the normal 30s**, not the total connector-green phase duration. The actual total duration when the cap is hit is **30000 + 60000 = 90000ms**, not 60000ms. (This case previously incorrectly stated the total phase was capped at 60s — now corrected; see `01-usecase-functional.md`'s TC-UC05-3, which already correctly describes this 90000ms mark.)
- **Environment**: (B) or (C)
- **Setup**: Same as CC-TIME-01, but **keep `QUEUE_WARNING` on continuously and never turn it off**.
- **Steps**: Press Start as soon as the drain phase begins (the `"signal phase now CONNECTOR GREEN"` line right after the gate reports open, with `drain_pending` already armed beforehand). Stop when `"signal phase now CONNECTOR YELLOW"` appears.
- **Expected result**: The phase ends within **90.0–90.5 s** from the start of drain (30s normal + 60s maximum extension, must not exceed — the cap is hard, `>=` not `>`), regardless of `QUEUE_WARNING` still being active.

### CC-TIME-03: Positive — drain ends early as soon as QUEUE_WARNING naturally clears (does not wait the full 60 s)
- **Type**: Positive
- **Related**: CC-03 (end condition "queue warning clears OR 60s cap")
- **Environment**: (B) or (C)
- **Setup**: Same as CC-TIME-01, but turn off `QUEUE_WARNING` early, e.g. at second 12 (between two 4 s marks: 12000 ms is a multiple of 4000, chosen as an easily observable point).
- **Steps**: Turn off `QUEUE_WARNING` at second 12, measure when the phase switches to YELLOW.
- **Expected result**: The phase ends at **~12.0–16.0 s** (up to one more 4 s beat after turn-off due to the poll-at-multiples-of-4000ms mechanism), **not** extending to near 60 s.

---

## 5. RC-03/04/06 — Railway Crossing Warning & Gate Close/Open

Relevant logs (no timestamp, use stopwatch — see section 0.2):
- `"RLx: flashers ON (train approaching, direction %u)"` — start of WARNING (`rlx_signal.c:24`), corresponding to T0 of `TRAIN_APPROACHING`.
- `"RLx: commanding gates DOWN (simulated motion, 3000 ms)"` — start of CLOSING (`rlx_gate.c:48`).
- `"RLx: train signal PROCEED for direction %u (gates confirmed closed)"` — CLOSED confirmed (`rlx_signal.c:16`).
- `"RLx: all train signals -> STOP (crossing reopening)"` — start of OPENING (`rlx_signal.c:21`).
- `"RLx: flashers OFF (gates confirmed open)"` — OPEN confirmed (`rlx_signal.c:29`).
- `"RLx: FAULT latched (GATE_CONFIRM_MISSING, bit 0x...) ..."` — fault reported (`rlx_signal.c:49`).

### RC-TIME-01: Positive — warning-to-closing is exactly 5 s
- **Type**: Positive
- **Related**: RC-03, `RLX_WARNING_TO_CLOSING_MS = 5000` (`rlx_timer.h:15`), checked at `rlx_fsm.c:351`
- **Environment**: (A) — 1 standalone `RL1` (internal 1 Hz tick control).
- **Setup**: `RL1` in state `RLX_OPEN` (default at startup).
- **Steps**: Press key `0` (TRAIN_APPROACHING direction 0) on `rlx_sensor`. Press Start as soon as `"flashers ON"` appears. Press Stop when `"commanding gates DOWN"` appears.
- **Expected result**: **4.5–5.5 s** (5 s ± 500 ms, 1000 ms tick + manual error).

### RC-TIME-02: Edge case (fault path) — total budget of 20 s (not 15 s) before FAULT_GATE_CONFIRM_MISSING
- **Type**: Edge case + Negative (checking the error path)
- **Related**: RC-03/RC-06; **important finding from reading the code**: `RLX_CLOSING_DEADLINE_MS = 15000` is measured **from entry into the CLOSING state** (`state_elapsed_ms` reset to 0 in `enter_closing()`, `rlx_fsm.c:162-167`), NOT from `TRAIN_APPROACHING`. Therefore the actual total time before the fault is raised is **5000 (WARNING) + 15000 (CLOSING) = 20000 ms**, matching exactly the cumulative "20 s — closed-confirmation margin" mark in Appendix B4 of `system_assumptions_tables.md`, not 15 s as the constant's name might misleadingly suggest.
- **Environment**: (A)
- **Setup**: Press key `x` on `rlx_sensor` to arm "gate motion will NEVER confirm closed" (demo fault, `rlx_gate.c:120`, "RC-06 fault path"). Then press `0` to start TRAIN_APPROACHING.
- **Steps**: Press Start at `"flashers ON"`. Watch continuously, note the marks at 19.5 s (expected: no fault yet) and 20.0–20.5 s (expected: fault appears).
- **Expected result**: NO `"FAULT latched"` line before **19.5 s**; the line `"FAULT latched (GATE_CONFIRM_MISSING, bit ...) ... commanding gates DOWN"` MUST appear within the **20.0–21.0 s** window from `flashers ON` (a looser tolerance since it combines 2 discrete 1000ms ticks + manual error).

### RC-TIME-03: Positive — the normal path confirms CLOSED quickly (~8 s), with a large safety margin versus the 20 s deadline
- **Type**: Positive
- **Related**: RC-03/RC-06, the normal path does NOT arm the demo fault — `RLX_GATE_MOTION_MS = 3000` (`rlx_gate.h:10`) so the gate confirms closed ~3 s after entering CLOSING, i.e. ~8 s after `TRAIN_APPROACHING` (5 s WARNING + ~3 s motion, rounded up to the next 1s tick).
- **Environment**: (A)
- **Setup**: Do NOT press `x` (do not arm the fault). Press `0` to start TRAIN_APPROACHING.
- **Steps**: Press Start at `"flashers ON"`, Stop at `"train signal PROCEED for direction 0 (gates confirmed closed)"`.
- **Expected result**: **7.5–9.5 s** (5 s + 3 s motion, rounded to `RLx`'s 1000 ms tick, plus manual error). This value must be far less than the 20 s fault threshold from RC-TIME-02, proving the design's safety margin.

### RC-TIME-04: Positive — occupancy window is exactly 20 s before reopening begins
- **Type**: Positive
- **Related**: RC-04, `RLX_OCCUPANCY_WINDOW_MS = 20000` (`rlx_timer.h:21`), counted down at `rlx_fsm.c:372-390` via `rlx_timer_tick_window()`
- **Environment**: (A)
- **Setup**: From a confirmed CLOSED state (continuing from RC-TIME-03), wait long enough for `RLX_EXPECTED_ARRIVAL_MS` (20 s, internal placeholder) for the system to automatically transition to `TRAIN_PRESENT` (`rlx_fsm.c:362-370`) — this is the "train has arrived" simulation step, out of scope for the RC-04 test itself but needed to start the real 20 s countdown.
- **Steps**: Press Start as soon as (there is no dedicated log for entering TRAIN_PRESENT in the current build — use an estimated time = time of `PROCEED` + 20 s as the reference point, or add a temporary log if the Verifier allows it to support this test). Stop at `"all train signals -> STOP (crossing reopening)"` (start of OPENING).
- **Expected result**: The interval from entering `TRAIN_PRESENT` to `"all train signals -> STOP"` falls within **19.5–20.5 s**.
- **Note**: since there is no log marking entry into `TRAIN_PRESENT`, this test is hard to measure accurately using the console alone — it is recommended that the Verifier Agent add a temporary `printf`/log line for this test run, or accept measuring indirectly via the total time from `TRAIN_APPROACHING` → `OPENING` (= 5 s + ~3 s + 20 s (RLX_EXPECTED_ARRIVAL_MS) + 20 s (RLX_OCCUPANCY_WINDOW_MS) ≈ 48 s) and cross-checking against the formula instead of measuring the 20 s segment directly.

### RC-TIME-05: Edge case — two trains' occupancy windows overlap, reopening only occurs once BOTH windows have expired
- **Type**: Edge case
- **Related**: RC-04 ("Gates remain closed until every active occupancy window... has elapsed"), `rlx_fsm.c:384-389` (`if (active_window_count == 0) enter_opening()`)
- **Environment**: (A)
- **Setup**: After the direction-0 train has entered `TRAIN_PRESENT` (20 s countdown window started), press key `1` (TRAIN_APPROACHING direction 1) at around second 10 to register a second, offset window.
- **Steps**: Measure the time of reopening (`"all train signals -> STOP"` signals the start of OPENING).
- **Expected result**: The reopening time must be later than the single-train scenario (RC-TIME-04) by an amount corresponding to the offset of the second window's registration (~10 s later), **not** reopening at the 20 s mark of the first window — confirming correct "wait for both windows" semantics, tolerance ±1 s (1000 ms tick).

### RC-TIME-06: Positive — OPEN confirmed quickly (~3 s after entering OPENING), well within the 15 s deadline
- **Type**: Positive
- **Related**: RC-06 internal, `RLX_OPENING_DEADLINE_MS = 15000` (`rlx_timer.h:22`), gate motion 3000 ms
- **Environment**: (A)
- **Setup**: Continuing from RC-TIME-04/05, do not arm the demo fault.
- **Steps**: Press Start at `"all train signals -> STOP (crossing reopening)"`, Stop at `"flashers OFF (gates confirmed open)"`.
- **Expected result**: **2.5–4.5 s** (3 s motion ± 1000 ms tick + manual error), safely under the 15 s fault threshold.

---

## 6. PA-07 — Heartbeat & Watchdog

### PA-TIME-01: Positive — steady 1 Hz heartbeat (real observation limits of the method)
- **Type**: Positive
- **Related**: PA-07, `ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, ...)` on the `Lx` side (`lx_main.c:179`) and `RLx` (similarly) sends `MSG_HEARTBEAT` at 1 Hz.
- **Environment**: (B) or (C) — `C1` + 1 `Lx`.
- **Setup**: System running stably, no errors.
- **Real observation limitation (confirmed by reading the code)**: `c_main.c`'s `on_request()` handling of `MSG_HEARTBEAT`/`MSG_STATUS` does NOT call `c_logger_log()` for the normal case (it only logs on a transition — reconnect/UNAVAILABLE/fault report/mode change) — meaning **there is no dedicated log line for each individual heartbeat** in `central_log.txt`. `c_hmi.c`'s `c_hmi_render()` also has **no `last_seen`/timestamp column** (only `ID/ROLE/MODE/PHASE/CROSSING_STATE/SUPERVISORY/FAULTS/SENSOR/OVERRIDE/AVAILABILITY`). Therefore **there is no way to accurately count 9-11 times/10s** with the tools currently available — any prior claim of "measured X times/10s" was inference, not real observation.
- **Steps (the only practical method)**: Watch that controller's `AVAILABILITY` column on the HMI board continuously for ≥10 seconds.
- **Expected result**: The `AVAILABILITY` column stays `AVAILABLE` throughout (does not drop to `UNAVAILABLE`, which only happens after 3 consecutive ticks with no heartbeat received, ≈3s) — this is indirect evidence that heartbeats are still arriving regularly with a gap of <3s between consecutive ones, **NOT direct proof of the exact 1 Hz frequency** (not measurable — record **not measured** for the exact frequency figure; it can only be asserted via code review: `ipc_timer_arm(...,1000,1000,...)` guarantees a 1000ms cycle at the pulse-scheduling layer).

### PA-TIME-02: Edge case — the correct moment of being marked UNAVAILABLE (2.0–3.0 s, not exactly 3.000 s)
- **Type**: Edge case
- **Related**: PA-07, `c_watchdog_mon_tick()` (`c_watchdog_mon.c:4-16`): increments `missed_heartbeat_ticks` on each 1 Hz tick of `C1` (not phase-synced with `Lx`'s heartbeat), marks unavailable when the count reaches exactly **3**. Since the watchdog's tick and the last heartbeat are not in phase, the actual time to reach the threshold varies within **[2.0 s, 3.0 s)** from the last heartbeat received — not exactly 3.000 s.
- **Environment**: (B) or (C)
- **Setup**: `C1` and 1 `Lx` (e.g. `L1`) running normally, exchanging heartbeats regularly.
- **Steps**: Abruptly stop `L1`'s `lx_main` process (kill -9, simulating a complete connection loss — not a graceful shutdown, to avoid any "intentional" final packet). Record the time the process was stopped (T0, using the system clock where the kill command was run or a manual stopwatch). Watch `central_log.txt` for the line `"Controller 1 marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)"` (the number `1` because `controller_id_t` declares `CTRL_C1 = 0, CTRL_L1 = 1, ...` at `sys_types.h:18-19`, and the log prints the raw enum value, not the `controllers[]` array index) and read its second-resolution timestamp.
- **Expected result**: The interval `[UNAVAILABLE line timestamp] - T0` falls within **1.5–4.0 s** (extra tolerance since `c_logger` only has second resolution → ±1 s rounding on top of the theoretical 2.0–3.0 s interval, plus manual T0 measurement error).

### PA-TIME-03: Negative — must NOT be marked UNAVAILABLE after only 2 missed ticks
- **Type**: Negative
- **Related**: PA-07, the exact condition `missed_heartbeat_ticks == 3` (`c_watchdog_mon.c:11`) — meaning at the 2nd tick (~1.0–2.0 s after the last heartbeat) it must absolutely not yet be marked.
- **Environment**: (B) or (C)
- **Setup**: Same as PA-TIME-02, but this time **restart `L1` early** (resend a heartbeat) at around 1.5 s after stopping it — i.e. before the 3rd tick can occur.
- **Steps**: Kill `lx_main` at T0, restart it (or simulate sending 1 manual heartbeat) at T0+1.5s. Check `central_log.txt` over the entire T0 → T0+3s window.
- **Expected result**: NO `"marked UNAVAILABLE"` line appears in the log during this window (since `missed_heartbeat_ticks` is reset to 0 as soon as `c_server_record_status()`/a new heartbeat arrives, `c_server.c:11-12`, before reaching 3).

---

## 7. PA-11/12 — Override Cap & ACK Round-Trip

### PA-TIME-04: Negative + Edge case — the 300000 ms cap is the correct ACK/NACK boundary
- **Type**: Negative (value exceeding the cap) + Edge case (N/N-1 boundary)
- **Related**: PA-11, `LX_OVERRIDE_DURATION_CAP_MS = 300000` (`lx_timer.h:36`), checked via `duration_ms == 0 || duration_ms > LX_OVERRIDE_DURATION_CAP_MS` at `lx_fsm.c:701` (plus a preliminary validation layer on `C1`'s side, `c_mode_eng.c:113`, using the same 300000 threshold).
- **Environment**: (B) or (C)
- **Setup**: `L1` in `NORMAL_OPERATION`, no railway pre-emption/fault/ped-clearance running (to avoid falling into a different `NACK` branch or `ACK_PENDING`, which would contaminate the result).
- **Steps**:
  1. Send `REQUEST_OVERRIDE` to `L1` with `duration_ms = 300000` (exactly the cap).
  2. Send `REQUEST_OVERRIDE` (after canceling the previous override via `CANCEL_OVERRIDE`) with `duration_ms = 300001`.
- **Expected result**: Step 1 → `"C1: REQUEST_OVERRIDE to 1 -> ACK"`. Step 2 → `"C1: REQUEST_OVERRIDE to 1 -> NACK reason=INVALID_DURATION"`. Exactly at the cap is valid (only `> cap` is rejected, not `>=`).

### PA-TIME-05: Edge case — duration_ms = 0 is also NACKed (not just an upper bound)
- **Type**: Edge case
- **Related**: PA-11, the same `duration_ms == 0` condition at `lx_fsm.c:701`
- **Environment**: (B) or (C)
- **Setup**: Same as above, no active override.
- **Steps**: Send `REQUEST_OVERRIDE` with `duration_ms = 0`.
- **Expected result**: `"C1: REQUEST_OVERRIDE to 1 -> NACK reason=INVALID_DURATION"` — confirming PA-11's cap is the range **(0, 300000]**, rejecting 0 even though theoretically "0 ≤ cap".

### PA-TIME-06: Positive — ACK returns within 1 round-trip, in practice well under 1 s
- **Type**: Positive
- **Related**: PA-12, the synchronous `MsgSend`/`MsgReply` mechanism — latency is only bounded by OS scheduling + Qnet network latency, with no artificial delay logic in `lx_fsm_on_request_override()` for the immediate-ACK branch (`lx_fsm.c:729-736`).
- **Environment**: (C) — important to measure round-trip over a real network rather than the internal loopback (B), which is nearly 0 ms and does not represent Qnet's real inter-VM latency.
- **Setup**: `L1` ready to receive commands, no condition causing `ACK_PENDING`.
- **Steps**: On `C1`'s console, type command `o` (REQUEST_OVERRIDE) and press Enter — start the stopwatch the moment Enter is pressed. Stop the moment the line `"C1: REQUEST_OVERRIDE to 1 -> ACK"` appears on the console.
- **Expected result**: The measured latency is **< 1.0 s** — in practice, on an internal LAN, expected to be only **a few tens to a few hundred milliseconds**; since `c_logger` only prints seconds, visually observing both events will usually fall within **the same displayed second** on the console, sufficient to conclude PA-12 is met (sub-second precision is not required for this test — only confirming there is no "visible" multi-second delay).
- **Measurement-error note**: this is a real manual stopwatch (not a diff between 2 log lines — `c_operator.c` does not print a separate "sending" line to compare against, only a single result line `-> ACK`), so the tester's own Start/Stop reaction time (~150-300ms) is itself part of this measurement's error — use this method only to confirm "under 1s" (PA-12's threshold is much larger than the reaction-time error), not to assert an exact value down to tens of milliseconds.

### PA-TIME-07: Edge case — ACK_PENDING still responds instantly even though activation is deferred
- **Type**: Edge case
- **Related**: PA-12 ("only activation is deferred"), the `ped_clearance_active` branch at `lx_fsm.c:713-728` returns `RESULT_ACK_PENDING` immediately, while actual activation waits until `lx_fsm_on_phase_timer()` detects `ped_clearance_active` has turned off.
- **Environment**: (B) or (C)
- **Setup**: Trigger a pedestrian request (WALK+FDW running, total ~10 s per TL-TIME-07) on `L1`, then **immediately while WALK/FDW is still running**, send `REQUEST_OVERRIDE`.
- **Steps**: Press Start when sending the command, Stop when `"C1: REQUEST_OVERRIDE to 1 -> ACK_PENDING"` appears — this is the immediate response to measure (< 1 s). Then, continue observing and separately measure the time from sending the command to when the override **actually takes effect** (e.g. observing the signal output change per `override_target_movement`, or a corresponding log line if available) — this interval is ALLOWED to last as long as the remaining ped-clearance sequence (up to ~10 s per TL-05/06), which does not violate PA-12.
- **Expected result**: The `ACK_PENDING` response appears **within 1 s** (in practice nearly instant, similar to PA-TIME-06); the actual activation may be delayed by several seconds afterward (as designed by intent) and this **is not** counted as a violation of PA-12's 1 s response threshold, since these are two distinct milestones (ACK vs. activation).

---

## Appendix: Test Case Summary

| ID | Group | Type | Environment |
| --- | --- | --- | --- |
| TC-TIME-01 | TC-02 | Positive | C |
| TC-TIME-02 | TC-02 | Positive | C |
| TC-TIME-03 | TC-02 | Positive | C |
| TC-TIME-04 | TC-03 | Edge case | B/C |
| TC-TIME-05 | PA-09 | Negative + Edge | D |
| TL-TIME-01 | TL-01 | Positive | A |
| TL-TIME-02 | TL-01 | Edge case | A |
| TL-TIME-03 | TL-01 | Positive | A |
| TL-TIME-04 | TL-01 | Edge case | A |
| TL-TIME-05 | TL-01 | Positive | A |
| TL-TIME-06 | TL-01 | Positive | A |
| TL-TIME-07 | TL-05/06 | Positive | A |
| TL-TIME-08 | TL-02 | Negative (integrity) | A |
| DP-TIME-01 | DP-02 | Positive | A |
| DP-TIME-02 | DP-02 | Edge + Positive | B/C |
| DP-TIME-03 | DP-01 | Positive | B/C |
| CC-TIME-01 | CC-03 | Positive | B/C |
| CC-TIME-02 | CC-03 | Edge case | B/C |
| CC-TIME-03 | CC-03 | Positive | B/C |
| RC-TIME-01 | RC-03 | Positive | A |
| RC-TIME-02 | RC-03 | Edge + Negative | A |
| RC-TIME-03 | RC-03/06 | Positive | A |
| RC-TIME-04 | RC-04 | Positive | A |
| RC-TIME-05 | RC-04 | Edge case | A |
| RC-TIME-06 | RC-06 | Positive | A |
| PA-TIME-01 | PA-07 | Positive | B/C |
| PA-TIME-02 | PA-07 | Edge case | B/C |
| PA-TIME-03 | PA-07 | Negative | B/C |
| PA-TIME-04 | PA-11 | Negative + Edge | B/C |
| PA-TIME-05 | PA-11 | Edge case | B/C |
| PA-TIME-06 | PA-12 | Positive | C |
| PA-TIME-07 | PA-12 | Edge case | B/C |

**Total: 32 test cases**, covering all required timing-assumption groups (TC-01..05, TL-01..06, DP-01/02, CC-01/03, RC-03/04/06, PA-07, PA-11/12), each numeric value having at least one positive test and one edge/boundary test at the tick boundary, plus negative tests for the NACK thresholds under PA-09/PA-11.
