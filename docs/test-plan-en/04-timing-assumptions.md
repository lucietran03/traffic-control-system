# 04. Test Plan — Verifying Timing Assumptions

This document tests whether the **numeric/timing values** published in
`system_assumptions_tables.md` (TC-01..05, TL-01..06, DP-01/02, CC-01/03,
RC-03/04/06, PA-07, PA-11/12) actually match the runtime behavior of the
system when running on real QNX Neutrino. Pure physical/planning
assumptions that cannot be observed by running the program (NU-01, RC-08,
TC-01's 350/400/320/380 m distances, RC-08's 80 km/h train speed, etc.)
are **not** in scope for this document.

Every number below is cross-checked directly against the source code as
of the time of writing (not guessed) — see the "Code source" column in
table 0.3.

---

## 0. General Conventions

### 0.1 Three test environments

| Symbol | Description | When to use |
| --- | --- | --- |
| **(A)** | Single node: only one process (`lx_main`, `rlx_main`, or `c_main`) running on one QNX target/VM. No `TRAFFIC_NODE_MAP` needed. | Testing an FSM's own internal timing constants (TL-xx, RC-xx internal countdown, local PA-11/12). |
| **(B)** | Multiple nodes on the same QNX machine/VM (multiple `lx_main`/`rlx_main`/`c_main` processes on the same target, communicating over internal Qnet loopback, no `TRAFFIC_NODE_MAP` needed since the default is "same node"). | Testing multi-controller interaction (PA-07 watchdog, RC-xx involving both `Lx` and `RLx`, PA-11/12 round-trip C1↔Lx) when 2-3 physical machines aren't available. |
| **(C)** | Multiple real QNX VMs/machines over real Qnet networking (requires declaring `TRAFFIC_NODE_MAP`, see `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` section 2.2–2.3 and Case 1/2/3). | **Required for TC-01..05** because the green-wave offset only has real test meaning when `L1/L3/L5` (or `L2/L4/L6`) are independent processes, started at staggered times, synchronized **solely** via wall clock — exactly as `lx_fsm_apply_offset_locked()` assumes. Can be downgraded to (B) if running on the same machine is acceptable (see the "limitation" note in each TC-TIME test case). |

Example declaration for (C), from `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`:
```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"
```

### 0.2 Measurement tolerance — why sub-millisecond precision is not required

- **Fixed 100 ms signal-phase/railway tick** (`LX_PHASE_TICK_MS`, `lx_timer.h:40`) for `Lx`, and a 1000 ms tick for `RLx`/heartbeat/watchdog. Any timing threshold is only re-evaluated at multiples of that tick — requiring precision below 100 ms (Lx) or below 1 s (RLx/PA-07) is meaningless.
- **No shared monotonic clock across nodes.** `lx_fsm_apply_offset_locked()` (`app/intersection/src/lx_fsm.c:564-614`) synchronizes the green-wave offset using `clock_gettime(CLOCK_REALTIME, ...)` — i.e., each machine/VM's **wall clock**. If the VMs' system clocks drift apart (no NTP/time sync), that drift adds directly to the measured offset error. **Before running any TC-TIME test, synchronize the wall clocks of all participating VMs** (e.g., manual `date` or NTP if outbound network is available) and record the residual drift (should be < 200 ms).
- **`C1`'s log (`c_logger.c`) has only 1-second resolution** (`strftime("%Y-%m-%d %H:%M:%S", ...)`, `app/central/src/c_logger.c:29-31`) — no milliseconds. Using this log to measure the gap between two events a few seconds apart is acceptable (±1 s rounding error), but **must not** be used to measure gaps < 2 s.
- **`Lx`/`RLx` logs (`lx_signal.c`, `rlx_signal.c`, `rlx_gate.c`) have NO timestamp at all** — plain `printf` output (e.g., `"Lx %d: SIGNAL -> %s\n"`, `app/intersection/src/lx_signal.c:42`). So any test case that needs to measure time between two `Lx`/`RLx` log lines **must** use one of:
  1. **Manual stopwatch** (phone): start timing the instant the log line appears on the SSH console, stop at the next log line. Estimated human reaction error **±300–500 ms** — adds to the system tolerance.
  2. Pipe stdout through a timestamping wrapper before writing to file/console, e.g. (if the target shell supports it): `/tmp/lx_main 1 | while IFS= read -r line; do echo "$(date +%T.%3N) $line"; done > /tmp/l1.log`. More accurate than a stopwatch, but depends on `date` supporting milliseconds (`%3N`) on the QNX shell in use — check first; fall back to method 1 if unsupported.
- **Default recommended tolerance for any stopwatch-measured test:** expected value ± (500 ms + 1 relevant tick). Specific test cases below state the exact figure that applies.

### 0.3 Reference timing constants table — cross-checked directly against code (source of truth)

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
| `LX_PHASE_TICK_MS` | 100 | `lx_timer.h:40` | (system tick) |
| `LX_WALK_MS` | 6000 | `lx_timer.h:57` | TL-05 (placeholder, not a spec value) |
| `LX_FLASHING_DONT_WALK_MS` | 4000 | `lx_timer.h:58` | TL-05 (placeholder) |
| `LX_DRAIN_MAX_EXTENSION_MS` | 60000 | `lx_timer.h:71` | CC-03 |
| `LX_CYCLE_LENGTH_MS` | 90000 (= 48+4+2+30+4+2 s, computed from the constants above) | `lx_timer.h:81-82` | TL-02/TC-02 |
| `R1_L1_OFFSET_MS` / `R1_L3_OFFSET_MS` / `R1_L5_OFFSET_MS` | 0 / 21000 / 45000 | `c_mode_eng.h:53-55` | TC-02 |
| `R2_L2_OFFSET_MS` / `R2_L4_OFFSET_MS` / `R2_L6_OFFSET_MS` | 0 / 19000 / 42000 | `c_mode_eng.h:56-58` | TC-02 |
| `C_MODE_ENG_DEFAULT_PEAK_START_HOUR` / `_END_HOUR` | 6 / 9 | `c_mode_eng.h:49-50` | DP-02 (custom placeholder) |
| NACK offset threshold | `offset_ms >= LX_CYCLE_LENGTH_MS` (90000) → NACK | `lx_fsm.c:625-636` | PA-09 |
| `RLX_WARNING_TO_CLOSING_MS` | 5000 | `rlx_timer.h:15` | RC-03 |
| `RLX_CLOSING_DEADLINE_MS` | 15000 (measured **from entry into the CLOSING state**, i.e. **5000+15000=20000 ms from TRAIN_APPROACHING** before the fault is raised — matches the "20 s" mark of Appendix B4, not 15 s) | `rlx_timer.h:16`; applied at `rlx_fsm.c:139` (`enter_closing()` resets `state_elapsed_ms=0` at `rlx_fsm.c:162-167`) | RC-03 |
| `RLX_OCCUPANCY_WINDOW_MS` | 20000 | `rlx_timer.h:21` | RC-04 |
| `RLX_OPENING_DEADLINE_MS` | 15000 (from entry into OPENING state) | `rlx_timer.h:22` | RC-06 (internal value, no direct counterpart in the main table) |
| `RLX_GATE_MOTION_MS` | 3000 (simulated gate-motion duration — used to show the normal path confirms CLOSED/OPEN much faster than the deadline) | `rlx_gate.h:10` | (supports RC-03/RC-06) |
| Heartbeat period | 1000 ms both directions (`Lx`/`RLx` send, `C1` ticks/counts) | `lx_main.c:179`, `c_main.c:174` | PA-07 |
| UNAVAILABLE threshold | `missed_heartbeat_ticks == 3` (1 Hz tick) → **2.0–3.0 s** after the last heartbeat, not exactly 3.000 s (see PA-TIME-02) | `c_watchdog_mon.c:4-16` | PA-07 |
| Override duration cap | `duration_ms == 0 \|\| duration_ms > 300000` → NACK (both at `Lx` and at `C1`'s validation layer) | `lx_fsm.c:701`, `c_mode_eng.c:113` | PA-11 |

**Important note discovered while reading the code (affects how DP-01/02 tests are written):**
`c_mode_eng_select_mode()` (`app/central/src/c_mode_eng.c:56-62`) takes
`current_hour` as a **caller-supplied** parameter — the function itself
never reads the system clock. A full review of
`app/central/src/c_main.c` and `app/central/src/c_operator.c` confirms
**no code path in the actual running system calls this function with the
real wall-clock hour**; neither `Lx` nor `RLx` has any equivalent logic
reading `tm_hour`/`localtime`. In other words: **automatic switching
between `PEAK_FIXED` ↔ `OFF_PEAK_SENSOR` based on wall-clock time (DP-02)
is currently NOT wired into the running system** — this is an
implementation gap, not an incorrect assumption. The DP-TIME-xx test
cases below reflect this actual state rather than pretending the feature
exists.

---

## 1. TC-01..05 — Green-wave offset

> Required environment: **(C)** to allow real measurement between
> independent nodes; may be downgraded to (B) if identical system time
> zones (same machine) is acceptable — in that case, record in the test
> report that it was "run on (B), real inter-VM clock sync unverified".

### TC-TIME-01: Offset R1 — L1 → L3 = 21 s
- **Type**: Positive
- **Related**: TC-02, `R1_L3_OFFSET_MS = 21000` (`c_mode_eng.h:54`)
- **Environment**: (C) — `L1`, `L3`, `L5` running on 3 different VMs (or at minimum `L1`/`L3` on 2 different VMs), `C1` on its own VM, wall clocks synchronized across VMs (measured drift < 200 ms via `date` on each VM beforehand).
- **Setup**: Start `c_main`, `rlx_main 1..3`, `lx_main 1..6` in the order given in `QNX_DEPLOYMENT_RUN_GUIDE.md` section 2.3. Let the system run stably in `MODE_PEAK_FIXED` (the default mode at startup, `lx_fsm.c:360`) for at least 1 full 90 s cycle before measuring, so that `assigned_offset_ms` (sent from `C1` via `SET_TIMING_PROFILE`, UC-03) has been applied to a new ARTERIAL_GREEN phase (see TC-TIME-04 on apply-delay behavior).
- **Steps**:
  1. On `L1`'s console, wait for the log line `"Lx 1: SIGNAL -> ARTERIAL GREEN"`.
  2. As soon as it appears, start the stopwatch.
  3. Watch `L3`'s console in parallel, wait for `"Lx 3: SIGNAL -> ARTERIAL GREEN"` in the **immediately following** cycle (not a random later one).
  4. Stop the stopwatch as soon as that line appears.
- **Expected Result**: Measured time falls within **20.5–21.5 s** (21 s ± 500 ms for the 100 ms tick + human reaction delay + inter-VM clock sync error).

### TC-TIME-02: Offset R1 — L3 → L5 = 24 s (cumulative L1 → L5 = 45 s)
- **Type**: Positive
- **Related**: TC-02, `R1_L5_OFFSET_MS = 45000` (`c_mode_eng.h:55`); `L3→L5` segment = 45000-21000 = 24000 ms.
- **Environment**: (C), continuing from the stable state established in TC-TIME-01.
- **Setup**: Same as TC-TIME-01.
- **Steps**: Repeat the TC-TIME-01 procedure but start at `"Lx 3: SIGNAL -> ARTERIAL GREEN"` and stop at `"Lx 5: SIGNAL -> ARTERIAL GREEN"` in the following cycle. Also directly measure L1 → L5 (bridging 2 cycles if needed) to cross-check the cumulative total.
- **Expected Result**: L3 → L5 within **23.5–24.5 s**; L1 → L5 (measured directly or derived) within **44.5–45.5 s**.

### TC-TIME-03: Offset R2 — L2 → L4 = 19 s, L4 → L6 = 23 s (cumulative 42 s)
- **Type**: Positive
- **Related**: TC-02, `R2_L4_OFFSET_MS = 19000`, `R2_L6_OFFSET_MS = 42000` (`c_mode_eng.h:57-58`)
- **Environment**: (C)
- **Setup**: Same as TC-TIME-01, applied to the R2 chain (`L2`→`L4`→`L6`).
- **Steps**: Measure `"Lx 2: SIGNAL -> ARTERIAL GREEN"` → `"Lx 4: SIGNAL -> ARTERIAL GREEN"` (expect ~19 s) and `"Lx 4: ..."` → `"Lx 6: ..."` (expect ~23 s), using the same stopwatch method as above.
- **Expected Result**: L2→L4 within **18.5–19.5 s**; L4→L6 within **22.5–23.5 s**.

### TC-TIME-04: Edge case — offset only applies on the next new ARTERIAL_GREEN entry, does not cut short the running phase
- **Type**: Edge case
- **Related**: TC-03, algorithm `lx_fsm_apply_offset_locked()` (`lx_fsm.c:481-563`, especially the "Compliance-audit fix" section — the offset only sets the `offset_apply_pending` flag at `lx_fsm_on_set_timing_profile()`, `lx_fsm.c:645`, and is only actually applied at `lx_fsm.c:344` when a **new** `PHASE_ARTERIAL_GREEN` begins).
- **Environment**: (B) or (C) — only needs 1 `L1` + `C1`.
- **Setup**: Run `L1` in `PEAK_FIXED`. Use the operator command tool (`C1` key `t`) to send `SET_TIMING_PROFILE` with a new `offset_ms` **right in the middle of `L1` displaying ARTERIAL GREEN** (watch `L1`'s console, send the command when you know for certain more than 20 s of green remains).
- **Steps**:
  1. Record the time the command was sent (via stopwatch) and the time `"Lx 1: SIGNAL -> ARTERIAL YELLOW"` next appears (end of the current ARTERIAL_GREEN phase).
  2. Measure the interval from when the command was sent to when that ARTERIAL_GREEN phase **actually ends** (transitions to YELLOW).
  3. Measure the duration of that ARTERIAL_GREEN phase **from its own start** (not from when the command was sent) to the transition to YELLOW.
- **Expected Result**: The ARTERIAL_GREEN phase **running at the time the command was sent** must total exactly **48 s ± 200 ms** (not shortened/lengthened by the new offset) — i.e. `SET_TIMING_PROFILE` does not cut into the current phase. The new offset is only observed starting from the **next** ARTERIAL_GREEN onward (cross-check by repeating TC-TIME-01/02/03 after sending the command).

### TC-TIME-05: Negative — offset_ms = LX_CYCLE_LENGTH_MS (90000) is NACKed; 89999 is still ACKed
- **Type**: Negative + Edge case (boundary)
- **Related**: PA-09, checks `payload->offset_ms >= LX_CYCLE_LENGTH_MS` at `lx_fsm.c:625-636` (returns `RESULT_NACK` / `NACK_REASON_STALE_OR_UNSAFE_PROFILE`)
- **Environment**: (A) or (B) — only needs 1 `L1` receiving commands directly (via the `C1` operator console or a manual test client sending `MSG_SET_TIMING_PROFILE`).
- **Setup**: `L1` running normally, no fault.
- **Steps**:
  1. Send `SET_TIMING_PROFILE` with `offset_ms = 89999` → check `C1` log: `"C1: SET_TIMING_PROFILE to 1 -> ACK"`.
  2. Send `SET_TIMING_PROFILE` with `offset_ms = 90000` (exactly `LX_CYCLE_LENGTH_MS`) → check `C1` log.
  3. (Optional) Send `offset_ms = 90001` to confirm the same NACK behavior.
- **Expected Result**: Step 1 → `ACK`. Steps 2 and 3 → `"C1: SET_TIMING_PROFILE to 1 -> NACK reason=STALE_OR_UNSAFE_PROFILE"` (name printed by `nack_reason_name()`, `c_comm.c:57`). This is the correct N-1/N boundary: 89999 valid, 90000 (N) rejected.

---

## 2. TL-01..06 — Traffic signal phase timing

### TL-TIME-01: Positive — 8 s min green respected when there is no demand
- **Type**: Positive
- **Related**: TL-01, `LX_MIN_GREEN_MS = 8000` (`lx_timer.h:31`), enforced at `lx_timer_should_exit_green()` (`lx_timer.c:19-30`: `if (elapsed_ms < LX_MIN_GREEN_MS) return 0`)
- **Environment**: (A) — 1 standalone `L1`.
- **Setup**: Switch `L1` to `MODE_OFF_PEAK_SENSOR` (`SET_MODE` command from `C1`, or wait if the default mode differs — confirm the current mode via log/HMI before testing). Ensure no demand (arterial/connector) exists before ARTERIAL_GREEN begins — use the sensor keys to clear demand (uppercase `C`/`A` = clear).
- **Steps**: Start the stopwatch as soon as `"Lx 1: SIGNAL -> ARTERIAL GREEN"` appears, generate no demand at all during the session. Stop at `"Lx 1: SIGNAL -> ARTERIAL YELLOW"`.
- **Expected Result**: Measured time **≥ 8.0 s** (must not end earlier), and within **8.0–8.5 s** since the extension check only runs at multiples of 4000 ms, and 8000 ms is the first exit-check point when there's no demand (`lx_fsm.c:981` `if ((green_elapsed_ms % LX_EXTENSION_MS) == 0)`).

### TL-TIME-02: Edge case — exact boundary 7.9 s vs 8.0 s
- **Type**: Edge case
- **Related**: TL-01, same mechanism as TL-TIME-01, boundary `elapsed_ms < 8000` (holds) vs `elapsed_ms == 8000` (allowed to exit).
- **Environment**: (A)
- **Setup**: Same as TL-TIME-01, no demand.
- **Steps**: Precisely measure when the ARTERIAL_GREEN phase starts and monitor continuously until it switches to YELLOW. Since the system tick is 100 ms, the actual "N-1/N" points to verify are **7.9 s (must not yet exit) and 8.0 s (first valid exit-check point)**, not fractional milliseconds (the system has no resolution below 100 ms).
- **Expected Result**: `ARTERIAL YELLOW` must never be observed before the 7.9 s mark (measurement tolerance −0/+300 ms for manual stopwatch); the phase must end within the 8.0–8.5 s window as in TL-TIME-01 if there is no demand.

### TL-TIME-03: Positive — 40 s max green forces phase exit despite remaining demand
- **Type**: Positive
- **Related**: TL-01, `LX_MAX_GREEN_MS = 40000` (`lx_timer.h:32`), `maxed = elapsed_ms >= LX_MAX_GREEN_MS` (`lx_timer.c:27-29`)
- **Environment**: (A)
- **Setup**: `MODE_OFF_PEAK_SENSOR`. Generate continuous arterial demand (hold key `a` as "present" and never press `A` to clear) throughout the phase.
- **Steps**: Start the stopwatch at `ARTERIAL GREEN`, keep demand continuous, stop at `ARTERIAL YELLOW`.
- **Expected Result**: Phase ends within **40.0–40.5 s** despite demand still being present (proving the 40 s cap overrides continuous demand), must not exceed 40.5 s.

### TL-TIME-04: Edge case — boundary 39.9 s (not yet maxed, demand holds the phase) vs 40.0 s (maxed, forced exit)
- **Type**: Edge case
- **Related**: TL-01, same mechanism as TL-TIME-03.
- **Environment**: (A)
- **Setup**: Same as TL-TIME-03.
- **Steps**: Monitor continuously around the 39.9–40.1 s mark (stopwatch measurement, ±300 ms tolerance accepted since this is a visual console observation).
- **Expected Result**: At ~39.9 s the phase is still ARTERIAL GREEN (since `elapsed_ms < 40000` and demand remains, `should_exit` returns 0 while not maxed); the phase must switch to YELLOW within the 40.0–40.5 s window regardless of demand.

### TL-TIME-05: Positive — yellow exactly 4 s
- **Type**: Positive
- **Related**: TL-01, `LX_YELLOW_MS = 4000` (`lx_timer.h:27`), checked via `green_elapsed_ms >= LX_YELLOW_MS` at `lx_fsm.c:944`
- **Environment**: (A), applies to both `PEAK_FIXED` and `OFF_PEAK_SENSOR` (shared constant).
- **Setup**: Any mode, wait for a YELLOW phase to appear.
- **Steps**: Start at `"SIGNAL -> ARTERIAL YELLOW"` (or CONNECTOR YELLOW), stop at the following `"SIGNAL -> ALL RED (...)"`.
- **Expected Result**: **3.9–4.1 s** (4 s ± 100 ms tick + ±300 ms manual-timing error → rounded acceptance tolerance **3.6–4.4 s**).

### TL-TIME-06: Positive — all-red clearance exactly 2 s
- **Type**: Positive
- **Related**: TL-01, `LX_ALL_RED_MS = 2000` (`lx_timer.h:28`), checked at `lx_fsm.c:951`
- **Environment**: (A)
- **Setup**: Same as above.
- **Steps**: Start at `"SIGNAL -> ALL RED (A to B)"`, stop at `"SIGNAL -> CONNECTOR GREEN"` (or the corresponding next phase).
- **Expected Result**: **1.6–2.4 s** (2 s ± 100 ms tick + ±300 ms manual error).

### TL-TIME-07: Positive — pedestrian WALK (6 s) → FLASHING_DONT_WALK (4 s) sequence, total 10 s
- **Type**: Positive
- **Related**: TL-05/TL-06, `LX_WALK_MS = 6000`, `LX_FLASHING_DONT_WALK_MS = 4000` (`lx_timer.h:57-58` — **note these are the team's own placeholder values, not numbers mandated by the spec**, but must still match exactly what was coded).
- **Environment**: (A)
- **Setup**: During a compatible phase (e.g. ARTERIAL_GREEN), press the pedestrian button (key `1`) to generate a `PED_REQUEST` on the compatible side.
- **Steps**: Start at `"PED SIGNAL side 0 -> WALK"`, lap at `"PED SIGNAL side 0 -> FLASHING_DONT_WALK"`, stop at `"PED SIGNAL side 0 -> DONT_WALK"`.
- **Expected Result**: WALK lasts **5.6–6.4 s**, FLASHING_DONT_WALK lasts **3.6–4.4 s**, total sequence **9.6–10.4 s**.

### TL-TIME-08: Negative/integrity — total PEAK_FIXED cycle must equal exactly 90 s
- **Type**: Negative (checks for no cumulative drift)
- **Related**: TL-02, `LX_CYCLE_LENGTH_MS = 90000` (`lx_timer.h:81-82`, computed from 48+4+2+30+4+2)
- **Environment**: (A)
- **Setup**: `MODE_PEAK_FIXED`, no override/railway pre-emption/fault occurring during measurement (any such interruption invalidates this measurement).
- **Steps**: Start at any `"SIGNAL -> ARTERIAL GREEN"` occurrence, stop at the **next** `"SIGNAL -> ARTERIAL GREEN"` (exactly 1 full cycle: arterial green+yellow+all-red+connector green+yellow+all-red).
- **Expected Result**: **89.0–91.0 s** (90 s ± ~1 s, a looser tolerance than the above tests since this is a sum of 6 manually-measured intervals compounding error). If the deviation persistently exceeds ±1 s across multiple cycles, suspect drift in `lx_fsm_on_phase_timer()` requiring further investigation (out of scope for this document).

---

## 3. DP-01/02 — PEAK_FIXED / OFF_PEAK_SENSOR modes

> **Warning before testing**: as noted in section 0.3, automatic
> clock-time-based mode switching is **not wired into the runtime**. The
> three test cases below are designed to reflect that actual state, not
> to "prove" a feature that doesn't exist.

### DP-TIME-01: Positive — default placeholder values are exactly 6 and 9
- **Type**: Positive (checks source code/constants, not dynamic runtime behavior)
- **Related**: DP-02, `C_MODE_ENG_DEFAULT_PEAK_START_HOUR = 6`, `C_MODE_ENG_DEFAULT_PEAK_END_HOUR = 9` (`c_mode_eng.h:49-50`), initialized by `c_mode_eng_init()`.
- **Environment**: (A) — run `c_main` standalone.
- **Setup**: No special setup needed; this checks the values loaded into the struct at startup.
- **Steps**: If a debug/unit-test hook is available that calls `c_mode_eng_select_mode(&eng, 6)`, `c_mode_eng_select_mode(&eng, 8)`, and `c_mode_eng_select_mode(&eng, 9)` after `c_mode_eng_init()` — run it and print the results. If no such hook exists, verify by reading `central_log.txt`/the compiled source to confirm the constants weren't changed at build time.
- **Expected Result**: `select_mode(6)` → `MODE_PEAK_FIXED`, `select_mode(8)` → `MODE_PEAK_FIXED`, `select_mode(9)` → `MODE_OFF_PEAK_SENSOR` (boundary exactly at hour 9, see DP-TIME-02b).

### DP-TIME-02: Edge case / Known-gap — crossing the 06:00 or 09:00 mark does NOT auto-switch mode
- **Type**: Edge case (hour boundary) and Negative (confirms implementation gap)
- **Related**: DP-02; confirms no call to `c_mode_eng_select_mode()` in `c_main.c`/`c_operator.c` uses real system time.
- **Environment**: (B) or (C) — `C1` + at least 1 `Lx`.
- **Setup**: Set the system clock of the VM running `C1` to 05:59:00 (using `date` with root privileges on the QNX target — only on a test VM, never a shared machine). Start `c_main`.
- **Steps**: Continuously watch `C1`'s log/HMI as the clock crosses 06:00:00 and 09:00:00 (advance the clock or wait in real time). Record the `operating_mode` reported by each `Lx` (via HMI `c_hmi_render`) at each mark.
- **Expected Result (current state)**: **Mode does NOT auto-switch** as the clock crosses 06:00/09:00 — because no process actively reads the time and calls `c_mode_eng_select_mode()`/sends `SET_MODE`. This is a "PASS" for verifying the actual state of the code, but it is also a **discrepancy that must be recorded for the Compliance Agent**: DP-02 describes "clock-time boundary... selects the applicable normal mode" as runtime behavior, while the current code only provides a pure function that is never called automatically — the team needs to either (a) add a caller that reads `localtime()->tm_hour` every tick and invokes this function, or (b) update the documentation to state this is an API ready for future integration.

### DP-TIME-03: Positive — the only working path: manual SET_MODE via the operator console
- **Type**: Positive
- **Related**: DP-01 (both modes exist and can be switched), via the `MSG_SET_MODE` command (`lx_fsm_on_set_mode`, `lx_fsm.c:689+` — moved further down in the file after the TC-02/TC-03 fixes above), operator console key `m` (`c_operator.c`).
- **Environment**: (B) or (C)
- **Setup**: `C1` and `L1` running, `L1` in `MODE_PEAK_FIXED` (default).
- **Steps**: On `C1`'s console, press `m`, select `L1`, choose mode `1` (OFF_PEAK_SENSOR). Watch `central_log.txt`.
- **Expected Result**: Log line `"C1: SET_MODE to 1 -> ACK"` appears almost immediately (within 1 s per the logger's second-resolution clock — see also PA-TIME-06 on round-trip); `L1`'s phase behavior switches to sensor-driven starting from the next safe phase boundary (TL-04).

---

## 4. CC-01/03 — Queue detection & drain phase

### CC-TIME-01: Positive — drain phase extends in 4 s steps while QUEUE_WARNING remains active
- **Type**: Positive
- **Related**: CC-03, `LX_EXTENSION_MS = 4000` reused for the drain check cadence (`lx_fsm.c:1021` `if ((drain_extension_total_ms % LX_EXTENSION_MS) == 0)`)
- **Environment**: (B) or (C) — needs `RLx` to trigger the crossing-reopen scenario (drain is only armed via `drain_pending` when the crossing reopens with `queue_warning_active`, `lx_fsm.c:809-822`).
- **Setup**: Put the relevant `RLx` into a closed-gate state (simulating an incoming train), and enable `QUEUE_WARNING` on the relevant `Lx`'s approach connector (key `w`). Wait for the gate to reopen (see section 5 for gate timing).
- **Steps**: Once the connector-drain phase begins, keep `QUEUE_WARNING` on continuously, measure the interval between "extensions" — since there's no dedicated log line per 4 s extension, measure the total duration of the connector-drain phase from its start until you actively turn off `QUEUE_WARNING` (key `W`) and observe it ending **exactly at the next 4 s check point**, not immediately.
- **Expected Result**: After turning off `QUEUE_WARNING`, the phase continues for up to nearly 4 more seconds before switching to YELLOW (since the condition is only re-checked at multiples of 4000 ms of `drain_extension_total_ms`) — observed shutoff delay within **0–4.3 s** from the moment the flag was cleared.

### CC-TIME-02: Edge case — hard cap at exactly 60 s even with QUEUE_WARNING still active
- **Type**: Edge case
- **Related**: CC-03, `LX_DRAIN_MAX_EXTENSION_MS = 60000` (`lx_timer.h:71`), checks `drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS` (`lx_fsm.c:1022`)
- **Environment**: (B) or (C)
- **Setup**: Same as CC-TIME-01, but **keep `QUEUE_WARNING` on continuously and never turn it off**.
- **Steps**: Start the stopwatch as soon as the drain phase begins (the `"SIGNAL -> CONNECTOR GREEN"` line right after the gate signals open, with `drain_pending` already armed beforehand). Stop at `"SIGNAL -> CONNECTOR YELLOW"`.
- **Expected Result**: Phase ends within **60.0–60.5 s** from the start of drain (must not exceed — the cap is hard, `>=` not `>`), regardless of `QUEUE_WARNING` still being active.

### CC-TIME-03: Positive — drain ends early as soon as QUEUE_WARNING naturally clears (doesn't wait the full 60 s)
- **Type**: Positive
- **Related**: CC-03 (end condition "queue warning clears OR 60s cap")
- **Environment**: (B) or (C)
- **Setup**: Same as CC-TIME-01, but clear `QUEUE_WARNING` early, e.g. at second 12 (between two 4 s marks: 12000 ms is a multiple of 4000, chosen for easy observation).
- **Steps**: Clear `QUEUE_WARNING` at second 12, measure when the phase switches to YELLOW.
- **Expected Result**: Phase ends at **~12.0–16.0 s** (up to one more 4 s beat after clearing, due to the poll cadence at multiples of 4000 ms), **not** extending close to 60 s.

---

## 5. RC-03/04/06 — Railway crossing warning & gate close/open

Relevant log lines (no timestamp, use stopwatch — see section 0.2):
- `"RLx: flashers ON (train approaching, direction %u)"` — start of WARNING (`rlx_signal.c:6`), corresponds to T0 of `TRAIN_APPROACHING`.
- `"RLx: commanding gates DOWN (simulated motion, 3000 ms)"` — start of CLOSING (`rlx_gate.c:48`).
- `"RLx: train signal PROCEED for direction %u (gates confirmed closed)"` — CLOSED confirmed (`rlx_signal.c:16`).
- `"RLx: all train signals -> STOP (crossing reopening)"` — start of OPENING (`rlx_signal.c:21`).
- `"RLx: flashers OFF (gates confirmed open)"` — OPEN confirmed (`rlx_signal.c:11`).
- `"RLx: FAULT latched (fault bit 0x...) ..."` — fault reported (`rlx_signal.c:31`).

### RC-TIME-01: Positive — warning-to-closing exactly 5 s
- **Type**: Positive
- **Related**: RC-03, `RLX_WARNING_TO_CLOSING_MS = 5000` (`rlx_timer.h:15`), checked at `rlx_fsm.c:351`
- **Environment**: (A) — 1 standalone `RL1` (its own internal 1 Hz tick).
- **Setup**: `RL1` in `RLX_OPEN` state (default at startup).
- **Steps**: Press key `0` (TRAIN_APPROACHING direction 0) on `rlx_sensor`. Start the stopwatch as soon as `"flashers ON"` appears. Stop at `"commanding gates DOWN"`.
- **Expected Result**: **4.5–5.5 s** (5 s ± 500 ms, 1000 ms tick + manual error).

### RC-TIME-02: Edge case (fault path) — total budget is 20 s (not 15 s) before FAULT_GATE_CONFIRM_MISSING
- **Type**: Edge case + Negative (checks error path)
- **Related**: RC-03/RC-06; **important finding from reading the code**: `RLX_CLOSING_DEADLINE_MS = 15000` is measured **from entry into the CLOSING state** (`state_elapsed_ms` reset to 0 at `enter_closing()`, `rlx_fsm.c:162-167`), NOT from `TRAIN_APPROACHING`. So the actual total elapsed time before the fault is raised is **5000 (WARNING) + 15000 (CLOSING) = 20000 ms**, matching the cumulative "20 s — closed-confirmation margin" mark of Appendix B4 in `system_assumptions_tables.md`, not the 15 s the constant name might misleadingly suggest.
- **Environment**: (A)
- **Setup**: Press key `x` on `rlx_sensor` to arm "gate motion will NEVER confirm closed" (fault demo, `rlx_gate.c:120`, "RC-06 fault path"). Then press `0` to start TRAIN_APPROACHING.
- **Steps**: Start the stopwatch at `"flashers ON"`. Watch continuously, note the marks at 19.5 s (expect: no fault yet) and 20.0–20.5 s (expect: fault appears).
- **Expected Result**: NO `"FAULT latched"` line before **19.5 s**; the line `"FAULT latched (fault bit ... ) ... commanding gates DOWN"` MUST appear within **20.0–21.0 s** from `flashers ON` (looser tolerance since it accumulates 2 discrete 1000ms ticks + manual error).

### RC-TIME-03: Positive — normal path confirms CLOSED quickly (~8 s), with a large safety margin vs. the 20 s deadline
- **Type**: Positive
- **Related**: RC-03/RC-06, normal path with NO fault demo armed — `RLX_GATE_MOTION_MS = 3000` (`rlx_gate.h:10`) so the gate confirms closed ~3 s after entering CLOSING, i.e. ~8 s after `TRAIN_APPROACHING` (5 s WARNING + ~3 s motion, rounded up to the next 1 s tick).
- **Environment**: (A)
- **Setup**: Do NOT press `x` (do not arm the fault). Press `0` to start TRAIN_APPROACHING.
- **Steps**: Start the stopwatch at `"flashers ON"`, stop at `"train signal PROCEED for direction 0 (gates confirmed closed)"`.
- **Expected Result**: **7.5–9.5 s** (5 s + 3 s motion, rounded to `RLx`'s 1000 ms tick, plus manual error). This value must be much smaller than the 20 s fault threshold in RC-TIME-02, demonstrating the design's safety margin.

### RC-TIME-04: Positive — occupancy window exactly 20 s before reopening begins
- **Type**: Positive
- **Related**: RC-04, `RLX_OCCUPANCY_WINDOW_MS = 20000` (`rlx_timer.h:21`), counted down at `rlx_fsm.c:372-390` via `rlx_timer_tick_window()`
- **Environment**: (A)
- **Setup**: From a confirmed CLOSED state (continuing from RC-TIME-03), wait for `RLX_EXPECTED_ARRIVAL_MS` (20 s, internal placeholder) to elapse so the system auto-transitions to `TRAIN_PRESENT` (`rlx_fsm.c:362-370`) — this simulates "the train has arrived", not itself part of the RC-04 test but needed to start the real 20 s countdown.
- **Steps**: Start the stopwatch as soon as the internal state changes (there's no dedicated log line for entering TRAIN_PRESENT in the current build — use an estimated reference point = `PROCEED` timestamp + 20 s, or add a temporary log line if the Verifier allows it for this test). Stop at `"all train signals -> STOP (crossing reopening)"` (start of OPENING).
- **Expected Result**: Interval from entering `TRAIN_PRESENT` to `"all train signals -> STOP"` falls within **19.5–20.5 s**.
- **Note**: since there is no log line marking entry into `TRAIN_PRESENT`, this test is hard to measure precisely from the console alone — recommend the Verifier Agent add a temporary `printf`/log for this test run, or accept indirect measurement via the total `TRAIN_APPROACHING` → `OPENING` time (= 5 s + ~3 s + 20 s (RLX_EXPECTED_ARRIVAL_MS) + 20 s (RLX_OCCUPANCY_WINDOW_MS) ≈ 48 s) and cross-check against the formula instead of measuring the 20 s segment in isolation.

### RC-TIME-05: Edge case — two overlapping occupancy windows, reopens only once BOTH windows expire
- **Type**: Edge case
- **Related**: RC-04 ("Gates remain closed until every active occupancy window... has elapsed"), `rlx_fsm.c:384-389` (`if (active_window_count == 0) enter_opening()`)
- **Environment**: (A)
- **Setup**: After the direction-0 train has entered `TRAIN_PRESENT` (20 s countdown window started), press key `1` (TRAIN_APPROACHING direction 1) at around second 10 to register a second, offset window.
- **Steps**: Measure the time reopening occurs (`"all train signals -> STOP"` signals the start of OPENING).
- **Expected Result**: Reopening must occur later than the single-train scenario (RC-TIME-04) by an amount corresponding to the offset of the second window's registration (~10 s later), **not** reopening at the 20 s mark of the first window — confirming correct "wait for both windows" semantics, ±1 s tolerance (1000 ms tick).

### RC-TIME-06: Positive — OPEN confirmed quickly (~3 s after entering OPENING), well within the 15 s deadline
- **Type**: Positive
- **Related**: RC-06 (internal), `RLX_OPENING_DEADLINE_MS = 15000` (`rlx_timer.h:22`), 3000 ms gate motion
- **Environment**: (A)
- **Setup**: Continuing from RC-TIME-04/05, no fault demo armed.
- **Steps**: Start the stopwatch at `"all train signals -> STOP (crossing reopening)"`, stop at `"flashers OFF (gates confirmed open)"`.
- **Expected Result**: **2.5–4.5 s** (3 s motion ± 1000 ms tick + manual error), safely below the 15 s fault threshold.

---

## 6. PA-07 — Heartbeat & watchdog

### PA-TIME-01: Positive — steady 1 Hz heartbeat
- **Type**: Positive
- **Related**: PA-07, `ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, ...)` on the `Lx` side (`lx_main.c:179`) and `RLx` (similarly) sending `MSG_HEARTBEAT` at 1 Hz.
- **Environment**: (B) or (C) — `C1` + 1 `Lx`.
- **Setup**: System running stably, no errors.
- **Steps**: Over 10 consecutive seconds (timed with a clock), count how many times a log line related to `MSG_HEARTBEAT`/STATUS from that controller is recorded on `C1` (if no per-heartbeat log exists, use the HMI `c_hmi_render` — which refreshes at 1 Hz on the same pulse — to observe `last_seen`/reset values updating continuously without "freezing").
- **Expected Result**: Observed heartbeat frequency is **9–11 times in 10 s** (1 Hz ± 10% for OS scheduling jitter), i.e. average period **0.9–1.1 s**.

### PA-TIME-02: Edge case — correct timing when marked UNAVAILABLE (2.0–3.0 s, not exactly 3.000 s)
- **Type**: Edge case
- **Related**: PA-07, `c_watchdog_mon_tick()` (`c_watchdog_mon.c:4-16`): counts `missed_heartbeat_ticks` on each 1 Hz tick of `C1` (not phase-synced with `Lx`'s heartbeat), marks unavailable when the count reaches exactly **3**. Since the watchdog tick and the last heartbeat are not phase-aligned, the actual time to reach the threshold varies within **[2.0 s, 3.0 s)** from the last heartbeat received — not exactly 3.000 s.
- **Environment**: (B) or (C)
- **Setup**: `C1` and 1 `Lx` (e.g. `L1`) running normally, exchanging heartbeats steadily.
- **Steps**: Abruptly kill `L1`'s `lx_main` process (kill -9, simulating total loss of connection — not a graceful shutdown, so there's no "intentional" final packet). Record the process-kill time (T0, using the system clock where the kill command runs, or a manual stopwatch). Watch `central_log.txt` for the line `"Controller 1 marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)"` (the `1` is because `controller_id_t` declares `CTRL_C1 = 0, CTRL_L1 = 1, ...` at `sys_types.h:18-19`, and the log prints the raw enum value, not a `controllers[]` array index) and read its second-resolution timestamp.
- **Expected Result**: The interval `[UNAVAILABLE line timestamp] - T0` falls within **1.5–4.0 s** (extra tolerance since `c_logger` only has second resolution → ±1 s rounding on top of the theoretical 2.0–3.0 s interval, plus manual T0 measurement error).

### PA-TIME-03: Negative — must NOT be marked UNAVAILABLE after only 2 missed ticks
- **Type**: Negative
- **Related**: PA-07, exact condition `missed_heartbeat_ticks == 3` (`c_watchdog_mon.c:11`) — meaning at the 2nd tick (~1.0–2.0 s after the last heartbeat) it must absolutely not yet be marked.
- **Environment**: (B) or (C)
- **Setup**: Same as PA-TIME-02, but this time **restart `L1` early** (resume sending heartbeats) at around 1.5 s after stopping it — i.e. before the 3rd tick can occur.
- **Steps**: Kill `lx_main` at T0, restart it (or simulate sending 1 heartbeat manually) at T0+1.5s. Check `central_log.txt` across the whole T0 → T0+3s window.
- **Expected Result**: NO `"marked UNAVAILABLE"` line appears in the log during this window (since `missed_heartbeat_ticks` is reset to 0 as soon as `c_server_record_status()`/a new heartbeat arrives, `c_server.c:11-12`, before reaching 3).

---

## 7. PA-11/12 — Override cap & ACK round-trip

### PA-TIME-04: Negative + Edge case — the 300000 ms cap is the correct ACK/NACK boundary
- **Type**: Negative (over-cap value) + Edge case (N/N-1 boundary)
- **Related**: PA-11, `LX_OVERRIDE_DURATION_CAP_MS = 300000` (`lx_timer.h:36`), checks `duration_ms == 0 || duration_ms > LX_OVERRIDE_DURATION_CAP_MS` at `lx_fsm.c:701` (and the preliminary validation layer on the `C1` side, `c_mode_eng.c:113`, same 300000 threshold).
- **Environment**: (B) or (C)
- **Setup**: `L1` in `NORMAL_OPERATION`, no railway pre-emption/fault/ped-clearance running (to avoid falling into a different `NACK` branch or `ACK_PENDING` that would confound the result).
- **Steps**:
  1. Send `REQUEST_OVERRIDE` to `L1` with `duration_ms = 300000` (exactly the cap).
  2. Send `REQUEST_OVERRIDE` (after canceling the previous override via `CANCEL_OVERRIDE`) with `duration_ms = 300001`.
- **Expected Result**: Step 1 → `"C1: REQUEST_OVERRIDE to 1 -> ACK"`. Step 2 → `"C1: REQUEST_OVERRIDE to 1 -> NACK reason=INVALID_DURATION"`. Exactly at the cap is valid (only `> cap` is rejected, not `>=`).

### PA-TIME-05: Edge case — duration_ms = 0 is also NACKed (not just an upper bound)
- **Type**: Edge case
- **Related**: PA-11, same `duration_ms == 0` condition at `lx_fsm.c:701`
- **Environment**: (B) or (C)
- **Setup**: Same as above, no override currently active.
- **Steps**: Send `REQUEST_OVERRIDE` with `duration_ms = 0`.
- **Expected Result**: `"C1: REQUEST_OVERRIDE to 1 -> NACK reason=INVALID_DURATION"` — confirms the PA-11 cap range is **(0, 300000]**, 0 is not accepted even though "0 ≤ cap" in theory.

### PA-TIME-06: Positive — ACK returns within 1 round-trip, in practice well under 1 s
- **Type**: Positive
- **Related**: PA-12, the synchronous `MsgSend`/`MsgReply` mechanism — latency is bounded only by OS scheduling + Qnet network delay, with no artificial delay logic in `lx_fsm_on_request_override()` for the immediate-ACK branch (`lx_fsm.c:729-736`).
- **Environment**: (C) — important for measuring round-trip over a real network rather than internal loopback (B), which is near-0 ms and doesn't reflect real Qnet latency between VMs.
- **Setup**: `L1` ready to receive commands, no condition causing `ACK_PENDING`.
- **Steps**: On `C1`'s console, type command `o` (REQUEST_OVERRIDE) and press Enter — start the stopwatch the instant Enter is pressed. Stop as soon as the line `"C1: REQUEST_OVERRIDE to 1 -> ACK"` appears on the console.
- **Expected Result**: Measured latency **< 1.0 s** — in practice on a local LAN expected to be just **tens to a few hundred milliseconds**; since `c_logger` only prints seconds, visually both events will usually fall within the **same displayed second** on the console, sufficient to conclude PA-12 is met (sub-second precision isn't needed for this test — just confirming there's no "visible" multi-second delay).

### PA-TIME-07: Edge case — ACK_PENDING still responds instantly even though activation is deferred
- **Type**: Edge case
- **Related**: PA-12 ("only activation is deferred"), the `ped_clearance_active` branch at `lx_fsm.c:713-728` returns `RESULT_ACK_PENDING` immediately, while actual activation waits until `lx_fsm_on_phase_timer()` detects `ped_clearance_active` has turned off.
- **Environment**: (B) or (C)
- **Setup**: Trigger a pedestrian request (WALK+FDW running, total ~10 s per TL-TIME-07) on `L1`, then **immediately while WALK/FDW is still running**, send `REQUEST_OVERRIDE`.
- **Steps**: Start the stopwatch when sending the command, stop when `"C1: REQUEST_OVERRIDE to 1 -> ACK_PENDING"` appears — this is the immediate response to measure (< 1 s). Then separately continue observing and measure the interval from sending the command to when the override **actually takes effect** (e.g. observe the signal output changing per `override_target_movement`, or a corresponding log line if available) — this interval is ALLOWED to last as long as the remaining ped clearance sequence (up to ~10 s per TL-05/06), without violating PA-12.
- **Expected Result**: The `ACK_PENDING` response appears **within 1 s** (in practice nearly instant, similar to PA-TIME-06); actual activation may be delayed by several more seconds (as designed) and this **does not** count as a violation of PA-12's 1 s response threshold, since these are two distinct milestones (ACK vs. activation).

---

## Appendix: Summary list of test cases

| ID | Group | Type | Environment |
| --- | --- | --- | --- |
| TC-TIME-01 | TC-02 | Positive | C |
| TC-TIME-02 | TC-02 | Positive | C |
| TC-TIME-03 | TC-02 | Positive | C |
| TC-TIME-04 | TC-03 | Edge case | B/C |
| TC-TIME-05 | PA-09 | Negative + Edge | A/B |
| TL-TIME-01 | TL-01 | Positive | A |
| TL-TIME-02 | TL-01 | Edge case | A |
| TL-TIME-03 | TL-01 | Positive | A |
| TL-TIME-04 | TL-01 | Edge case | A |
| TL-TIME-05 | TL-01 | Positive | A |
| TL-TIME-06 | TL-01 | Positive | A |
| TL-TIME-07 | TL-05/06 | Positive | A |
| TL-TIME-08 | TL-02 | Negative (integrity) | A |
| DP-TIME-01 | DP-02 | Positive | A |
| DP-TIME-02 | DP-02 | Edge/Known-gap | B/C |
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

**Total: 31 test cases**, covering all required timing-assumption groups (TC-01..05, TL-01..06, DP-01/02, CC-01/03, RC-03/04/06, PA-07, PA-11/12), each numeric value backed by at least one positive test and one edge/boundary test at the tick boundary, plus negative tests for the NACK thresholds per PA-09/PA-11.
