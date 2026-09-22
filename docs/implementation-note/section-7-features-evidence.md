# 7. Implemented Features and Operational Evidence

> **Note on evidence in this section:** this Implementation Note was assembled without a live multi-VM QNX deployment available to the author at time of writing. Section 7 is therefore written as a **reproducible demo script**: the exact binary, the exact keys to press, in which terminal, in which order, and the exact `printf`/log text the real code will emit (copied verbatim from `app/intersection/src/lx_signal.c`, `app/railway/src/rlx_signal.c`, `app/central/src/c_hmi.c`, `app/central/src/c_logger.c`, `app/intersection/src/lx_comm.c`, `app/railway/src/rlx_comm.c`, and `app/central/src/c_comm.c`/`c_main.c`). Every `[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]` placeholder below is left in place — the assessor/team must run the script on the real QNX VMs and drop the actual screenshot in before submission. No output below should be read as an observed result; it is the code's documented, deterministic behaviour for the given input.

All commands below assume binaries built per Section 2.2 (`build/bin/c_main`, `build/bin/lx_main`, `build/bin/rlx_main`) and launched per Section 2.3's startup order (C1, then RLx, then Lx).

## 7.1 Normal Traffic and Pedestrian Operation

**Feature under test:** UC-01 (Serve Vehicle Demand) and UC-02 (Serve Pedestrian Crossing Request), single-node, no Central/Railway dependency (`docs/flow-diagrams/UC-01-serve-vehicle-demand.md`, `UC-02-serve-pedestrian-crossing-request.md`). Business rules exercised: TL-01 (4s yellow / 2s all-red clearance), TL-03 (8-40s off-peak green, 4s extension increments), TL-05 (WALK → FLASHING_DONT_WALK → DONT_WALK sequencing, only on a compatible vehicle phase), TL-06 (repeated pedestrian presses are idempotent / latch, not duplicate).

**Demo script**

1. Terminal 1, on the node hosting L1: run `./lx_main 1`.
   Expected startup lines (`app/intersection/src/lx_main.c`):
   ```
   l1 (Intersection Controller) starting...
   l1: attached on traffic/l1, server loop starting.
   ```
2. L1 boots with `fsm->mode = MODE_OFF_PEAK_SENSOR` on arterial rest (no demand yet) — confirm no phase-change lines are printing on their own; the sequencer only advances on demand or a 4 s modulo check while `PHASE_ARTERIAL_GREEN` is active.
3. **Vehicle-demand cycle (OFF_PEAK_SENSOR, UC-01):** in Terminal 1's stdin, press `c` (connector vehicle present, per Table 5 — `lx_sensor.c : lx_sensor_reader_thread()` → `lx_fsm_set_connector_vehicle_demand(1)`).
   - Wait: L1 is currently resting on `PHASE_ARTERIAL_GREEN`. Once `lx_timer_should_exit_green()` decides to yield (arterial's exit guard requires a real waiting connector demand — `requires_other_demand=1`, DP-06/BR-4), the FSM walks the full clearance chain and prints one `lx_signal_show_phase()` line per transition, exact text from `lx_signal.c:42`:
     ```
     Lx 1: signal phase now ARTERIAL YELLOW
     Lx 1: signal phase now ALL RED (A to B)
     Lx 1: signal phase now CONNECTOR GREEN
     ```
   - With connector demand still asserted (`c` not yet cleared), the connector green extends in `LX_EXTENSION_MS=4000` ms increments (own_demand present, `elapsed < LX_MAX_GREEN_MS=40000`) — no new line prints during an extension, only when the phase actually advances.
   - Press `C` (clear connector demand) to let the phase exit at the next 4 s check. Expect:
     ```
     Lx 1: signal phase now CONNECTOR YELLOW
     Lx 1: signal phase now ALL RED (B to A)
     Lx 1: signal phase now ARTERIAL GREEN
     ```
   - This full sequence demonstrates TL-01 (every transition passes through a printed YELLOW line then an ALL RED line before the next GREEN) and TL-03 (green bounded 8-40 s, extended only in 4 s steps).
4. **Pedestrian cycle (UC-02):** while L1 is in `PHASE_ARTERIAL_GREEN` (compatible with sides 0/1), press `1` (pedestrian button, side 0). `lx_sensor.c` → `lx_fsm_latch_pedestrian_request(fsm, 0)` latches `ped_latched[0]=1`; this produces **no immediate output** — the request is silently latched (TL-06) until the next 100 ms tick finds a compatible phase already active. Since `ARTERIAL_GREEN` is compatible with side 0, the very next tick serves it:
   ```
   Lx 1: PED SIGNAL side 0 -> WALK
   ```
   (from `lx_signal_show_walk()`, `lx_signal.c:57`). After `LX_WALK_MS=6000` ms:
   ```
   Lx 1: PED SIGNAL side 0 -> FLASHING_DONT_WALK
   ```
   After a further `LX_FLASHING_DONT_WALK_MS=4000` ms:
   ```
   Lx 1: PED SIGNAL side 0 -> DONT_WALK
   ```
   (`lx_signal.c:62,67`). Press `1` a second time while WALK is already serving side 0 to demonstrate TL-06/idempotency: the press is recorded as `ped_recall[0]=1` internally (not a duplicate WALK) — the observable proof is that **no second `WALK` line for side 0 appears** until the current DONT_WALK cycle completes and the recall re-latches it for the next compatible phase.
5. Optional: press `q` in Terminal 1 to stop only the keyboard reader thread (expected line: whatever `lx_sensor.c`'s `q`-handler prints, e.g. `Lx sensor: stopping keyboard input (this thread only)`), confirming the 100 ms phase sequencer keeps running independently (no further sensor input possible, but phase lines continue if a phase is mid-cycle).

**Reference IDs:** UC-01, UC-02; TL-01, TL-03, TL-05, TL-06, DP-04, DP-06, PA-02, PA-04.

**Screenshot to capture:** the `lx_main 1` terminal, scrolled/cropped to show one complete vehicle-demand cycle (the `c` keypress through `ARTERIAL YELLOW` → `ALL RED (A to B)` → `CONNECTOR GREEN` → `CONNECTOR YELLOW` → `ALL RED (B to A)` → `ARTERIAL GREEN` lines) immediately followed by one complete pedestrian cycle (`1` keypress through `PED SIGNAL side 0 -> WALK` → `-> FLASHING_DONT_WALK` → `-> DONT_WALK`), all in a single legible terminal capture with the typed keys visible in the same window (local echo).

[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]

*Figure 3. Normal traffic and pedestrian operation*

## 7.2 Railway Protection and Closure Traffic

**Feature under test:** UC-04 (Protect a Railway Crossing for an Approaching Train) and UC-05 (Manage Road Traffic During and After a Railway Closure) (`docs/flow-diagrams/UC-04-protect-railway-crossing.md`, `UC-05-manage-traffic-during-railway-closure.md`). Business rules exercised: RC-01/RC-02 (only the owning RLx actuates railway equipment), RC-03 (5 s warning-to-closing lead), RC-04 (gates held closed until every active occupancy window elapses), RC-06 (train PROCEED only after sensor-confirmed gate closure — never on elapsed time alone), CC-01/CC-02 (no toward-crossing green until crossing reports OPEN), CC-03 (4 s drain-extension increments, 60 s cap).

**Demo script — three terminals.** Use RL1 (adjacent to L1 and L2 per the compile-time `ADJACENCY` table in `rlx_comm.c`).

1. Terminal A: `./rlx_main 1` on the RL1 node.
   ```
   rl1 (Railway Controller) starting...
   rl1: attached on traffic/rl1, server loop starting.
   ```
2. Terminal B: `./lx_main 1` (L1, adjacent). Terminal C: `./lx_main 2` (L2, adjacent). Both print their own `l1 (Intersection Controller) starting...` / `l2 (Intersection Controller) starting...` startup lines and settle into `PHASE_ARTERIAL_GREEN` rest with no further output.
3. **(Optional, to exercise CC-03 later) Before triggering the train:** in Terminal B, press `w` on L1 (advance queue-warning sensor, Table 5) to arm the post-preemption drain extension (`queue_warning_active=1`).
4. **Train approach:** in Terminal A, press `0` (`TRAIN_APPROACHING`, direction 0). `rlx_sensor.c` → `rlx_fsm_simulate_train_approaching(fsm, 0)`, OPEN → WARNING. Expected line (`rlx_signal.c:24`):
   ```
   RLx: flashers ON (train approaching, direction 0)
   ```
5. After `RLX_WARNING_TO_CLOSING_MS=5000` ms (WARNING → CLOSING, `enter_closing()`), the gate-motion driver prints (`rlx_gate.c:48`):
   ```
   RLx: commanding gates DOWN (simulated motion, 3000 ms)
   ```
6. Once `rlx_gate_poll_closed()` confirms closure (RC-06 — state-based, never on elapsed time alone) the FSM moves CLOSING → CLOSED and, on that same tick, `rlx_comm_broadcast_crossing_status_if_changed()` fans `MSG_CROSSING_STATUS(CROSSING_CLOSED)` out to L1, L2, and C1 (no reply awaited — RC-10). There is no dedicated "gates confirmed closed" printf on RLx's own terminal at this instant (the code only prints on the *train proceed* transition, next step); RC-06 compliance is evidenced by the fact that the "gates DOWN" line (step 5) and any train-proceed line (step 7) are separated by however long real gate-closure motion takes to confirm, not by a fixed timer.
7. At `CLOSED` state elapsed `>= 20000` ms (`enter_train_present()`, RC-04 occupancy window start), RLx prints:
   ```
   RLx: train signal PROCEED for direction 0 (gates confirmed closed)
   ```
   (`rlx_signal.c:34`, printed once per active occupancy window).
8. **Adjacent Lx reaction (UC-05, cross-node):** as soon as L1/L2 each receive `MSG_CROSSING_STATUS(state != OPEN)`, `lx_fsm_on_crossing_status()` sets `supervisory = SUPERVISORY_RAILWAY_PREEMPTION`. This produces **no dedicated printf of its own** — the observable evidence on Terminal B/C is *absence*: `PHASE_CONNECTOR_GREEN` (the toward-crossing movement) never appears as a `lx_signal_show_phase()` line while preemption is active. Two sub-cases to actually demonstrate on camera:
   - If L1 was already resting on `ARTERIAL_GREEN` when preemption began, its phase cycle continues to print `ARTERIAL YELLOW` → `ALL RED (A to B)` → **`ARTERIAL GREEN` again** (never `CONNECTOR GREEN`) — the `PHASE_ALL_RED_A_TO_B` boundary guard in `lx_fsm_advance_phase_locked()` skips straight back to arterial (CC-02: no green toward a non-open crossing).
   - If a connector green happened to already be running at the moment preemption began, the *mid-phase cutoff* fix truncates it at `LX_MIN_GREEN_MS=8000` ms regardless of remaining scheduled duration, printing `Lx 1: signal phase now CONNECTOR YELLOW` earlier than a normal cycle would, immediately followed by `ALL RED (A to B)` and then `ARTERIAL GREEN` (never re-entering connector green while preemption holds).
9. **Reopening:** in Terminal A, wait for all active occupancy windows to reach `active_window_count == 0` (`RLX_OCCUPANCY_WINDOW_MS=20000` ms per direction). RLx prints, in order:
   ```
   RLx: all train signals -> STOP (crossing reopening)
   RLx: commanding gates UP (simulated motion, 3000 ms)
   RLx: flashers OFF (gates confirmed open)
   ```
   (`rlx_signal.c:39`, `rlx_gate.c:61`, `rlx_signal.c:29`, in that order across OPENING → OPEN). The last line only prints once `rlx_gate_poll_open()` confirms the gate is actually open.
10. On the `MSG_CROSSING_STATUS(CROSSING_OPEN)` broadcast, `lx_fsm_on_crossing_status()` on L1 sets `supervisory = NORMAL_OPERATION`. **If `w` was asserted in step 3** (`queue_warning_active`), `drain_pending=1` is set; the next `ALL_RED_A_TO_B` boundary consumes it into `drain_active=1` and enters `PHASE_CONNECTOR_GREEN` for an extended drain: watch Terminal B print `Lx 1: signal phase now CONNECTOR GREEN`, then hold in 4 s (`LX_EXTENSION_MS`) increments (CC-03) up to a `LX_DRAIN_MAX_EXTENSION_MS=60000` ms cap before the normal `CONNECTOR YELLOW` → `ALL RED (B to A)` → `ARTERIAL GREEN` sequence resumes. If `w` was never pressed, the reopening simply resumes ordinary phase cycling with no drain extension — press `W` first if you need to demonstrate the *non*-drain path for comparison.

**Reference IDs:** UC-04, UC-05; RC-01, RC-02, RC-03, RC-04, RC-06, RC-10, CC-01, CC-02, CC-03, TL-01.

**Screenshot to capture:** three terminal windows tiled side by side — Terminal A (RL1) showing the full crossing lifecycle from `flashers ON` through `commanding gates DOWN`, `train signal PROCEED`, `all train signals -> STOP`, `commanding gates UP`, to `flashers OFF`; Terminal B (L1) and Terminal C (L2) each showing their phase-print lines never containing `CONNECTOR GREEN` for the duration RLx's lines show anything other than `flashers OFF`/OPEN, then resuming (with the CC-03 drain-extension `CONNECTOR GREEN` hold visible in Terminal B if `w` was asserted beforehand).

[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]

*Figure 4. Railway protection and adjacent-intersection response*

## 7.3 Central Monitoring, Commands, and Local Autonomy

**Feature under test:** UC-07 (Configure Traffic Operating Parameters), UC-09 (Monitor Network Status and Faults), UC-10 (Continue Local Operation During Central Link Loss) (`docs/flow-diagrams/UC-09-monitor-network-status.md`, `UC-10-continue-local-operation-during-link-loss.md`). Business rules exercised: PA-07 (three consecutive missed heartbeats → link unavailable / `DEGRADED_LOCAL`), PA-08 (reconnect supplies full current state before new commands are accepted), DP-01/DP-02 (mode selection and auto peak-hour switching), TC-04 (loss of coordination degrades to standalone operation, never blocks the controller).

**Demo script**

1. Launch order per Section 2.3: Terminal 1 `./c_main` (Central) first.
   ```
   C1 (Central Controller) starting...
   C1: attached on traffic/c1, server loop starting.
   ```
2. Terminals 2-4: `./rlx_main 1`, `./rlx_main 2`, `./rlx_main 3`. Terminals 5-10: `./lx_main 1` through `./lx_main 6`.
3. **Status aggregation (UC-09):** watch Terminal 1. `c_main.c : on_pulse()`'s `IPC_PULSE_HEARTBEAT_TICK` case drives `c_hmi_render()` at 1 Hz regardless of how many controllers have reported in yet, printing the full 9-row table (`c_hmi.c:39-77`), header row exactly:
   ```
   ---- C1 network status ----
   ID    ROLE          MODE      PHASE  CROSSING_STATE   SUPERVISORY  FAULTS   SENSOR    OVERRIDE AVAILABILITY
   ```
   Each controller row's final column reads `UNAVAILABLE` (`c->marked_unavailable` true, the initial condition before any heartbeat has landed) until its first `MSG_HEARTBEAT`/`MSG_STATUS` arrives and is recorded by `c_server_record_status()`, at which point the same row's final column flips to `AVAILABLE`. As each `lx_main`/`rlx_main` process comes up and sends its first 1 s heartbeat, its row should visibly transition `UNAVAILABLE` → `AVAILABLE` within one heartbeat interval (~1 s) of that process starting — capture this transition mid-startup for at least one row.
4. **Validated operator command (UC-07):** in Terminal 1, press `m` (SET_MODE, Table 4). Follow the prompts (`c_operator.c:140,147`):
   ```
     Lx number (1-6): 1
     mode (0=PEAK_FIXED, 1=OFF_PEAK_SENSOR): 1
   ```
   This logs (via `c_logger_log()`, which prefixes every line with a `[YYYY-MM-DD HH:MM:SS]` timestamp and also appends to `central_log.txt`):
   ```
   Operator: SET_MODE(target=1, mode=OFF_PEAK_SENSOR) submitted
   ```
   The command is unicast via `c_comm_send_set_mode()`; its asynchronous reply is logged separately (client thread, `c_comm.c : on_command_reply()`) once L1 answers. If L1's current mode already differs from the requested one, `lx_fsm_on_set_mode()` defers actual application to L1's next `ALL_RED` phase boundary and replies `RESULT_ACK_PENDING`, so Terminal 1 should show:
   ```
   C1: SET_MODE to 1 -> ACK_PENDING
   ```
   (exact format string `"C1: %s to %d -> %s"`, `c_comm.c:111-112`, with `verb_name(MSG_SET_MODE)="SET_MODE"` and `result_name(RESULT_ACK_PENDING)="ACK_PENDING"`). If L1 was already in the requested mode, expect `... -> ACK` instead (immediate, no boundary wait — `lx_fsm_on_set_mode()`'s no-op branch). The "applied at boundary" claim is verified by watching L1's own terminal: no phase-cycle disruption occurs immediately on the `m` keypress — the mode only takes effect the next time L1 reaches an `ALL RED` phase.
5. **Heartbeat loss / DEGRADED_LOCAL (UC-10):** in the terminal running `./lx_main 3` (L3), send a real process kill — **Ctrl+C**, not `q` — to terminate the whole process (simulating a crash/link loss, not a graceful sensor-thread stop). L3's terminal ends immediately with no further output (the process is gone; it cannot log its own `DEGRADED_LOCAL` transition because that transition is detected by *other* nodes watching for L3's absence, and Central detects it independently on its own 1 s watchdog clock — see the note below).
   - **On Central (Terminal 1):** within 3 missed heartbeat ticks (~3 s) of L3's last heartbeat, `c_watchdog_mon_tick()` (`c_main.c : on_pulse()`) marks L3 `marked_unavailable=1` and logs, under `console_io_lock`:
     ```
     Controller 3 marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)
     ```
     (exact text, `c_main.c:170-171`). The same tick's `c_hmi_render()` table row for `L3` should show `AVAILABILITY = UNAVAILABLE`.
   - **Note on wording:** this exercise demonstrates Central's *independent* detection of the outage (PA-07) via a hard process kill. To see the `DEGRADED_LOCAL`/`CENTRAL_CONNECTED` wording itself (which is printed by the *Lx/RLx* side, not by Central), the link must be interrupted while the Lx process keeps running (e.g. by blocking Qnet routing to that node rather than killing the process) so that `on_heartbeat_reply()`'s callback keeps firing with `acked=0`. In that scenario, after 3 consecutive unacknowledged heartbeats, the affected node's own terminal prints (`lx_comm.c:29`, verbatim):
     ```
     Lx: 3 consecutive HEARTBEATs unacknowledged - entering DEGRADED_LOCAL (PA-07)
     ```
     While `DEGRADED_LOCAL`, that same node's `lx_fsm_local_clock_mode_check()` keeps evaluating the real wall clock every phase tick and continues serving traffic/pedestrians completely normally (TC-04) — this is the moment to demonstrate that a link-loss node is never blocked, only unsupervised.
6. **Reconnection (PA-08):** restart the killed node: `./lx_main 3` again on the same terminal (or, for the link-loss variant, restore Qnet routing). Its first successful heartbeat produces, on that node's own terminal (`lx_comm.c:31`, verbatim):
   ```
   Lx: HEARTBEAT acknowledged by C1 - reconnected, resuming CENTRAL_CONNECTED (PA-08)
   ```
   On Central's side, `c_server_record_status()` captures `was_unavailable` before clearing it, and `c_main.c : on_request()` calls `log_reconnect_if_needed()`, which logs (verbatim, `c_main.c:77`):
   ```
   Controller 3 reconnected (PA-08)
   ```
   and the same 1 Hz table row flips `AVAILABILITY` back from `UNAVAILABLE` to `AVAILABLE`. Because `heartbeat_payload_t` is defined as the full `status_report_payload_t` (PA-08's "supply current state before new commands" requirement), this single reconnect heartbeat already carries L3's complete mode/phase/supervisory/fault state — there is no separate resync message to wait for.
7. Optional: point `tools/dashboard/server.py` at a `central_log.txt`/redirected-stdout log from this run (`tail_log()` → `/state.json` → `static/app.js : poll()`) to show the same UNAVAILABLE→AVAILABLE and reconnect-log-line transition rendered in the browser dashboard as an alternative capture to a raw terminal screenshot.

**Reference IDs:** UC-07, UC-09, UC-10; PA-07, PA-08, DP-01, DP-02, TC-04.

**Screenshot to capture:** C1's terminal (Terminal 1) showing, in one legible scroll-back: the 1 Hz status table with at least one row's `AVAILABILITY` column visible in both `UNAVAILABLE` and `AVAILABLE` states across two consecutive captures (or one capture spanning the transition), the `Operator: SET_MODE(...)` and `C1: SET_MODE to 1 -> ACK_PENDING` (or `ACK`) lines from the operator command, the `Controller 3 marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)` line, and the `Controller 3 reconnected (PA-08)` line — alternatively, the live `tools/dashboard/` browser view showing the same UNAVAILABLE/AVAILABLE transition alongside a terminal capture of the reconnect log line.

[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]

*Figure 5. Central monitoring and degraded-local operation*
