# 06. Test Plan: Race Condition / Concurrency / Bursty Input

This document contains regression test cases for subtle bugs related to
**event timing order** (timing/race) previously found and fixed in
`app/intersection/src/lx_fsm.c`, `app/railway/src/rlx_fsm.c`,
`app/shared/src/qnet_utils.c`, and `app/central/src/c_main.c` /
`c_operator.c`. Unlike other test-plan categories (functional, boundary,
negative...), this category does **not** check "does the system do the
right thing" but rather "does the system behave correctly when two (or
more) events happen at nearly the same instant" — i.e., bugs that only
appear when execution order across multiple threads/ticks lands within a
single narrow window.

## Environment Conventions

Each test case specifies the required environment:

- **(A) Single standalone node**: one process (`lx_main`, `rlx_main`, or
  `c_main`) running independently on one machine/VM QNX, no inter-node IPC
  needed. Used for races confined to a single FSM's internals (e.g.
  `lx_fsm.c` racing itself between the keyboard thread and the
  server/watchdog thread).
- **(B) Multiple nodes on the same QNX machine**: multiple processes
  (`c_main` + several `lx_main` + several `rlx_main`) running on the
  **same** machine/VM QNX, communicating via internal `name_attach`/
  `name_open` (no `TRAFFIC_NODE_MAP` needed). Used for races involving IPC
  between Central and Lx/RLx where real network latency doesn't matter.
- **(C) Multiple machines/VMs QNX over a real network**: per the topology
  in `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` (`VM_x86_Target01/02/03` +
  `TRAFFIC_NODE_MAP`). Used when real Qnet latency is needed to widen the
  race window (some races are too narrow to catch on internal loopback but
  are more exposed over a real network with higher latency/jitter).

## Why race conditions are hard to reproduce 100% of the time, and how to mitigate

1. **OS scheduling dependency**: execution order between the keyboard
   thread (`lx_sensor.c`/`rlx_sensor.c`/`c_operator.c`), the server thread
   (`ipc_server_run()`), the IPC client thread (`ipc_client_thread_main()`),
   and the watchdog thread is not guaranteed by any contract in the code —
   they only coincide when the tester happens to press keys at the right
   moment. On a lightly loaded machine, the race window can be as narrow as
   a few milliseconds.
2. **Fixed tick resolution**: `lx_fsm` ticks every 100 ms
   (`LX_PHASE_TICK_MS`), `rlx_fsm` ticks every 1000 ms — whether a keyboard
   event arrives "just before" or "just after" a tick depends on which
   half-millisecond of that tick cycle the keypress lands in, something a
   human hand cannot precisely control on every attempt.
3. **Logs only have second-level resolution**: `c_logger_log()` (central)
   timestamps with `strftime("%Y-%m-%d %H:%M:%S")` — not fine-grained
   enough to confirm the order of two events less than 1 second apart just
   by reading logs. Many test cases below therefore verify via **final
   state** (through `status_report_payload_t`/the HMI printed on C1, or
   through the immediate `printf` lines of `lx_signal.c`/`rlx_signal.c` —
   these print right when called, with no second-granularity timestamp, so
   they're still usable to confirm relative order within the same run)
   rather than trying to prove exact order via timestamps.
4. **Mitigate via repetition**: each test case specifies a recommended
   minimum repeat count (usually N ≥ 10) to increase the odds of hitting
   the race window at least once. Where possible, prefer scripted key
   input via pipe/`expect`/`tmux send-keys` with millisecond-granularity
   `sleep`s instead of manual typing, since a human cannot reliably repeat
   delays under ~150-200 ms — for steps requiring finer delays than that,
   treat it as "press as fast as possible, repeat N times" rather than an
   absolute timing mark a human hand can guarantee.
5. **Failure to reproduce after N attempts does not mean the bug is
   absent**: record it as "no failure observed in N attempts" (a
   provisional pass), not "proven race-free" — this is an inherent
   limitation of manual race-condition testing, not a shortcoming of the
   test plan.

---

## Group A — Pedestrian latch / recall race (`lx_fsm.c`)

### TC-RACE-1: Re-pressing the same-side pedestrian button while WALK is active
- **Type**: Edge case
- **Why it's a race**: `lx_fsm_latch_pedestrian_request()` only sets
  `ped_recall[side]=1` when `ped_serving_mask` already has that `side`'s
  bit set (i.e. currently WALK/FDW). If the tester presses the button
  *exactly within the window* between `ped_phase` transitioning to
  `PED_PHASE_WALK` (the tick that logs `lx_signal_show_walk`) and before
  `ped_phase_elapsed_ms` reaches `LX_WALK_MS` (6000 ms), the old bug would
  drop the request because `ped_latched[side]` was "already 1" so there
  was nothing to do.
- **Related**: `lx_fsm_latch_pedestrian_request()` +
  `lx_fsm_ped_service_tick_locked()`, field `ped_recall[4]` (`lx_fsm.h`
  lines ~108-119, `lx_fsm.c` lines 425-438, 172-194).
- **Environment**: (A) single standalone `lx_main` node.
- **Setup**: Start `lx_main` with `mode=PEAK_FIXED` (default). Wait for the
  phase to enter `PHASE_ARTERIAL_GREEN` (log `SIGNAL -> ARTERIAL GREEN`).
- **Steps**:
  1. Press `1` (pedestrian side 0) — log must show `PED SIGNAL side 0 ->
     WALK` almost immediately.
  2. Within 1-2 seconds after the `WALK` line (i.e. still mid-`LX_WALK_MS`
     = 6000 ms), press `1` again.
  3. Wait out the full WALK (6 s) + FLASHING_DONT_WALK (4 s) = 10 s cycle
     until `PED SIGNAL side 0 -> DONT_WALK` appears.
  4. Keep observing: since `ped_recall[0]` was set in step 2, as soon as
     the phase returns to `PHASE_ARTERIAL_GREEN` next (or the same phase if
     long enough), a **new** WALK/FDW cycle for side 0 must auto-start
     without a third button press.
- **Expected Result**: The second press is not lost — a second
  WALK→FDW→DONT_WALK cycle for side 0 runs automatically after the first
  cycle completes, with no further action needed.
- **Note**: Since `LX_WALK_MS`/`LX_FLASHING_DONT_WALK_MS` are fixed (not a
  millisecond-narrow window), this test **reproduces nearly 100%** of the
  time as long as the second press lands 6-10 seconds after the first — no
  need for N repeats, but run at least 3 times to rule out mis-keying.

### TC-RACE-2: Re-pressing the same-side button right before the FLASHING_DONT_WALK completion tick
- **Type**: Edge case (the narrowest edge of the same bug as TC-RACE-1)
- **Why it's a race**: This window is much narrower than TC-RACE-1: the
  old bug (if it regresses) could only surface at the exact final tick of
  FDW — `lx_fsm_ped_service_tick_locked()` reads `fsm->ped_recall[side]`
  **at the exact tick** where `ped_phase_elapsed_ms >=
  LX_FLASHING_DONT_WALK_MS` to decide whether to re-latch or fully clear.
  If the button is pressed one tick before that (99 ms before the
  completion tick runs) vs. right after it, the outcome must consistently
  differ, but both paths must preserve correct semantics (no in-between
  state that loses data).
- **Related**: `lx_fsm.c` lines 172-194 (the `PED_PHASE_FLASHING_DONT_WALK`
  branch).
- **Environment**: (A) single standalone `lx_main` node.
- **Setup**: Press `1` to start WALK for side 0. Watch the log to estimate
  the transition to FDW (`PED SIGNAL side 0 -> FLASHING_DONT_WALK`, occurs
  at second 6 after the first press).
- **Steps**:
  1. After seeing the `FLASHING_DONT_WALK` line, wait roughly 4 seconds
     (by hand/stopwatch) then press `1` again **as close to the 4-second
     mark as possible** (try within 3.8-4.0 seconds after the FDW line).
  2. Record exactly which log line appears next: either `DONT_WALK` (if
     the press landed before the completion tick — request retained via
     `ped_recall`) or an immediate new WALK cycle following right after (if
     the completion tick already ran and `ped_latched[0]` was cleared to 0
     before the keypress arrived, in which case the press re-latches
     normally via `ped_latched[side]=1` rather than via `ped_recall`).
- **Expected Result**: Regardless of which branch is taken, side 0 must
  **never** end up permanently at `DONT_WALK` without a subsequent new
  WALK cycle — the second press must always be served, differing only in
  whether it's served "seamlessly" (via `ped_recall`) or "as a fresh
  request" (via `ped_latched`).
- **Note**: This exact window is very narrow (under ~100 ms) and cannot be
  hit precisely by hand — repeat at least **N=10 times**, each time
  offsetting the press by a few hundred ms around the 4-second mark, to
  increase the chance of landing on the boundary tick at least once.

### TC-RACE-3: Rapid repeated presses (≥5 times within 1 second) on the same side spanning both WALK and FDW
- **Type**: Edge case
- **Why it's a race**: This is the most realistic "button spam" scenario
  (an impatient pedestrian mashing the button) — it checks that
  `ped_recall[side]` isn't corrupted or reset with side effects by repeated
  writes (no bug arises from setting `ped_recall[side]=1` repeatedly, since
  it's idempotent, but empirical confirmation is needed that spamming
  doesn't create 2 "owed" WALK cycles instead of exactly 1).
- **Related**: `ped_recall[4]`, `lx_fsm_latch_pedestrian_request()`.
- **Environment**: (A) single standalone `lx_main` node.
- **Setup**: Wait for `PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. Press `1` once to start WALK for side 0.
  2. Over the next 10 seconds (the entire WALK+FDW span), press `1`
     repeatedly, about 5-10 times, unevenly spaced (some <200 ms apart,
     some seconds apart).
  3. Wait out the first cycle (`DONT_WALK`).
- **Expected Result**: Exactly **one** second WALK/FDW cycle runs next for
  side 0 (not 5-10 cycles, not 0 cycles) — proving repeated spamming still
  counts as a single "pending" request, matching TL-06's "coalesce
  repeated presses" semantics.
- **Note**: Reproduces reliably (~100%) since it doesn't depend on a narrow
  window — run at least 3 times to confirm the served-cycle count is
  always 1, with no variance between runs.

### TC-RACE-4: Simultaneous presses on different sides within the same 100ms tick
- **Type**: Edge case
- **Why it's a race**: `ped_serving_mask` is computed once when
  `ped_phase == PED_PHASE_NONE` by merging all `ped_latched[]` entries
  compatible with the current phase (`compatible_mask`). If two keys (`1`
  and `2`, both arterial-compatible) are typed within the same 100 ms tick
  before `lx_fsm_ped_service_tick_locked()` runs, both must be merged into
  **the same** `ped_serving_mask` and served in the same WALK/FDW cycle —
  if there's a lock-ordering bug (`fsm->lock`) between the two
  `lx_fsm_latch_pedestrian_request()` calls from the keyboard thread and
  the tick's read, one of them could be dropped from this cycle's
  `compatible_mask` and forced to wait for the next ARTERIAL_GREEN cycle.
- **Related**: `lx_fsm_ped_service_tick_locked()` lines 129-158 (computing
  `compatible_mask`), lock `fsm->lock`.
- **Environment**: (A) single standalone `lx_main` node.
- **Setup**: Wait until entering `PHASE_ARTERIAL_GREEN` at the very start
  of the cycle (log just printed `SIGNAL -> ARTERIAL GREEN`), to maximize
  time before the next tick.
- **Steps**:
  1. Type `1` then `2` in rapid succession (both arterial-compatible
     sides) as fast as possible (under 100 ms apart if using a scripted
     input; if typing by hand, type the two characters as close together
     as possible, e.g. type "12" then Enter).
  2. Watch for the log lines `PED SIGNAL side 0 -> WALK` and `PED SIGNAL
     side 1 -> WALK`.
- **Expected Result**: Both WALK lines for side 0 and side 1 appear
  **in the same cycle** (starting together, transitioning to FDW together,
  reaching DONT_WALK together) — side 1 must never have to wait for a
  separate later cycle despite being pressed nearly simultaneously with
  side 0.
- **Note**: If the two keys land in two different 100 ms ticks (quite
  likely by hand), side 2 waiting for the next cycle is correct by design
  ("A side that latches mid-sequence... is picked up the next time a
  sequence starts") — this is **not** a bug. Only count it a failure if
  both keys are guaranteed to land in the same tick (use a script sending
  input via a single `write()` to ensure this) and they still get split
  into 2 cycles. Repeat N ≥ 10 times with a script to increase the odds of
  both keys landing in the same tick.

---

## Group B — Override request/renew/cancel race (`lx_fsm.c`)

### TC-RACE-5: REQUEST_OVERRIDE arriving right as pedestrian clearance is about to finish
- **Type**: Edge case
- **Why it's a race**: This is a recently fixed race in
  `lx_fsm_on_phase_timer()`: the re-validation code for
  `OVR_PENDING_CLEARANCE -> OVR_ACTIVE` must run **AFTER**
  `lx_fsm_ped_service_tick_locked()` in the same tick, since
  `ped_clearance_active` is only cleared inside that function. If the
  order were reversed (old bug), the override would activate one tick
  (100 ms) late — not a safety hazard but inconsistent with the spec's
  "activate immediately when clearance ends". This test forces the
  override request to land exactly on FDW's final tick to expose the
  ordering bug if it regresses.
- **Related**: `lx_fsm_on_phase_timer()` lines 904-939 (calls
  `lx_fsm_ped_service_tick_locked()` first, then checks
  `OVR_PENDING_CLEARANCE`); `lx_fsm_on_request_override()` lines 713-728.
- **Environment**: (A) single standalone `lx_main` node (needs both the
  `lx_sensor` keyboard for pedestrian presses and a way to send
  `MSG_REQUEST_OVERRIDE` — use `c_operator`'s `o` command if running
  alongside `c_main`, or a direct IPC test client if available). If only a
  standalone Lx node without Central is available, use environment (B)
  with a minimal `c_main` to get the `o` command.
- **Setup**: Press `1` to start WALK for side 0. From `c_operator`, prepare
  the input sequence for the `o` command (Lx number, target movement,
  duration) but do not press Enter on the final line yet.
- **Steps**:
  1. Watch for `PED SIGNAL side 0 -> FLASHING_DONT_WALK` (marks the start
     of the 4-second FDW).
  2. Send `MSG_REQUEST_OVERRIDE` (via `o` on `c_operator`, selecting the
     right Lx, target movement = arterial, a valid duration e.g. 30000) so
     it reaches the Lx **within 3.5-4.0 seconds** after the FDW line in
     step 1 — i.e. as close to FDW's final tick as possible.
  3. Observe the ACK/ACK_PENDING response and the next log lines on the Lx.
- **Expected Result**: If the request arrives while FDW is still active, Lx
  returns `RESULT_ACK_PENDING` and `override_substate =
  OVR_PENDING_CLEARANCE`; at the very **tick** `DONT_WALK` appears (FDW
  ending), the override must transition to `OVR_ACTIVE` **within that same
  tick** — no extra "waiting" tick between `ped_clearance_active` reaching
  0 and the override activating. Confirm via
  `status_report_payload_t.override_active` or by observing the physical
  phase switching to the overridden movement right at the next ALL_RED
  boundary without additional half-cycle delay.
- **Note**: Since the tick unit is 100 ms, precisely timing the "final
  tick" by hand is difficult. Repeat at least N ≥ 10 times, offsetting the
  request's send time by a few hundred ms around the 4-second mark, to
  ensure at least one attempt lands within FDW's last 1-2 ticks.

### TC-RACE-6: Sending 2 REQUEST_OVERRIDE messages back-to-back extremely fast for the same Lx
- **Type**: Edge case (negative-ish, but a race since it depends on the
  arrival order of 2 messages)
- **Why it's a race**: `lx_fsm_on_request_override()` blocks a second
  override by checking `fsm->supervisory ==
  SUPERVISORY_CENTRAL_OVERRIDE` when the second request is processed. If
  two requests are sent nearly simultaneously (before the first is
  processed by `lx_fsm.c` and `supervisory` switched to
  `SUPERVISORY_CENTRAL_OVERRIDE`), there's a risk (absent correct locking)
  that both see `supervisory != CENTRAL_OVERRIDE` at once and both get
  ACKed, overwriting each other's state. `fsm->lock` must serialize the two
  `lx_fsm_on_request_override()` calls (they already run on the same Lx
  server thread so are inherently serial — a bug could only arise if a
  parallel call path is added later), ensuring the later-arriving one
  always sees the correct state left by the earlier one.
- **Related**: `lx_fsm_on_request_override()` (starting at line 731 in the
  current `lx_fsm.c`), the `NACK_REASON_OUT_OF_RANGE` branch at line 747
  when already `SUPERVISORY_CENTRAL_OVERRIDE` - note: this is a different
  use of `NACK_REASON_OUT_OF_RANGE` from the mode-validation branch newly
  added in `lx_fsm_on_set_mode()` (line 710) - different functions, only
  sharing the reason code name.
- **Environment**: (B) multiple nodes on the same machine (1 `c_main` + 1
  `lx_main`) to have a real IPC path via `ipc_client_post()`/`MsgSend()`,
  not direct C function calls.
- **Setup**: Lx in `SUPERVISORY_NORMAL_OPERATION` (no override active).
- **Steps**:
  1. On `c_operator`, type `o`, enter the target Lx, movement=arterial,
     duration=60000, Enter to send the first request.
  2. **Immediately** (within under 1 second, as fast as possible) repeat
     `o` with the same target Lx, movement=connector, duration=60000, to
     send the second request.
  3. Watch both ACK/NACK responses printed via `c_comm.c`'s
     `on_command_reply` (log line `C1: SET_MODE/REQUEST_OVERRIDE to N ->
     RESULT`).
- **Expected Result**: Exactly one of the two requests receives
  `RESULT_ACK` (or `RESULT_ACK_PENDING` if ped clearance is running), the
  other must receive `RESULT_NACK` with `NACK_REASON_OUT_OF_RANGE`. Under
  no circumstances should both be ACKed (double-processing) —
  `override_target_movement` on the Lx must match exactly the ACKed
  request, never overwritten by the NACKed one.
- **Note**: Since both requests must travel serially through the same
  `ipc_client_thread_main()` of `c_main` (only 1 client thread), send order
  is usually preserved to the destination, giving this test a fairly high
  reproduction rate (>80%) as long as the 2 `o` commands are typed back to
  back quickly in the same `c_operator` session. Run at least N ≥ 5 times
  to confirm stability.

### TC-RACE-7: RENEW_OVERRIDE sent right as an override is about to expire naturally
- **Type**: Edge case
- **Why it's a race**: `override_remaining_ms` counts down every 100 ms
  tick; once `<= LX_PHASE_TICK_MS`, `lx_fsm_terminate_override_locked()` is
  called right inside `lx_fsm_on_phase_timer()`, switching
  `override_substate = OVR_NONE` and `supervisory` back to
  `NORMAL_OPERATION`. If `MSG_RENEW_OVERRIDE` arrives at that exact tick,
  `lx_fsm_on_renew_override()` requires `override_substate == OVR_ACTIVE`
  — if the expiry tick runs before the renew message is processed (though
  both run serially under `fsm->lock` so there's no true data race, only an
  **arrival-order** race), the renew must be cleanly NACKed
  (`NACK_REASON_UNKNOWN_TARGET`) rather than crashing or leaving a
  half-finished state (`override_remaining_ms` nonzero but `substate ==
  OVR_NONE`).
- **Related**: `lx_fsm_on_phase_timer()` lines 895-902 (expiry),
  `lx_fsm_on_renew_override()` lines 740-763.
- **Environment**: (B) multiple nodes on the same machine (`c_main` +
  `lx_main`).
- **Setup**: Send `REQUEST_OVERRIDE` with a very short `duration_ms`, e.g.
  `500` (half a second — still valid since `(0, 300000]`) to make the
  expiry moment easy to time.
- **Steps**:
  1. Send `o` with duration_ms=500. Record the send time (t0).
  2. Immediately after, send `r` (RENEW_OVERRIDE, extend_duration_ms=0) so
     it reaches the Lx **around t0+400 to t0+600 ms** — right around the
     expected expiry mark.
  3. Repeat multiple times with renew-send delays spread around the 500 ms
     mark (400, 450, 500, 550, 600 ms) to cover both sides of the boundary.
- **Expected Result**: For a renew arriving before expiry: `RESULT_ACK`,
  override continues with the new duration. For a renew arriving after
  expiry (log `override cleared/expired - running safe clearance sequence`
  already shown): `RESULT_NACK`/`NACK_REASON_UNKNOWN_TARGET`, and the Lx
  state must be exactly `SUPERVISORY_NORMAL_OPERATION`/`OVR_NONE` — no
  intermediate dangling state.
- **Note**: The exact window around 500 ms is hard to hit by hand within
  ±100 ms. Repeat N ≥ 10 times with varied send delays to cover both sides
  of the expiry boundary.

### TC-RACE-8: CANCEL_OVERRIDE sent right as the override expires on its own
- **Type**: Edge case
- **Why it's a race**: Similar to TC-RACE-7 but with `CANCEL_OVERRIDE`:
  both expiry (within the tick) and cancel (upon request arrival) call
  `lx_fsm_terminate_override_locked()` — this function must be
  **idempotent** under `fsm->lock` (calling it twice in a row must not
  cause double errors, must not print "override cleared" twice for the
  same override, must not decrement `override_remaining_ms` into a
  negative/uint32 underflow).
- **Related**: `lx_fsm_on_cancel_override()` lines 765-779 (guard
  `override_substate != OVR_PENDING_CLEARANCE && != OVR_ACTIVE` ->
  `NACK_REASON_UNKNOWN_TARGET`), `lx_fsm_terminate_override_locked()` lines
  205-216.
- **Environment**: (B) multiple nodes on the same machine.
- **Setup**: Same as TC-RACE-7 — override with `duration_ms=500`.
- **Steps**:
  1. Send `o` duration_ms=500 (t0).
  2. Send `c` (CANCEL_OVERRIDE, same target) so it arrives around
     t0+500ms, repeating with delays spread around the 500 ms mark as in
     TC-RACE-7.
- **Expected Result**: If cancel arrives before expiry: `RESULT_ACK`, log
  "override cleared/expired" prints exactly **once**. If cancel arrives
  after it has already expired: `RESULT_NACK`/`NACK_REASON_UNKNOWN_TARGET`
  (already `OVR_NONE`), and "override cleared/expired" still prints
  exactly **once** (from the expiry branch) — never twice for the same
  expiry event, no negative/overflowed `override_remaining_ms`.
- **Note**: Repeat N ≥ 10 times with delays spread around the expiry mark.

### TC-RACE-9: Railway preemption (crossing status) arriving right as a REQUEST_OVERRIDE is in flight
- **Type**: Edge case
- **Why it's a race**: CC-02 requires that an override never be granted
  conflicting with railway preemption, and if preemption starts **while**
  an override is active, it must be evicted via
  `lx_fsm_terminate_override_locked()` (`lx_fsm_on_crossing_status()` lines
  792-798). If `MSG_REQUEST_OVERRIDE` and `MSG_CROSSING_STATUS` (sent by
  RLx upon detecting a train) arrive at the Lx nearly simultaneously,
  processing order determines the outcome: if the override is processed
  before preemption arrives, it must be evicted as soon as preemption
  arrives afterward (known bug: CC-02 "cannot grant override conflicting
  with active railway preemption" — the request must be NACKed if
  preemption arrived **first**, and must be cleanly evicted if preemption
  arrives **after**).
- **Related**: `lx_fsm_on_request_override()` lines 705-709
  (`NACK_REASON_RAILWAY_CONFLICT`), `lx_fsm_on_crossing_status()` lines
  781-833.
- **Environment**: (B) multiple nodes on the same machine: `c_main` + 1
  `lx_main` + 1 `rlx_main` (this is a demo scenario; in the real system
  `crossing_status` comes directly from RLx, not via Central — verify the
  actual wiring in `ipc_msg.h`/`c_server.c` to confirm the true path; if
  `MSG_CROSSING_STATUS` is only sent by RLx to the relevant Lx and not via
  C1, the demo topology's target mapping needs to be configured
  accordingly).
- **Setup**: Lx in `NORMAL_OPERATION`, RLx in `RLX_OPEN`.
- **Steps**:
  1. Prepare the `o` command on `c_operator` (but don't Enter the final
     line yet, or be ready to send it on cue).
  2. On `rlx_sensor`, press `0` (TRAIN_APPROACHING direction 0) so RLx
     starts the WARNING → CLOSING → CLOSED sequence, leading RLx to send
     `MSG_CROSSING_STATUS(CLOSED)` to the relevant Lx.
  3. **Right after pressing `0`** (within under 1 second), send `o` on
     `c_operator` requesting an override for that same Lx.
  4. Repeat in reverse order: override first, then press `0` right after
     (within under 1 second) so the override gets ACKed before preemption
     arrives.
- **Expected Result**:
  - If preemption arrives **before** the override: the override must be
    `RESULT_NACK`/`NACK_REASON_RAILWAY_CONFLICT`.
  - If the override is ACKed **before** preemption arrives: the override
    must be evicted via safe clearance (log "override cleared/expired") and
    `supervisory` switches to `SUPERVISORY_RAILWAY_PREEMPTION`, never
    remaining `SUPERVISORY_CENTRAL_OVERRIDE` alongside preemption.
  - There must never be a state where both `override_active` and railway
    preemption are simultaneously "active" in `status_report_payload_t`.
- **Note**: Because this depends on the real delay of the WARNING sequence
  (5s before entering CLOSING) before `CROSSING_STATUS(CLOSED)` is
  actually sent (not immediately upon pressing `0`), the real race window
  is wider than initially expected — confirm beforehand by reading
  `RLX_WARNING_TO_CLOSING_MS` (5000ms)/`RLX_CLOSING_DEADLINE_MS` (15000ms)
  in `rlx_timer.h` to time exactly when `CROSSING_STATUS` is actually sent
  (not when the key is pressed), then send the override around that mark.
  Repeat N ≥ 5 times for each direction.

### TC-RACE-10: Watchdog trip occurring right as an override is ACTIVE
- **Type**: Edge case
- **Why it's a race**: `lx_fsm_report_watchdog_trip()` is called from a
  **separate watchdog thread** (not the server thread), and it takes
  `fsm->lock` itself then calls `lx_fsm_terminate_override_locked()` if
  currently `SUPERVISORY_CENTRAL_OVERRIDE` — this is a genuine two-thread
  race (unlike the message-ordering cases above): if the watchdog trip
  happens right while the server thread is mid-`lx_fsm_on_phase_timer()`
  processing an override tick (e.g. counting down
  `override_remaining_ms`), `fsm->lock` must correctly serialize these two
  operations, leaving no `override_substate` inconsistent with
  `supervisory`.
- **Related**: `lx_fsm_report_watchdog_trip()` lines 465-477 (calls
  `lx_fsm_terminate_override_locked()` directly from the watchdog thread,
  independent of the server thread — see the doc comment explaining the
  compliance-audit fix rationale).
- **Environment**: (A) single standalone `lx_main` node (internal watchdog
  trips on its own, no Central needed).
- **Setup**: Activate an override that is `OVR_ACTIVE` (via `c_main` if
  available, or by calling a direct test hook if one exists). If there's no
  way to trigger a real watchdog trip (`lx_watchdog.c` incomplete in some
  builds), use any available demo trip mechanism in
  `lx_main.c`/`lx_watchdog.c` to simulate a hung main loop.
- **Steps**:
  1. With an override `OVR_ACTIVE`, trigger the condition that causes the
     watchdog to trip (e.g. artificially block the server thread if a
     debug hook exists, or wait for a natural watchdog timeout if the
     environment supports it).
  2. Right as the watchdog trip occurs, if possible simultaneously send a
     `MSG_RENEW_OVERRIDE`/`MSG_CANCEL_OVERRIDE` at that same moment to
     increase the odds of overlap between the two threads writing the same
     fields.
- **Expected Result**: After the watchdog trip, the final state must be
  `SUPERVISORY_FAULT_SAFE`, `override_substate == OVR_NONE`,
  `override_remaining_ms == 0` — fully consistent, no conflicting field
  combination (e.g. `supervisory == FAULT_SAFE` but `override_substate ==
  OVR_ACTIVE`). Any renew/cancel sent at the same time must receive
  `NACK`/`FAULT_ACTIVE` or `UNKNOWN_TARGET`, with no process
  panic/crash.
- **Note**: This is a genuine two-thread race (unlike the IPC-message cases
  above), so it's hard to script by hand without a debug hook to force the
  trip at the right moment. If the test environment has no way to force a
  trip manually, treat this as "run when a natural opportunity arises" and
  record it via long-duration logging (run the system for hours with an
  override continuously renewed, watching for whether the watchdog ever
  trips while an override is active) rather than a deterministic
  keypress scenario.

---

## Group C — Mode / offset boundary race (`lx_fsm.c`)

### TC-RACE-11: Repeatedly changing mode before the first ALL_RED boundary
- **Type**: Edge case
- **Why it's a race**: `lx_fsm_on_set_mode()` only writes
  `fsm->pending_mode`/`mode_change_pending`; actual application only
  happens in `lx_fsm_advance_phase_locked()`'s
  `PHASE_ALL_RED_A_TO_B`/`PHASE_ALL_RED_B_TO_A`. If the operator changes
  their mind repeatedly (sending `SET_MODE` with different values) before
  the phase reaches the next ALL_RED boundary, `pending_mode` gets
  overwritten repeatedly — by design only the **last** value should be
  applied, but this must be empirically confirmed so that no intermediate
  value "leaks through" due to a processing-order bug.
- **Related**: `lx_fsm_on_set_mode()` lines 689-729 in the current
  `lx_fsm.c` (shifted down after TC-02/TC-03's `offset_extra_hold_ms` fix
  added code above it in `lx_fsm_apply_offset_locked()`),
  `lx_fsm_advance_phase_locked()` lines 239-242, 306-309.
- **Environment**: (B) `c_main` + 1 `lx_main`.
- **Setup**: Lx mid-`PHASE_ARTERIAL_GREEN` (many seconds left before the
  48s ends), current mode = PEAK_FIXED.
- **Steps**:
  1. Send `m` selecting mode=1 (OFF_PEAK_SENSOR).
  2. Right after (within 1-2 seconds, definitely still in
     ARTERIAL_GREEN), send `m` again selecting mode=0 (PEAK_FIXED).
  3. Repeat toggling back and forth 3-4 times over a few seconds, ending on
     a specific final value (e.g. mode=1) as the last command.
  4. Wait through ARTERIAL_YELLOW + ALL_RED_A_TO_B (about 4s+2s) for the
     mode to be applied.
- **Expected Result**: Right at `PHASE_ALL_RED_A_TO_B`, `fsm->mode` must
  switch to exactly the **value of the last `m` command sent** (mode=1 in
  the example above) — not any intermediate value, not reverted to the old
  value. Confirm via `status_report_payload_t.mode` after the boundary.
- **Note**: Since each `SET_MODE` is processed serially through the same Lx
  server thread, application order is stable — reproduces nearly 100%.
  Run 3 times to confirm.

### TC-RACE-12: Changing mode at both ALL_RED boundaries within one rapid cycle
- **Type**: Edge case (direct regression for a previously fixed bug)
- **Why it's a race**: This is a direct regression test for a bug where "one
  branch was dropped" — the old patch only applied `mode_change_pending`
  at `PHASE_ALL_RED_A_TO_B` and forgot `PHASE_ALL_RED_B_TO_A` (or vice
  versa). This test forces mode to change twice in a row, each time right
  before a different ALL_RED boundary in the same run, to confirm **both**
  code branches (lines 239-242 and lines 306-309 in
  `lx_fsm_advance_phase_locked()`) work, not just one.
- **Related**: `lx_fsm_advance_phase_locked()` lines 236-242 (A_TO_B
  branch) and lines 301-309 (B_TO_A branch) — the comment at lines 302-305
  notes this was "fixed after a re-verification pass caught that this got
  dropped".
- **Environment**: (B) `c_main` + 1 `lx_main`.
- **Setup**: Initial mode = PEAK_FIXED, Lx at the start of
  `PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. Right upon entering `PHASE_ARTERIAL_GREEN`, send `m` mode=1
     (OFF_PEAK_SENSOR) — applies at the upcoming `ALL_RED_A_TO_B`.
  2. Confirm via status report: `mode` switched to OFF_PEAK_SENSOR right
     after `ALL_RED_A_TO_B` (subsequent log `SIGNAL -> CONNECTOR GREEN`
     confirms the boundary was crossed).
  3. Right upon entering `PHASE_CONNECTOR_GREEN`, send `m` mode=0
     (PEAK_FIXED) — applies at the upcoming `ALL_RED_B_TO_A`.
  4. Confirm via status report after the next `SIGNAL -> ARTERIAL GREEN`
     line appears.
- **Expected Result**: Both mode changes are applied correctly — the first
  at `ALL_RED_A_TO_B`, the second at `ALL_RED_B_TO_A`. The second change
  (B_TO_A branch) must never be skipped while the first (A_TO_B branch)
  works fine — if only the A_TO_B branch works, that's exactly the bug
  recurring.
- **Note**: This test doesn't depend on a millisecond window (just send `m`
  any time during each green phase's long duration), so it reproduces
  reliably — run at least 3 times, no need for N=10.

### TC-RACE-13: SET_TIMING_PROFILE (offset) and SET_MODE sent nearly simultaneously, both awaiting a boundary
- **Type**: Edge case
- **Why it's a race**: Both `offset_apply_pending` (TC-02/TC-03) and
  `mode_change_pending` (SC-01A) are "pending flags" consumed only at a
  specific safe point: `offset_apply_pending` only **right upon entering**
  `PHASE_ARTERIAL_GREEN` (lines 339-345), while `mode_change_pending`
  consumes at the **preceding ALL_RED boundary** (lines
  239-242/306-309). If both are sent nearly at once within the current
  phase, the consumption order of these two flags overlaps the same phase
  transition — must confirm no side interaction (e.g. offset computed
  wrong because `green_elapsed_ms` was affected by the mode change, or vice
  versa).
- **Related**: `lx_fsm_apply_offset_locked()` (lines 564-614),
  `lx_fsm_advance_phase_locked()` (lines 227-347) — both pending-flag
  patterns share the "safe boundary" idea but differ in trigger point
  (ALL_RED_A_TO_B/B_TO_A for mode, start of ARTERIAL_GREEN for offset).
- **Environment**: (B) `c_main` + 1 `lx_main`, mode = PEAK_FIXED.
- **Setup**: Lx in `PHASE_CONNECTOR_GREEN` (so both commands have time to
  pass through `ALL_RED_B_TO_A` and into a new `ARTERIAL_GREEN`, where the
  offset is applied).
- **Steps**:
  1. Send `t` (SET_TIMING_PROFILE) selecting the chain containing this Lx,
     with any valid offset (< `LX_CYCLE_LENGTH_MS`=90000).
  2. Right after (within 1 second), send `m` to change mode (even though
     TC-02/TC-03 only applies when `mode == MODE_PEAK_FIXED`, still send it
     to check interaction even though switching to OFF_PEAK_SENSOR right
     after should cause the offset to be skipped, per the "apply only when
     PEAK_FIXED" design).
  3. Watch for `SIGNAL -> ALL RED (B to A)` then `SIGNAL -> ARTERIAL
     GREEN`.
- **Expected Result**:
  - If mode is still PEAK_FIXED when entering the new `ARTERIAL_GREEN`: the
    offset must be applied exactly once (not skipped, not applied twice).
  - If mode already switched to OFF_PEAK_SENSOR before entering that
    `ARTERIAL_GREEN`: by design the offset is **not** applied (since
    `lx_fsm_apply_offset_locked()`'s guard `mode != MODE_PEAK_FIXED`
    returns early) — `offset_apply_pending` must still be cleared (line
    343) even though not applied, so it isn't mistakenly applied a cycle
    later when mode switches back to PEAK_FIXED.
  - Neither pending flag should ever get "stuck" and never consumed.
- **Note**: Run both send orders (timing first/mode first) and both
  resulting mode outcomes, at least 4 combinations total, 2-3 runs each.

---

## Group D — Railway occupancy window race (`rlx_fsm.c`)

### TC-RACE-14: Two trains arriving from different directions nearly simultaneously
- **Type**: Edge case
- **Why it's a race**: `register_window()` scans `fsm->windows[]` (only
  `RLX_MAX_OCCUPANCY_WINDOWS`=2 slots) to find a free slot or an existing
  slot for the same direction. If keys `0` and `1` are typed nearly
  simultaneously (both go through `rlx_sensor_reader_thread` — **a single
  reader thread**, so inherently serialized by `scanf()` reading one
  character at a time; the real race here is between the keyboard thread
  and the 1Hz tick thread (`rlx_fsm_on_tick()`) running concurrently, which
  may be mid-`enter_train_present()`/gate polling exactly when the second
  key arrives). Must confirm both directions are tracked to the correct
  slot, with the second direction never overwriting the first's slot.
- **Related**: `register_window()` lines 83-104,
  `RLX_MAX_OCCUPANCY_WINDOWS` = 2 (`rlx_fsm.h` line 27).
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: RLx in `RLX_OPEN`.
- **Steps**:
  1. Type `0` then `1` in very quick succession (under 1 second apart,
     ideally typed together as "01" then Enter if `scanf(" %c", ...)`
     allows reading each character without an Enter — confirm the actual
     behavior of `rlx_sensor_reader_thread`).
  2. Watch the log: `flashers ON (train approaching, direction 0)` must
     appear, RLx moves to `RLX_WARNING`. The following `1` key hits the
     self-loop branch (`RLX_WARNING`/`CLOSING`/`RECLOSING`) — no second
     "flashers ON" line is printed (current design doesn't log for the
     self-loop, it just silently calls `register_window()`), but there
     must be 2 active windows.
  3. Wait through the full 5s (`RLX_WARNING_TO_CLOSING_MS`) then the 15s
     deadline (`RLX_CLOSING_DEADLINE_MS`) until the gate confirms closed —
     log `train signal PROCEED for direction 0` and `train signal PROCEED
     for direction 1` must **both** appear (see
     `check_closing_or_reclosing_complete()` looping over all active
     `windows[]`).
- **Expected Result**: Both directions have an active occupancy window and
  both receive `train signal PROCEED` when the gate finishes closing — no
  direction is dropped due to a slot overwrite.
- **Note**: Since `RLX_MAX_OCCUPANCY_WINDOWS`=2 exactly matches the max
  number of directions (RC-01 only has 2 directions), this test doesn't
  depend on a tight millisecond window — run 3 times to confirm stability.

### TC-RACE-15: Bursty trains exceeding 2 slots / repeated same-direction presses while both windows are full
- **Type**: Edge case (boundary + race combined)
- **Why it's a race**: `register_window()` has no third slot — once both
  slots are `active=1` for 2 different directions, an additional call
  (whether same direction or a hypothetical third direction) falls into a
  second loop that finds no free slot and gets "ignored defensively"
  (comment lines 102-104). Must confirm this doesn't corrupt the 2
  existing windows, and that re-calling for an already-existing direction
  (a refresh, not a new direction) still works correctly even under
  bursty calls.
- **Related**: `register_window()` lines 83-104 (particularly the refresh
  branch lines 87-92 vs. the new-slot branch lines 93-101).
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: RLx in `RLX_OPEN`.
- **Steps**:
  1. Type `0` to open a window for direction 0.
  2. Type bursty repeats `0 0 0 0 0` (5 times, <300ms apart each) — each
     must refresh `remaining_ms` for direction 0's slot (no new window
     created, `active_window_count` must not exceed 1 for this direction).
  3. Type `1` once to open a window for direction 1 (now 2 slots full).
  4. Type bursty alternating `0 1 0 1 0 1` (<300ms apart each) — both are
     refreshes of existing slots, not new slots.
- **Expected Result**: After the whole sequence, `active_window_count` must
  be exactly 2 (not 5, not 0, no runaway growth from bursty presses) —
  each repeated-direction press only refreshes `remaining_ms`, no wasted
  new slot. When both windows expire (or the gate finishes closing),
  exactly 2 `train signal PROCEED` lines are printed (not 5, not 1).
- **Note**: Since the scenario only has 2 physical directions per RC-01,
  a "true third direction" can't be tested via keyboard (`rlx_sensor.c`
  only has keys `0`/`1`) — this test focuses on confirming that bursty
  **refreshes** don't overflow the window count, which is the practically
  verifiable part via keyboard. Reproduces reliably, run 3 times.

### TC-RACE-16: Two occupancy windows expiring at nearly the same time — gate opens only when BOTH have expired
- **Type**: Edge case (regression for the RC-04 invariant)
- **Why it's a race**: `rlx_fsm_on_tick()`'s `RLX_TRAIN_PRESENT` branch
  decrements `remaining_ms` for **each** active window every tick, and only
  calls `enter_opening()` when `active_window_count == 0` — meaning even
  when both windows expire at the **same tick** (since both were
  registered nearly simultaneously in TC-RACE-14, giving them identical
  initial `remaining_ms`), the `for` loop must decrement/clear both windows
  within that tick before checking `active_window_count == 0` — if there's
  an ordering bug (checking the count right after clearing the first
  window instead of after the whole loop finishes), the gate could open
  prematurely while the second window has, in fact, also expired but the
  logic didn't guarantee it processed the entire array first.
- **Related**: `rlx_fsm_on_tick()` lines 372-391 (the `for` loop clears all
  of `windows[]` first, checks `active_window_count == 0` after the loop,
  not inside it) — matching the comment "reopening fires only when the
  COUNT of active windows reaches zero, never on a single window's expiry
  alone while another remains active" (lines 384-386).
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: Type `0` then immediately `1` (as in TC-RACE-14) so both
  windows get nearly simultaneous initial `remaining_ms` (both start
  counting 20s from entering `RLX_TRAIN_PRESENT`, not from registration —
  see `enter_train_present()` lines 178-196: every window with
  `remaining_ms==0` when entering TRAIN_PRESENT is assigned
  `RLX_OCCUPANCY_WINDOW_MS` all at once).
- **Steps**:
  1. Type `0` then `1` quickly while RLx is still `RLX_OPEN`/`RLX_WARNING`.
  2. Wait out the full WARNING(5s)→CLOSING(gate motion 3s)→CLOSED→
     EXPECTED_ARRIVAL(20s)→TRAIN_PRESENT sequence — upon entering
     `RLX_TRAIN_PRESENT`, both windows have `remaining_ms = 20000` (equal,
     since both were assigned in the same `enter_train_present()` call).
  3. **Do not** press any other keys while waiting, to ensure both windows
     count down in parallel and expire on the same tick.
  4. Watch tick-by-tick logs (if `rlx_main.c` prints debug elapsed time), or
     otherwise watch for the 2 `train signal PROCEED` lines already
     printed earlier and wait exactly 20 seconds after entering
     TRAIN_PRESENT.
- **Expected Result**: Right at second 20 after entering TRAIN_PRESENT,
  both windows expire together and `commanding gates UP (simulated
  motion...)` (from `enter_opening()`) is called exactly **once** at that
  tick — not earlier (e.g. at second 19 when 1 window should still be
  active under buggy logic), not later (no extra unnecessary wait tick).
- **Note**: Since both windows start with identical countdown values, they
  are guaranteed to expire on the same tick — this test reproduces nearly
  100% of the time, no large N needed, but run 3 times to confirm the
  observed timing by hand matches 20s ± 1 tick.

### TC-RACE-17: TRAIN_APPROACHING from another direction arriving right during OPENING
- **Type**: Edge case
- **Why it's a race**: `rlx_fsm_simulate_train_approaching()`'s
  `RLX_OPENING` branch calls `enter_reclosing()` — this transition depends
  tightly on **exactly when** the keypress lands relative to state:
  `RLX_OPEN`/`RLX_TRAIN_PRESENT`/`RLX_OPENING` are each handled
  differently. The `RLX_OPENING` window lasts only `RLX_GATE_MOTION_MS`=
  3000ms (gate opening) — the narrowest window in this entire FSM to hit.
  If hit correctly, the gate must abort the open motion and reclose
  (`enter_reclosing` calls `rlx_gate_command_close()` again) without
  leaving a dangling gate state (neither `confirmed_open` nor
  `confirmed_closed` set — a valid transient state while motion is in
  progress, but it must resolve correctly afterward).
- **Related**: `rlx_fsm_simulate_train_approaching()` lines 273-275
  (`case RLX_OPENING: enter_reclosing(fsm, direction);`),
  `enter_reclosing()` lines 169-176.
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: Bring RLx to just before entering `RLX_OPENING`: type `0`,
  wait through WARNING→CLOSING→CLOSED→(20s)→TRAIN_PRESENT→(20s occupancy
  elapses, don't press anything else)→right as `commanding gates UP`
  appears (start of `RLX_OPENING`, lasting 3000ms).
- **Steps**:
  1. Right upon seeing the lines `RLx: all train signals -> STOP (crossing
     reopening)` + `commanding gates UP (simulated motion, 3000 ms)`, press
     `1` (other direction) **within 3 seconds** of that log line — as soon
     as possible to ensure landing mid-window within the 3000ms.
  2. Watch for the next log lines: `RLx: reclosing - aborting gate-open
     motion, flashers remain active` and `commanding gates DOWN`.
- **Expected Result**: RLx transitions to `RLX_RECLOSING` (wire-visible as
  `CROSSING_WARNING` per `map_to_crossing_state()`), the gate is
  commanded to close again, and once the gate confirms closed
  (`check_closing_or_reclosing_complete()`), the window for the new
  direction (direction 1) receives `train signal PROCEED`. There must
  never be a case where RLx reports `CROSSING_OPEN` while a new train has
  actually just been registered as approaching, and it must not
  crash/hang in an undetermined gate state beyond `RLX_CLOSING_DEADLINE_MS`
  (15s) without entering `RLX_FAULT`.
- **Note**: The 3000ms window is fairly wide compared to other races so a
  human can time it, but still repeat N ≥ 5 times with press timing spread
  across the 0-3000ms range to cover both the start and end of the
  `RLX_OPENING` window. If pressed after the gate already confirmed open
  (`RLX_OPEN` reached), the correct behavior is simply opening a new window
  via the `RLX_OPEN` branch — not a bug, just outside this test's window.

### TC-RACE-18: Pressing fault-clear ('f') right as a new train approaches
- **Type**: Edge case
- **Why it's a race**: `rlx_fsm_on_fault_clear()` and
  `rlx_fsm_simulate_train_approaching()` both take `fsm->lock` so they're
  correctly serialized, but both originate from the **same keyboard
  thread** (`rlx_sensor_reader_thread` processes both `f` and `0`/`1`
  serially through the same `scanf` loop) — what needs verifying is the
  correct semantics when these two keys are pressed close together: per
  the code, when `state == RLX_FAULT`,
  `rlx_fsm_simulate_train_approaching()`'s `RLX_FAULT` case is a no-op
  (lines 277-280, "Latched until rlx_fsm_on_fault_clear() succeeds...
  approach events are ignored while faulted"). If `f` is typed right
  before a `0`/`1`, order determines the outcome: if fault-clear succeeds
  first, the new train must be registered normally (not swallowed); if
  `0`/`1` were somehow processed before `f` finishes (impossible given the
  same-thread serialization, but worth verifying), that approach event
  must be dropped entirely, not "queued" for after fault-clear.
- **Related**: `rlx_fsm_on_fault_clear()` lines 289-318,
  `rlx_fsm_simulate_train_approaching()` lines 277-280.
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: Bring RLx into `RLX_FAULT` (e.g. press `x` to arm a demo gate
  fault, then press `0` to trigger a failed gate-close confirmation cycle,
  leading to `FAULT_GATE_CONFIRM_MISSING` after
  `RLX_CLOSING_DEADLINE_MS`=15s). Wait for the gate to genuinely confirm
  open (`rlx_gate_poll_open()` must return 1) before testing — since
  `rlx_fsm_on_fault_clear()` only ACKs when `gates_confirmed_open()` is
  actually true.
- **Steps**:
  1. With RLx in `RLX_FAULT` and the gate genuinely confirmed open (demo
     fault no longer armed), type `f` then `0` in very quick succession
     (under 1 second, `f` before `0`).
  2. Observe `f`'s output: `fault-clear result=... reason=...`.
  3. Observe right after whether `0` is registered (log `flashers ON`
     must appear if fault-clear ACKed before `0` was processed).
- **Expected Result**: Since `rlx_sensor_reader_thread` processes
  characters serially through a single `while(scanf(...))` loop, `f` is
  always fully processed (including lock/unlock) before `0` is read —
  so `0` must always see the state **after** the fault has cleared
  (`RLX_OPEN`), hitting the normal `RLX_OPEN` branch and opening a new
  window, never swallowed by the `RLX_FAULT` branch. If `0` is observed to
  be swallowed (no subsequent `flashers ON`), this is a real bug (violating
  the single-threaded sensor reader's serialization assumption).
- **Note**: Since both keys go through the same single thread (no true
  OS/thread-level race here, only a business-logic-scenario race), this
  test reproduces reliably at 100% — mainly to document the "never
  swallow an event" invariant rather than catch a random bug. Run 3 times.

---

## Group E — IPC queue / Central concurrency

### TC-RACE-19: Bursty operator commands nearly filling `ipc_client_post()`'s 16-slot queue
- **Type**: Edge case
- **Why it's a race**: `ipc_client_queue_t` is a fixed-size ring buffer
  `IPC_CLIENT_QUEUE_CAPACITY`=16, consumed by **exactly one**
  `ipc_client_thread_main()` — each job must go through a blocking
  `name_open()` + `MsgSend()` before the next job is pulled from the
  queue. If the operator sends commands faster than the client thread can
  consume them (e.g. an unresponsive target causing `MsgSend()` to hang,
  or simply many consecutive broadcasts), the queue can fill and
  `ipc_client_post()` returns `-1` — must confirm the drop is handled
  safely (warning logged, no crash, operator thread not blocked since
  `ipc_client_post()` never blocks).
- **Related**: `ipc_client_post()` (`qnet_utils.c` lines 357-385,
  especially the guard `q->count == IPC_CLIENT_QUEUE_CAPACITY` at line
  368), the `c_comm_send_*()`/`c_comm_broadcast_timing_profile()`
  functions (`c_comm.c`) all log `"... dropped - outgoing queue full or
  stopping"` on a failed post.
- **Environment**: (B) multiple nodes on the same machine: `c_main` + all
  6 `lx_main` + 3 `rlx_main` (to have 9 real targets, and so `MsgSend()`
  can potentially be slower if a few targets don't respond in time — try
  shutting down 1-2 `lx_main` before testing to simulate an unresponsive
  target, making `name_open()`/`MsgSend()` in `ipc_client_thread_main()`
  take longer than usual and back up the queue).
- **Setup**: Don't start `lx_main` for L6, simulating a "hung"/nonexistent
  target (making `name_open()` fail quickly — to simulate genuine slowness
  a different approach is needed, e.g. a target that exists but whose
  `ipc_server_run()` is blocked; if a "genuinely slow" scenario can't be
  set up, at least test the "sending more than 16 commands before the
  client thread can drain them" part by sending as fast as possible).
- **Steps**:
  1. Type in rapid succession (a scripted input if possible, or as fast as
     possible by hand) ≥ 17 `m`/`o`/`t` commands (each `t` broadcast
     produces 3 `ipc_client_post()` calls for a 3-controller chain, so just
     6 consecutive `t` commands are enough to exceed 16 jobs) within a few
     seconds, without waiting for responses between commands.
  2. Watch `central_log.txt`/stdout and count the
     `"... dropped - outgoing queue full or stopping"` lines.
- **Expected Result**: When the queue fills, jobs beyond 16 must be dropped
  **with a clear log line** (never silently lost), the `c_main` process
  must not deadlock/crash, and the `c_operator_reader_thread` must
  continue accepting the next command immediately (not blocked waiting for
  the queue to drain, since `ipc_client_post()` returns `-1` immediately
  rather than waiting). Once the client thread drains the queue, subsequent
  commands (once no longer full) must process normally again.
- **Note**: It's quite hard to genuinely fill the queue on internal
  loopback since `MsgSend()` to a normally running target usually responds
  very quickly (under a few ms) — the client thread drains faster than a
  human can type. Prefer environment (C) over a real network (higher Qnet
  latency) and/or disable some targets to force `name_open()`/`MsgSend()`
  timeouts, increasing the chance of queue buildup. If a truly full queue
  can't be achieved, lower the goal to "confirm no crash/no data loss under
  moderate load" and note in the report that the full-queue branch wasn't
  proven. Repeat N ≥ 5 times.

### TC-RACE-20: Operator typing commands continuously while the server thread handles bursty heartbeats/status reports
- **Type**: Edge case
- **Why it's a race**: `c_mode_eng_t` (the `ctx.mode_eng` struct in
  `c_main.c`) is written by **two different threads** under the same
  `mode_eng_lock`: (1) `c_main`'s server thread, inside
  `ipc_server_run()`'s loop handling `on_request()`
  (`MSG_STATUS_REPORT`/`MSG_HEARTBEAT`/`MSG_FAULT_REPORT`/
  `MSG_CROSSING_STATUS` → `c_server_record_*()`) and `on_pulse()`
  (`IPC_PULSE_HEARTBEAT_TICK` every 1s → `c_watchdog_mon_tick()` +
  `c_hmi_render()`); and (2) the `c_operator_reader_thread`
  (`handle_set_mode()`/`handle_timing_profile()`/
  `handle_request_override()`/`handle_renew_override()`/
  `handle_cancel_override()`, each locking/unlocking `mode_eng_lock`
  around reading/writing
  `controllers[idx].last_commanded_mode`/`override_in_flight`/
  `last_applied_profile_id`). With 9 controllers (6 Lx + 3 RLx) sending
  bursty near-continuous status/heartbeat, the server thread holds
  `mode_eng_lock` very frequently (though each hold is very short) — if
  the operator types a command exactly while the lock is held, the
  operator's operation must **wait** (normal mutex blocking) rather than
  read/write stale data (torn read) or bypass the lock.
- **Related**: `c_main.c` lines 37-51 (field + comment explaining why
  `mode_eng_lock` was added), lines 60-89 (on_request handlers), lines
  114-117 (on_pulse: `c_watchdog_mon_tick()` + `c_hmi_render()`),
  `c_operator.c`'s every `handle_*()` (lines 116-330, all calling
  `pthread_mutex_lock(args->mode_eng_lock)`).
- **Environment**: (B) or (C) full setup: `c_main` + all 6 `lx_main` + 3
  `rlx_main` running and genuinely sending periodic status
  report/heartbeat (not simulated) to generate natural bursty load on the
  server thread.
- **Setup**: Ensure all 9 controllers are connected and sending
  status/heartbeat regularly (observe `c_hmi_render()` updating
  continuously on screen every second).
- **Steps**:
  1. While the system is fully running with 9 controllers, type several
     different operator commands rapidly and continuously: `m` (change
     mode L1), then immediately `o` (override L2), then `r` (renew L2 if
     the override was just accepted), then `t` (broadcast timing profile
     chain 1), then `c` (cancel L2) — each command under 1 second apart,
     without waiting for ACK/NACK between commands.
  2. Repeat this sequence continuously for about 30-60 seconds while the 9
     controllers keep sending status/heartbeat/crossing_status
     (natural background load, no extra intervention needed).
  3. Watch the HMI (`c_hmi_render()` output) and `central_log.txt`
     throughout.
- **Expected Result**:
  - No deadlock: both `c_operator_reader_thread` and the server thread
    must keep making progress throughout the 30-60 seconds (HMI still
    updates every second, operator still gets a response for each
    command).
  - No torn write: after stopping typing, re-reading
    `mode_eng.controllers[]` (via HMI or log) must show all fields
    (`last_commanded_mode`, `override_in_flight`, `last_applied_profile_id`)
    in a valid state, consistent with the last command sent to each
    controller — no "garbage"/invalid-enum values (a sign of reading
    mid-write without a lock).
  - `c_watchdog_mon_tick()`'s missed-heartbeat bookkeeping stays accurate:
    no controller falsely reported as a "missed heartbeat" while actually
    still sending heartbeats regularly, just because the lock was held too
    long by the operator side — confirm the lock is only held very briefly
    in each `handle_*()` (no blocking I/O inside the locked region).
- **Note**: This is a "high-frequency, subtle-consequence" race (torn
  read/write) rather than a "hit-or-miss once" race — so run continuously
  as long as possible (recommend at least 5 minutes continuous, not just
  30-60 seconds) to increase the number of observed lock contentions, and
  repeat the entire scenario across N ≥ 3 separate sessions (restarting
  the whole system between sessions) before concluding a pass.
