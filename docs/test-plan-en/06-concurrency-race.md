# 06. Test Plan: Race Condition / Concurrency / Bursty Input

This document contains regression test cases for subtle bugs related to
**event ordering over time** (timing/race) that were previously found and
fixed in `app/intersection/src/lx_fsm.c`, `app/railway/src/rlx_fsm.c`,
`app/shared/src/qnet_utils.c`, and `app/central/src/c_main.c` /
`c_operator.c`. Unlike the other test-plan categories (functional, boundary,
negative...), this category does **not** check "does the system do the right
thing" but rather "does the system behave correctly when two (or more) events
land almost at the same instant" — i.e., bugs that only surface when
execution order across multiple threads/ticks falls into one narrow window.

## Environment conventions

Each test case states which environment it needs:

- **(A) single standalone node**: one process (`lx_main`, `rlx_main`, or
  `c_main`) running alone on one machine/QNX VM, no inter-node IPC required.
  Used for races confined to a single FSM's internals (e.g. `lx_fsm.c`
  racing against itself between the keyboard thread and the server/watchdog
  thread).
- **(B) multiple nodes on one machine**: multiple processes (`c_main` + a
  few `lx_main` + a few `rlx_main`) running on the **same** machine/QNX VM,
  communicating via internal `name_attach`/`name_open` (no `TRAFFIC_NODE_MAP`
  needed). Used for races involving IPC between Central and Lx/RLx where
  real network latency doesn't matter. Note: the actual demo deployment uses
  10 separate QNX VMs, each running exactly one controller (no two
  controllers ever truly share one machine) — so any test case labeled (B)
  in this file is really a reduced-hardware stand-in for the real (C)
  environment (with `TRAFFIC_NODE_MAP` set), used when there aren't enough
  VMs to stand up the real topology.
- **(C) multiple machines/QNX VMs over a real network**: per the topology in
  `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` (`VM_x86_Target01/02/03` +
  `TRAFFIC_NODE_MAP`). Used when real Qnet latency is needed to widen the
  race window (some races are too narrow to catch on internal loopback but
  show up more clearly over a real network with higher latency/jitter).

## Why race conditions are hard to reproduce 100%, and how to mitigate

1. **OS scheduling dependency**: the execution order between the keyboard
   thread (`lx_sensor.c`/`rlx_sensor.c`/`c_operator.c`), the server thread
   (`ipc_server_run()`), the IPC client thread (`ipc_client_thread_main()`),
   and the watchdog thread is not guaranteed by any contract in the code —
   they only happen to align when the tester presses keys at the right
   moment. On a lightly loaded machine, the race window can be as narrow as
   a few milliseconds.
2. **Fixed tick resolution**: `lx_fsm` ticks every 100 ms
   (`LX_PHASE_TICK_MS`), `rlx_fsm` ticks every 1000 ms — whether a keyboard
   event arrives "just before" or "just after" a tick depends on which half
   of the tick period the keypress happens to land in, which a human hand
   cannot aim precisely each time.
3. **Log resolution is only seconds**: `c_logger_log()` (central) timestamps
   with `strftime("%Y-%m-%d %H:%M:%S")` — not fine-grained enough to confirm
   the order of two events less than 1 second apart just by reading the log.
   So many test cases below verify via **final state** (through
   `status_report_payload_t`/the HMI printed on C1, or through the immediate
   `printf` lines from `lx_signal.c`/`rlx_signal.c` — these print the instant
   they're called, with no second-granularity timestamp, so they're still
   usable to confirm relative order within the same run) rather than trying
   to prove exact order from timestamps.
4. **Mitigation via repetition**: each test case states a recommended minimum
   number of repetitions (usually N ≥ 10) to raise the odds of hitting the
   race window at least once. Where possible, prefer sending keystrokes via a
   script piping characters through `expect`/`tmux send-keys` with
   millisecond-granularity `sleep`s rather than typing by hand, since a human
   cannot reliably repeat delays under ~150-200 ms — for steps that require a
   smaller delay than that, treat it as "press as fast as possible, repeat N
   times" rather than an absolute timing mark a human can guarantee.
5. **Failure to reproduce after N repetitions does not mean the bug is
   gone**: record it as "no failure observed in N runs" (a provisional
   pass), not "proven race-free" — this is an inherent limitation of manual
   race-condition testing, not a shortcoming of the test plan.

---

## Group A — Pedestrian latch / recall race (`lx_fsm.c`)

### TC-RACE-1: Re-pressing the same-side pedestrian button while WALK is active
- **Type**: Edge case
- **Why it's a race**: `lx_fsm_latch_pedestrian_request()` only sets
  `ped_recall[side]=1` when `ped_serving_mask` already has that `side`'s bit
  set (i.e. currently WALK/FDW). If the tester presses the button *exactly
  within the window* between `ped_phase` switching to `PED_PHASE_WALK` (the
  tick that fires `lx_signal_show_walk`) and before `ped_phase_elapsed_ms`
  reaches `LX_WALK_MS` (6000 ms), the old bug would drop the request because
  `ped_latched[side]` was "already 1" so there was seemingly nothing to do.
- **Related**: `lx_fsm_latch_pedestrian_request()` +
  `lx_fsm_ped_service_tick_locked()`, field `ped_recall[4]` (`lx_fsm.h`
  lines ~108-119, `lx_fsm.c` lines 425-438, 172-194).
- **Environment**: (A) single standalone `lx_main` node.
- **Setup**: Start `lx_main` with `mode=PEAK_FIXED` (default). Wait for the
  phase to enter `PHASE_ARTERIAL_GREEN` (log `signal phase now ARTERIAL
  GREEN`).
- **Steps**:
  1. Press `1` (pedestrian side 0) — log should print `PED SIGNAL side 0 ->
     WALK` almost immediately.
  2. Within 1-2 seconds of seeing `WALK` (i.e. still within `LX_WALK_MS`=6000
     ms), press `1` again.
  3. Wait out the full WALK (6 s) + FLASHING_DONT_WALK (4 s) = 10 s cycle
     until `PED SIGNAL side 0 -> DONT_WALK` appears.
  4. Keep watching: since `ped_recall[0]` was set in step 2, as soon as the
     phase returns to `PHASE_ARTERIAL_GREEN` next time (or the same phase if
     long enough), a **new** WALK/FDW cycle for side 0 must auto-start
     without a third button press.
- **Expected result**: The second press is not lost — a second
  WALK→FDW→DONT_WALK cycle for side 0 auto-runs after the first cycle
  completes, with no extra action needed.
- **Note**: Since `LX_WALK_MS`/`LX_FLASHING_DONT_WALK_MS` are fixed (not a
  narrow millisecond window), this test reproduces **near 100%** every run
  as long as the second press lands 6-10 seconds after the first — no need
  for N repetitions, but run at least 3 times to rule out mis-presses.

### TC-RACE-2: Re-pressing the same-side button right before the FLASHING_DONT_WALK completion tick
- **Type**: Edge case (the narrowest edge of the same bug as TC-RACE-1)
- **Why it's a race**: This window is much narrower than TC-RACE-1: the old
  bug (if it regressed) could only show up at the exact final FDW tick —
  `lx_fsm_ped_service_tick_locked()` reads `fsm->ped_recall[side]` at the
  **exact tick** where `ped_phase_elapsed_ms >= LX_FLASHING_DONT_WALK_MS` to
  decide whether to re-latch or clear outright. If the button is pressed one
  tick before that (99 ms before the completion tick runs) versus right
  after it, the outcome must differ consistently, but both paths must
  preserve correct semantics (no falling in between and losing data).
- **Related**: `lx_fsm.c` lines 172-194 (the `PED_PHASE_FLASHING_DONT_WALK`
  branch).
- **Environment**: (A) single standalone `lx_main` node.
- **Setup**: Press `1` to start WALK for side 0. Watch the log to estimate
  the transition to FDW (`PED SIGNAL side 0 -> FLASHING_DONT_WALK`, occurs at
  second 6 after the first press).
- **Steps**:
  1. After seeing `FLASHING_DONT_WALK`, wait roughly 4 seconds (by
     stopwatch) then press `1` again **as close to the 4-second mark as
     possible** (try within 3.8-4.0 seconds after seeing the FDW line).
  2. Record exactly which log line appears next: either `DONT_WALK` (if the
     press landed before the completion tick — request held via
     `ped_recall`) or a new WALK cycle starting immediately (if the
     completion tick already ran and `ped_latched[0]` was cleared to 0
     before the keypress arrived, in which case the press re-latches
     normally via `ped_latched[side]=1` instead of via `ped_recall`).
- **Expected result**: Whichever branch it falls into, side 0 must **never**
  end up permanently at `DONT_WALK` with no subsequent new WALK cycle — i.e.
  the second press is always serviced, differing only in whether it's
  serviced "immediately, back-to-back" (via `ped_recall`) or "as a brand-new
  request" (via `ped_latched`).
- **Note**: This exact window is very narrow (under ~100 ms) and a human
  cannot press precisely — repeat a minimum of **N=10** times, each attempt
  timing the press a few hundred ms apart around the 4-second mark, to
  increase the odds of at least one landing exactly on the boundary tick.

### TC-RACE-3: Rapid repeated presses (≥5 within 1 second) on the same side throughout WALK and FDW
- **Type**: Edge case
- **Why it's a race**: This is the most realistic "button spam" scenario (an
  impatient pedestrian mashing the button) — checks that `ped_recall[side]`
  isn't wrongly overwritten or re-set with side effects (no bug arises from
  setting `ped_recall[side]=1` repeatedly since it's idempotent, but it
  needs empirical confirmation that spamming doesn't create 2 "owed" WALK
  cycles instead of exactly 1).
- **Related**: `ped_recall[4]`, `lx_fsm_latch_pedestrian_request()`.
- **Environment**: (A) single standalone `lx_main` node.
- **Setup**: Wait for `PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. Press `1` once to start WALK for side 0.
  2. Over the next 10 seconds (the entire WALK+FDW), press `1` repeatedly,
     about 5-10 times, irregularly spaced (some presses <200 ms apart, some
     several seconds apart).
  3. Wait out the first cycle (`DONT_WALK`).
- **Expected result**: Exactly **one** second WALK/FDW cycle runs next for
  side 0 (not 5-10 cycles, not 0 cycles) — proving repeated spamming still
  counts as a single "pending" request, matching TL-06's "coalesce repeated
  presses" semantics.
- **Note**: Reproduces reliably (~100%) since it doesn't depend on a narrow
  window — run at least 3 times to confirm the served-cycle count is always
  1, with no variation between runs.

### TC-RACE-4: Simultaneous presses on different sides within the same 100ms tick
- **Type**: Edge case
- **Why it's a race**: `ped_serving_mask` is computed once when
  `ped_phase == PED_PHASE_NONE` by merging all `ped_latched[]` entries
  compatible with the current phase (`compatible_mask`). If two keys (`1`
  and `2`, both arterial-compatible) are typed within the same 100 ms tick
  before `lx_fsm_ped_service_tick_locked()` runs, both must be merged into
  the **same** `ped_serving_mask` and served together in the same WALK/FDW
  cycle — if there's a lock-ordering bug (`fsm->lock`) between the two calls
  to `lx_fsm_latch_pedestrian_request()` from the keyboard thread and the
  read in the tick, one of them could be dropped from this cycle's
  `compatible_mask` and end up waiting for the next ARTERIAL_GREEN cycle.
- **Related**: `lx_fsm_ped_service_tick_locked()` lines 129-158 (computing
  `compatible_mask`), lock `fsm->lock`.
- **Environment**: (A) single standalone `lx_main` node.
- **Setup**: Wait to enter `PHASE_ARTERIAL_GREEN` right at its start (log
  just printed `signal phase now ARTERIAL GREEN`), to have maximum time
  before the next tick.
- **Steps**:
  1. Type `1` then `2` in rapid succession (both are arterial-compatible
     sides) as fast as possible in one keystroke burst (under 100 ms apart
     if using a script to send input; if typing by hand, try to type the two
     characters as close together as possible, e.g. type "12" then Enter).
  2. Watch for `PED SIGNAL side 0 -> WALK` and `PED SIGNAL side 1 -> WALK`.
- **Expected result**: Both WALK lines for side 0 and side 1 appear **in the
  same cycle** (starting together, transitioning to FDW together, reaching
  DONT_WALK together) — side 1 must never have to wait for a separate cycle
  afterward despite being pressed nearly simultaneously with side 0.
- **Note**: If the two keys land in two different 100 ms ticks (quite likely
  by hand), side 1 legitimately waits for the next cycle per design ("A side
  that latches mid-sequence... is picked up the next time a sequence
  starts") — this is **not** a bug. Only count it as a fail if both keys are
  confirmed to land in the exact same tick (using a script sending input via
  a single `write()` to guarantee it) and still get split into 2 cycles.
  Repeat N ≥ 10 times with a script to increase the odds of both keys
  landing on the same tick.

---

## Group B — Override request/renew/cancel race (`lx_fsm.c`)

### TC-RACE-5: REQUEST_OVERRIDE arriving right as pedestrian clearance is about to finish
- **Type**: Edge case
- **Why it's a race**: This is a race that was recently fixed in
  `lx_fsm_on_phase_timer()`: the code that re-validates
  `OVR_PENDING_CLEARANCE -> OVR_ACTIVE` must run **AFTER**
  `lx_fsm_ped_service_tick_locked()` in the same tick, because
  `ped_clearance_active` is only cleared inside that function. If the order
  were reversed (old bug), the override would activate one tick (100 ms)
  late — not a safety hazard but inconsistent with the spec's "activate as
  soon as clearance ends". This test forces the override request to land on
  the very last FDW tick to expose the ordering bug if it recurs.
- **Related**: `lx_fsm_on_phase_timer()` lines 904-939 (order of calling
  `lx_fsm_ped_service_tick_locked()` first, then checking
  `OVR_PENDING_CLEARANCE`); `lx_fsm_on_request_override()` lines 713-728.
- **Environment**: (A) single standalone `lx_main` node (needs both the
  `lx_sensor` keyboard for pedestrian presses and a way to send
  `MSG_REQUEST_OVERRIDE` — use `c_operator`'s `o` command if running with
  `c_main`, or a direct IPC test client if available). If only a standalone
  Lx node without Central is available, use environment (B) with a minimal
  `c_main` just to get the `o` command.
- **Setup**: Press `1` to start WALK for side 0. From `c_operator`, prepare
  the input sequence for the `o` command (Lx number, target movement,
  duration) but don't press Enter on the last line yet.
- **Steps**:
  1. Watch for `PED SIGNAL side 0 -> FLASHING_DONT_WALK` (marks the start of
     the 4-second FDW).
  2. Send `MSG_REQUEST_OVERRIDE` (via `o` on `c_operator`, selecting the
     right Lx, target movement = arterial, a valid duration e.g. 30000) so
     it reaches Lx **within 3.5-4.0 seconds** after seeing the FDW line in
     step 1 — i.e. as close to the last FDW tick as possible.
  3. Watch the ACK/ACK_PENDING response and the subsequent log on Lx.
- **Expected result**: If the request arrives while FDW is still active, Lx
  returns `RESULT_ACK_PENDING` and `override_substate = OVR_PENDING_CLEARANCE`;
  right at the **tick where** `DONT_WALK` appears (FDW ends), the override
  must switch to `OVR_ACTIVE` **in that same tick** — no extra "waiting"
  tick between `ped_clearance_active` going to 0 and the override
  activating. Confirm via `status_report_payload_t.override_active` or by
  the physical phase switching to the overridden movement right at the next
  ALL_RED boundary with no extra half-cycle delay.
- **Note**: Since the tick unit is 100 ms, hitting the "last tick" precisely
  by hand is difficult. Repeat at least N ≥ 10 times, each time offsetting
  the request send time a few hundred ms around the 4-second mark, to ensure
  at least one attempt lands exactly on the last 1-2 FDW ticks.

### TC-RACE-6: Sending 2 REQUEST_OVERRIDE messages back-to-back very fast for the same Lx
- **Type**: Edge case (negative-ish, but a race because it depends on the
  arrival order of 2 messages)
- **Why it's a race**: `lx_fsm_on_request_override()` blocks a second
  override by checking `fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE`
  when the second request is processed. If the two requests are sent almost
  simultaneously (before the first is processed by `lx_fsm.c` and
  `supervisory` flips to `SUPERVISORY_CENTRAL_OVERRIDE`), there's a risk
  (absent correct locking) that both see `supervisory != CENTRAL_OVERRIDE`
  at once and both get ACKed, clobbering each other's state. `fsm->lock`
  must serialize the two `lx_fsm_on_request_override()` calls (they run on
  the same Lx server thread so are already inherently serial — the bug could
  only arise if a parallel call path is added later) so that whichever
  arrives second always sees the correct state left by the one before it.
- **Related**: `lx_fsm_on_request_override()` (starts at line 731 in the
  current `lx_fsm.c`), the `NACK_REASON_OUT_OF_RANGE` branch at line 747
  when already `SUPERVISORY_CENTRAL_OVERRIDE` — note: this reuses
  `NACK_REASON_OUT_OF_RANGE` differently from the mode-validation branch
  newly added in `lx_fsm_on_set_mode()` (line 710) — different functions,
  just the same reason-code name.
- **Environment**: (B) multiple nodes on one machine (1 `c_main` + 1
  `lx_main`) to have a real IPC path via `ipc_client_post()`/`MsgSend()`,
  not a direct C function call.
- **Setup**: Lx is in `SUPERVISORY_NORMAL_OPERATION` (no override running).
- **Steps**:
  1. On `c_operator`, type `o`, enter the target Lx, movement=arterial,
     duration=60000, Enter to send the first request.
  2. **Immediately** (within under 1 second, as fast as possible) repeat `o`
     for the same target Lx, movement=connector, duration=60000, sending
     the second request.
  3. Watch both responses printed via `c_comm.c`'s `on_command_reply` (log
     line `C1: SET_MODE/REQUEST_OVERRIDE to N -> RESULT`).
- **Expected result**: Exactly one of the two requests gets `RESULT_ACK` (or
  `RESULT_ACK_PENDING` if ped clearance is running), the other must get
  `RESULT_NACK` with `NACK_REASON_OUT_OF_RANGE`. There must never be a case
  where both get ACKed (double-processing) — the final
  `override_target_movement` on Lx must match the request that was ACKed,
  not be overwritten by the NACKed one.
- **Note**: Since the two requests must travel through network/IPC serially
  on the same `c_main` `ipc_client_thread_main()` (only 1 client thread),
  send order is usually preserved at the destination, so this test
  reproduces fairly reliably (>80%) as long as the two `o` commands are
  typed back-to-back quickly in the same `c_operator` session. Run at least
  N ≥ 5 times to confirm stability.

### TC-RACE-7: RENEW_OVERRIDE sent right as the override is about to naturally expire
- **Type**: Edge case
- **Why it's a race**: `override_remaining_ms` counts down every 100 ms
  tick; when `<= LX_PHASE_TICK_MS`, `lx_fsm_terminate_override_locked()` is
  called immediately inside `lx_fsm_on_phase_timer()`, switching
  `override_substate = OVR_NONE` and `supervisory` back to
  `NORMAL_OPERATION`. If `MSG_RENEW_OVERRIDE` arrives on that exact tick,
  `lx_fsm_on_renew_override()` requires `override_substate == OVR_ACTIVE` —
  if the expiry tick runs before the renew message is processed (though
  both run serially under `fsm->lock` so there's no true data race, only a
  race over **arrival order**), the renew must be cleanly NACKed
  (`NACK_REASON_UNKNOWN_TARGET`) instead of crashing or leaving a half-baked
  state (`override_remaining_ms` nonzero but `substate == OVR_NONE`).
- **Related**: `lx_fsm_on_phase_timer()` lines 895-902 (expiry),
  `lx_fsm_on_renew_override()` lines 740-763.
- **Environment**: (B) multiple nodes on one machine (`c_main` + `lx_main`).
- **Setup**: Send `REQUEST_OVERRIDE` with a very short `duration_ms`, e.g.
  `500` (half a second — still valid since `(0, 300000]`) to make timing the
  expiry easier.
- **Steps**:
  1. Send `o` with duration_ms=500. Record the send time (t0).
  2. Right after, send `r` (RENEW_OVERRIDE, extend_duration_ms=0) so it
     reaches Lx **around t0+400 to t0+600 ms** — i.e. right around the
     expected expiry mark.
  3. Repeat multiple times with the renew send delay spread around the 500
     ms mark (400, 450, 500, 550, 600 ms) to cover both sides of the
     boundary.
- **Expected result**: For a renew arriving before expiry: `RESULT_ACK`,
  override continues with the new duration. For a renew arriving after
  expiry (log `override cleared/expired - running safe clearance sequence`
  already appeared): `RESULT_NACK`/`NACK_REASON_UNKNOWN_TARGET`, and Lx's
  state must be exactly `SUPERVISORY_NORMAL_OPERATION`/`OVR_NONE` — no
  lingering in-between state.
- **Note**: The exact window around 500 ms is hard to hit by hand within
  ±100 ms. Repeat N ≥ 10 times with different send delays to cover both
  sides of the expiry boundary.

### TC-RACE-8: CANCEL_OVERRIDE sent right as the override self-expires
- **Type**: Edge case
- **Why it's a race**: Similar to TC-RACE-7 but with `CANCEL_OVERRIDE`: both
  expiry (in-tick) and cancel (on request arrival) call
  `lx_fsm_terminate_override_locked()` — this function must be
  **idempotent** under `fsm->lock` (calling it twice in a row must not cause
  double errors, must not print "override cleared" twice for the same
  override, must not underflow `override_remaining_ms` below 0/wrap the
  uint32).
- **Related**: `lx_fsm_on_cancel_override()` lines 765-779 (guard
  `override_substate != OVR_PENDING_CLEARANCE && != OVR_ACTIVE` ->
  `NACK_REASON_UNKNOWN_TARGET`), `lx_fsm_terminate_override_locked()` lines
  205-216.
- **Environment**: (B) multiple nodes on one machine.
- **Setup**: Same as TC-RACE-7 — override with `duration_ms=500`.
- **Steps**:
  1. Send `o` duration_ms=500 (t0).
  2. Send `c` (CANCEL_OVERRIDE, same target) so it arrives around t0+500ms,
     repeating with delays spread around the 500 ms mark as in TC-RACE-7.
- **Expected result**: If cancel arrives before expiry: `RESULT_ACK`, log
  "override cleared/expired" prints exactly **once**. If cancel arrives
  after it has already self-expired: `RESULT_NACK`/`NACK_REASON_UNKNOWN_TARGET`
  (since already `OVR_NONE`), and "override cleared/expired" also prints
  exactly **once** (from the expiry branch) — never twice for the same
  expiry event, no negative/overflowed `override_remaining_ms`.
- **Note**: Repeat N ≥ 10 times with delays spread around the expiry mark.

### TC-RACE-9: Railway preemption (crossing status) arriving right as a REQUEST_OVERRIDE is in flight
- **Type**: Edge case
- **Why it's a race**: CC-02 requires that an override never be granted
  conflicting with a railway preemption, and if preemption starts **while**
  an override is active, it must be evicted via
  `lx_fsm_terminate_override_locked()` (`lx_fsm_on_crossing_status()` lines
  792-798). If `MSG_REQUEST_OVERRIDE` and `MSG_CROSSING_STATUS` (sent by RLx
  when it detects a train) arrive at Lx almost simultaneously, processing
  order determines the outcome: if the override is processed before
  preemption arrives, it must be evicted as soon as preemption arrives
  afterward (known behavior: CC-02 "cannot grant override conflicting with
  active railway preemption" — the request must be NACKed if preemption
  arrived **first**, and must be cleanly evicted if preemption arrives
  **later**).
- **Related**: `lx_fsm_on_request_override()` lines 705-709
  (`NACK_REASON_RAILWAY_CONFLICT`), `lx_fsm_on_crossing_status()` lines
  781-833.
- **Environment**: (B) multiple nodes on one machine: `c_main` + 1
  `lx_main` + 1 `rlx_main` (this is the demo scenario; in the real system
  `crossing_status` actually comes directly from RLx, not via Central —
  double-check the real wiring in `ipc_msg.h`/`c_server.c` to confirm the
  real path; if `MSG_CROSSING_STATUS` is only sent by RLx to the relevant
  Lx and not through C1, configure the correct target mapping for the demo
  topology).
- **Setup**: Lx in `NORMAL_OPERATION`, RLx in `RLX_OPEN`.
- **Steps**:
  1. Prepare the `o` command on `c_operator` (but don't press Enter on the
     last line yet, or be ready to send it on cue).
  2. On `rlx_sensor`, press `0` (TRAIN_APPROACHING direction 0) so RLx
     starts the WARNING → CLOSING → CLOSED chain, leading RLx to send
     `MSG_CROSSING_STATUS(CLOSED)` to the relevant Lx.
  3. **Right after** pressing `0` (within under 1 second), send `o` on
     `c_operator` requesting an override for that same Lx.
  4. Repeat in reverse order: override first, then press `0` right after
     (within under 1 second) so the override gets ACKed before preemption
     arrives.
- **Expected result**:
  - If preemption arrives **before** the override: the override must get
    `RESULT_NACK`/`NACK_REASON_RAILWAY_CONFLICT`.
  - If the override is ACKed **before** preemption arrives: the override
    must be evicted via safe clearance (log "override cleared/expired") and
    `supervisory` must switch to `SUPERVISORY_RAILWAY_PREEMPTION`, never
    remaining `SUPERVISORY_CENTRAL_OVERRIDE` alongside preemption.
  - No state where both `override_active` and railway preemption are
    "active" simultaneously in `status_report_payload_t`.
- **Note**: Since this depends on the real delay of the WARNING chain (5s
  before CLOSING) before `CROSSING_STATUS(CLOSED)` is actually sent (not
  immediately when `0` is pressed), the actual race window is wider than
  initially expected — confirm beforehand by reading
  `RLX_WARNING_TO_CLOSING_MS` (5000ms)/`RLX_CLOSING_DEADLINE_MS` (15000ms)
  in `rlx_timer.h` to time when `CROSSING_STATUS` is actually sent (not when
  the key is pressed) then send the override around that mark. Repeat N ≥ 5
  times per direction.

### TC-RACE-10: Watchdog trip occurring right as an override is ACTIVE
- **Type**: Edge case
- **Why it's a race**: `lx_fsm_report_watchdog_trip()` is called from a
  **separate watchdog thread** (not the server thread), and it acquires
  `fsm->lock` itself and calls `lx_fsm_terminate_override_locked()` if
  currently `SUPERVISORY_CENTRAL_OVERRIDE` — this is a genuine two-thread
  race (unlike the message-ordering cases above): if the watchdog trip
  arrives exactly while the server thread is mid-way through
  `lx_fsm_on_phase_timer()` handling an override tick (e.g. decrementing
  `override_remaining_ms`), `fsm->lock` must correctly serialize the two
  operations, leaving no `override_substate` inconsistent with
  `supervisory`.
- **Related**: `lx_fsm_report_watchdog_trip()` lines 465-477 (calls
  `lx_fsm_terminate_override_locked()` directly from the watchdog thread,
  independent of the server thread — see the doc comment explaining the
  compliance-audit fix rationale).
- **Environment**: (A) single standalone `lx_main` node (internal watchdog
  trips on its own, no Central needed).
- **Setup**: Activate an override that's `OVR_ACTIVE` (via `c_main` if
  available, or a direct test hook if one exists). If there's no way to
  trigger a real watchdog trip (`lx_watchdog.c` incomplete in some builds),
  use whatever demo-trip mechanism exists in `lx_main.c`/`lx_watchdog.c` to
  simulate the main loop hanging.
- **Steps**:
  1. With an override `OVR_ACTIVE`, trigger the condition that trips the
     watchdog (e.g. artificially block the server thread if a debug hook
     exists, or wait for a natural watchdog timeout if the environment
     supports it).
  2. Right as the watchdog trip occurs, simultaneously (if possible) send an
     extra `MSG_RENEW_OVERRIDE`/`MSG_CANCEL_OVERRIDE` timed to increase the
     odds of the two threads overlapping writes to the same field.
- **Expected result**: After the watchdog trip, the final state must be
  `SUPERVISORY_FAULT_SAFE`, `override_substate == OVR_NONE`,
  `override_remaining_ms == 0` — fully consistent, no contradictory field
  combination (e.g. `supervisory == FAULT_SAFE` but
  `override_substate == OVR_ACTIVE`). Any renew/cancel sent at the same time
  must get `NACK`/`FAULT_ACTIVE` or `UNKNOWN_TARGET`, no process
  panic/crash.
- **Note**: This is a genuine two-thread race (unlike the IPC-message cases
  above), hard to script manually without a debug hook to force the trip at
  the right moment. If the test environment has no way to force a trip,
  treat this as "run when a natural opportunity arises" and record it via a
  long-run observation (running the system for hours with the override
  continuously renewed, watching for any coincidence of watchdog trip with
  active override) rather than a deterministic keypress scenario. **Mark
  Skip** in the summary report unless the team actually ran the long-run
  observation session described above (several continuous hours) or held a
  debugger paused on the server thread — a single short run is not enough
  to claim Pass for this rare two-thread race.

---

## Group C — Mode / offset boundary race (`lx_fsm.c`)

### TC-RACE-11: Changing mode repeatedly before the first ALL_RED boundary is reached
- **Type**: Edge case
- **Why it's a race**: `lx_fsm_on_set_mode()` only writes to
  `fsm->pending_mode`/`mode_change_pending`; the actual apply happens only
  at `lx_fsm_advance_phase_locked()`'s
  `PHASE_ALL_RED_A_TO_B`/`PHASE_ALL_RED_B_TO_A`. If the operator changes
  their mind multiple times (sending `SET_MODE` with different values)
  before the phase reaches the next ALL_RED boundary, `pending_mode` gets
  overwritten repeatedly — by design only the **last** value should be
  applied, but it needs confirming that no intermediate value "leaks
  through" due to a processing-order bug.
- **Related**: `lx_fsm_on_set_mode()` lines 689-729 in the current
  `lx_fsm.c` (moved further down after the TC-02/TC-03 `offset_extra_hold_ms`
  fix added code above it in `lx_fsm_apply_offset_locked()`),
  `lx_fsm_advance_phase_locked()` lines 239-242, 306-309.
- **Environment**: (B) `c_main` + 1 `lx_main`.
- **Setup**: Lx is mid-`PHASE_ARTERIAL_GREEN` (still many seconds before the
  48s end), current mode = PEAK_FIXED.
- **Steps**:
  1. Send `m` selecting mode=1 (OFF_PEAK_SENSOR).
  2. Right after (within 1-2 seconds, definitely still in ARTERIAL_GREEN),
     send `m` again selecting mode=0 (PEAK_FIXED).
  3. Repeat toggling back and forth 3-4 times over a few seconds, ending
     with a definite value (e.g. mode=1) as the final command.
  4. Wait out ARTERIAL_YELLOW + ALL_RED_A_TO_B (about 4s+2s) for the mode
     to apply.
- **Expected result**: Right at `PHASE_ALL_RED_A_TO_B`, `fsm->mode` must
  switch to exactly the value of the **last `m` command sent** (mode=1 in
  the example above) — not any intermediate value, not falling back to the
  old value. Confirm via `status_report_payload_t.mode` after the boundary.
- **Note**: Since each `SET_MODE` is processed serially on the same Lx
  server thread, apply order is stable — reproduces near 100%. Run 3 times
  to confirm.

### TC-RACE-12: Repeated mode changes right at both ALL_RED boundaries in one quick cycle
- **Type**: Edge case (direct regression for a fixed bug)
- **Why it's a race**: This is a direct regression test for a bug that once
  "dropped one branch" — the old patch only applied `mode_change_pending` at
  `PHASE_ALL_RED_A_TO_B` and forgot `PHASE_ALL_RED_B_TO_A` (or vice versa).
  This test forces two consecutive mode changes, each timed right before a
  different ALL_RED boundary in the same run, to confirm **both** code
  branches (lines 239-242 and lines 306-309 in
  `lx_fsm_advance_phase_locked()`) work, not just one.
- **Related**: `lx_fsm_advance_phase_locked()` lines 236-242 (A_TO_B branch)
  and lines 301-309 (B_TO_A branch) — the comment at lines 302-305 notes
  this was "fixed after a re-verification pass caught that this got
  dropped".
- **Environment**: (B) `c_main` + 1 `lx_main`.
- **Setup**: Initial mode = PEAK_FIXED, Lx at the start of
  `PHASE_ARTERIAL_GREEN`.
- **Steps**:
  1. As soon as `PHASE_ARTERIAL_GREEN` starts, send `m` mode=1
     (OFF_PEAK_SENSOR) — applies at the upcoming `ALL_RED_A_TO_B`.
  2. Confirm via status report: `mode` changed to OFF_PEAK_SENSOR right
     after `ALL_RED_A_TO_B` (the subsequent `signal phase now CONNECTOR
     GREEN` log confirms the boundary was crossed).
  3. As soon as `PHASE_CONNECTOR_GREEN` starts, send `m` mode=0
     (PEAK_FIXED) — applies at the upcoming `ALL_RED_B_TO_A`.
  4. Confirm via status report after `signal phase now ARTERIAL GREEN`
     appears next.
- **Expected result**: Both mode changes apply correctly — the 1st at
  `ALL_RED_A_TO_B`, the 2nd at `ALL_RED_B_TO_A`. The second change (B_TO_A
  branch) must never be skipped while the first (A_TO_B branch) works
  normally — if only A_TO_B works, that's the original bug recurring.
- **Note**: This test doesn't depend on a millisecond window (`m` can be
  sent anytime during each long green phase), so it reproduces reliably —
  run at least 3 times for confidence, no need for N=10.

### TC-RACE-13: SET_TIMING_PROFILE (offset) and SET_MODE sent nearly simultaneously, both pending at a boundary
- **Type**: Edge case
- **Why it's a race**: Both `offset_apply_pending` (TC-02/TC-03) and
  `mode_change_pending` (SC-01A) are "pending flags" consumed only at one
  specific safe point each: `offset_apply_pending` only right at **entry
  into** `PHASE_ARTERIAL_GREEN` (lines 339-345), while
  `mode_change_pending` at the **preceding ALL_RED boundary** (lines
  239-242/306-309). If both are sent nearly simultaneously within the same
  current phase, the consumption order of these two flags overlaps on the
  same phase transition — need to confirm there's no side interaction (e.g.
  offset applied with a wrong value because `green_elapsed_ms` was affected
  by the mode change, or vice versa).
- **Related**: `lx_fsm_apply_offset_locked()` (lines 564-614),
  `lx_fsm_advance_phase_locked()` (lines 227-347) — both pending-flag
  patterns share a "safe boundary" pattern but differ in trigger point
  (ALL_RED_A_TO_B/B_TO_A for mode, start of ARTERIAL_GREEN for offset).
- **Environment**: (B) `c_main` + 1 `lx_main`, mode = PEAK_FIXED.
- **Setup**: Lx in `PHASE_CONNECTOR_GREEN` (so both commands have time to
  pass through `ALL_RED_B_TO_A` before entering the new `ARTERIAL_GREEN`,
  where offset is applied).
- **Steps**:
  1. Send `t` (SET_TIMING_PROFILE) selecting the chain containing this Lx,
     with any valid offset (< `LX_CYCLE_LENGTH_MS`=90000).
  2. Right after (within 1 second), send `m` to change mode (even though
     TC-02/TC-03 only applies when `mode == MODE_PEAK_FIXED`, still send it
     to test the interaction even if the mode switches to OFF_PEAK_SENSOR
     right after, causing offset to be skipped as designed "only apply when
     PEAK_FIXED").
  3. Watch for `signal phase now ALL RED (B to A)` then `signal phase now
     ARTERIAL GREEN`.
- **Expected result**:
  - If mode is still PEAK_FIXED when the new `ARTERIAL_GREEN` starts:
    offset must apply exactly once (not skipped, not applied twice).
  - If mode already switched to OFF_PEAK_SENSOR before entering that
    `ARTERIAL_GREEN`: by design, offset must **not** apply (since
    `lx_fsm_apply_offset_locked()`'s guard `mode != MODE_PEAK_FIXED` returns
    early) — `offset_apply_pending` must still be cleared (line 343) even
    without applying, so it doesn't get wrongly applied a cycle later when
    mode switches back to PEAK_FIXED.
  - Neither pending flag may ever get "stuck" and never consumed.
- **Note**: Run both send orders (timing first/mode first) and both
  resulting mode outcomes, at least 4 combinations total, 2-3 runs each.

---

## Group D — Railway occupancy window race (`rlx_fsm.c`)

### TC-RACE-14: Two trains arriving from different directions almost simultaneously
- **Type**: Edge case
- **Why it's a race**: `register_window()` scans `fsm->windows[]` (only
  `RLX_MAX_OCCUPANCY_WINDOWS`=2 slots) to find a free slot or an existing
  slot for the same direction. If keys `0` and `1` are typed nearly
  simultaneously (both go through `rlx_sensor_reader_thread` — **a single
  keyboard-reading thread**, so they're inherently serialized by `scanf()`
  reading one character at a time; the real race here is between the
  keyboard thread and the 1Hz tick thread (`rlx_fsm_on_tick()`) running
  concurrently and possibly handling `enter_train_present()`/gate polling
  right as the second key arrives). Need to confirm both directions are
  tracked in the correct slot, with the second direction never wrongly
  overwriting the first's slot.
- **Related**: `register_window()` lines 83-104,
  `RLX_MAX_OCCUPANCY_WINDOWS` = 2 (`rlx_fsm.h` line 27).
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: RLx in `RLX_OPEN`.
- **Steps**:
  1. Type `0` then `1` in rapid succession (under 1 second apart, ideally
     typed as "01" back-to-back then Enter if `scanf(" %c", ...)` allows
     reading each character without needing Enter — confirm against
     `rlx_sensor_reader_thread`'s actual behavior).
  2. Watch the log: `flashers ON (train approaching, direction 0)` must
     appear, RLx switches to `RLX_WARNING`. The following `1` press falls
     into the self-loop branch (`RLX_WARNING`/`CLOSING`/`RECLOSING`) — no
     second "flashers ON" line is printed (current design doesn't log
     anything extra for the self-loop, it just silently calls
     `register_window()`) but there must be 2 active windows.
  3. Wait out the 5s (`RLX_WARNING_TO_CLOSING_MS`) then the 15s deadline
     (`RLX_CLOSING_DEADLINE_MS`) until the gate confirms closed — log
     `train signal PROCEED for direction 0` and `train signal PROCEED for
     direction 1` must **both** appear (see
     `check_closing_or_reclosing_complete()` looping over all active
     `windows[]`).
- **Expected result**: Both directions have an active occupancy window and
  both get `train signal PROCEED` once the gate finishes closing — no
  direction is dropped due to a slot being overwritten.
- **Note**: Since `RLX_MAX_OCCUPANCY_WINDOWS`=2 exactly matches the max
  number of directions (RC-01 has only 2 directions), this test doesn't
  depend on a tight millisecond window — run 3 times to confirm stability.

### TC-RACE-15: Trains bursting past 2 slots / repeated same-direction bursts while both windows are full
- **Type**: Edge case (boundary + race combined)
- **Why it's a race**: `register_window()` has no third slot — once both
  slots are `active=1` with 2 different directions, one more call (whether
  same direction or a hypothetical third direction) falls into a second
  loop that finds no free slot and is "ignored defensively" (comment at
  lines 102-104). Need to confirm this behavior doesn't corrupt the
  existing 2 windows, and that repeated calls for an already-registered
  direction (a refresh, not a new direction) still work correctly even when
  bursty.
- **Related**: `register_window()` lines 83-104 (specifically the refresh
  branch lines 87-92 vs. the new-slot branch lines 93-101).
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: RLx in `RLX_OPEN`.
- **Steps**:
  1. Type `0` to open a window for direction 0.
  2. Type `0` in a rapid burst 5 times (`0 0 0 0 0`, each <300ms apart) —
     each must refresh `remaining_ms` for direction 0's slot (no new window
     created, `active_window_count` must not exceed 1 for this direction).
  3. Type `1` once to open a window for direction 1 (now 2 slots full).
  4. Type an alternating burst `0 1 0 1 0 1` (each <300ms apart) — both are
     refreshes of existing slots, not new slots.
- **Expected result**: After the whole sequence, `active_window_count` must
  be exactly 2 (not 5, not 0, no runaway growth from bursting) — each
  repeated same-direction press only refreshes `remaining_ms`, never
  wastefully creates a new slot. When both windows expire (or the gate
  finishes closing), exactly 2 `train signal PROCEED` lines print (not 5,
  not 1).
- **Note**: Since the scenario only has 2 physical directions per RC-01, a
  "real 3rd direction" can't be tested via keyboard (`rlx_sensor.c` only has
  keys `0`/`1`) — this test focuses on confirming bursty **refreshes** don't
  overflow the window count, which is the practically verifiable part via
  keyboard. Reproduces reliably, run 3 times.

### TC-RACE-16: Two occupancy windows expiring almost simultaneously — gate opens only when BOTH expire
- **Type**: Edge case (regression for the RC-04 invariant)
- **Why it's a race**: `rlx_fsm_on_tick()`'s `RLX_TRAIN_PRESENT` branch
  decrements `remaining_ms` for **each** active window every tick, and only
  calls `enter_opening()` when `active_window_count == 0` — meaning even
  when both windows expire on the **same tick** (since both were registered
  almost simultaneously in TC-RACE-14 and thus have identical initial
  `remaining_ms`), the `for` loop must decrement/clear both windows within
  that tick before checking `active_window_count == 0` — if there's an
  ordering bug (checking the count right after clearing the first window
  instead of after the whole loop finishes), the gate could open early while
  the second window has, in reality, also expired but the logic didn't
  guarantee the whole array was processed first.
- **Related**: `rlx_fsm_on_tick()` lines 372-391 (the `for` loop clears all
  of `windows[]` first, checks `active_window_count == 0` after the loop,
  not inside it) — matches the comment "reopening fires only when the COUNT
  of active windows reaches zero, never on a single window's expiry alone
  while another remains active" (lines 384-386).
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: Type `0` then immediately `1` (as in TC-RACE-14) so both
  windows get initial `remaining_ms` at nearly the same time (both start
  counting 20s from entering `RLX_TRAIN_PRESENT`, not from registration —
  see `enter_train_present()` lines 178-196: every window with
  `remaining_ms==0` when entering TRAIN_PRESENT is assigned
  `RLX_OCCUPANCY_WINDOW_MS` all at once).
- **Steps**:
  1. Type `0` then `1` quickly while RLx is still `RLX_OPEN`/`RLX_WARNING`.
  2. Wait out the WARNING(5s)→CLOSING(gate motion 3s)→CLOSED→
     EXPECTED_ARRIVAL(20s)→TRAIN_PRESENT chain — upon entering
     `RLX_TRAIN_PRESENT`, both windows have `remaining_ms = 20000` (equal,
     since both were assigned in the same `enter_train_present()` call).
  3. **Don't** press any other keys while waiting, to ensure both windows
     count down in parallel and expire on the same tick.
  4. Watch the log tick-by-tick (if `rlx_main.c` prints debug elapsed time;
     otherwise track the 2 previously printed `train signal PROCEED` lines
     and wait exactly 20 seconds after entering TRAIN_PRESENT).
- **Expected result**: Exactly at second 20 after entering TRAIN_PRESENT,
  both windows expire together and `commanding gates UP (simulated
  motion...)` (from `enter_opening()`) is called **exactly once** at that
  tick — not earlier (e.g. at second 19 when, under buggy logic, 1 window
  should still register as active), not later (no "extra" unnecessary wait
  tick).
- **Note**: Since both windows were initialized with the same countdown
  value, they're guaranteed to expire on the same tick — this test
  reproduces near 100%, no need for a large N, but run 3 times to confirm
  the observed timing by hand-held stopwatch matches 20s ± 1 tick.

### TC-RACE-17: TRAIN_APPROACHING for another direction arriving right during OPENING
- **Type**: Edge case
- **Why it's a race**: `rlx_fsm_simulate_train_approaching()`'s
  `RLX_OPENING` branch calls `enter_reclosing()` — this transition depends
  tightly on **exactly which state** the keypress lands in:
  `RLX_OPEN`/`RLX_TRAIN_PRESENT`/`RLX_OPENING` are each handled differently.
  The `RLX_OPENING` window lasts only `RLX_GATE_MOTION_MS`=3000ms (gate
  opening) — the narrowest window in this entire FSM to hit. If hit
  correctly, the gate must abort its opening motion and reverse to closing
  (`enter_reclosing` calls `rlx_gate_command_close()` again) without leaving
  the gate in a dangling state (half-open half-closed, both
  `confirmed_open`/`confirmed_closed` flags at 0 — this is a valid temporary
  state while motion is in progress, but it must resolve correctly
  afterward).
- **Related**: `rlx_fsm_simulate_train_approaching()` lines 273-275
  (`case RLX_OPENING: enter_reclosing(fsm, direction);`), `enter_reclosing()`
  lines 169-176.
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: Bring RLx to just before `RLX_OPENING`: type `0`, wait out the
  full chain WARNING→CLOSING→CLOSED→(20s)→TRAIN_PRESENT→(20s occupancy
  elapsed, no further keys pressed)→ right as `commanding gates UP` appears
  (start of `RLX_OPENING`, lasting 3000ms).
- **Steps**:
  1. As soon as `RLx: all train signals -> STOP (crossing reopening)` +
     `commanding gates UP (simulated motion, 3000 ms)` appear, press `1`
     (a different direction) **within 3 seconds** of that log line — as
     early as possible after seeing the log, to reliably land within the
     3000ms window.
  2. Watch for the next log: should see `RLx: reclosing - aborting
     gate-open motion, flashers remain active` and `commanding gates DOWN`.
- **Expected result**: RLx switches to `RLX_RECLOSING` (wire-visible as
  `CROSSING_WARNING` per `map_to_crossing_state()`), gate is commanded
  closed again, and once the gate confirms closed
  (`check_closing_or_reclosing_complete()`), the window for the new
  direction (direction 1) gets `train signal PROCEED`. There must never be
  a case where RLx reports `CROSSING_OPEN` while a new train has actually
  just been registered as approaching, and it must never crash/hang in an
  undetermined gate state beyond `RLX_CLOSING_DEADLINE_MS` (15s) without
  entering `RLX_FAULT`.
- **Note**: The 3000ms window is relatively wide compared to other races so
  a human can time it, but still repeat N ≥ 5 times with press timing spread
  across 0-3000ms after seeing the log, to cover both the start and end of
  the `RLX_OPENING` window. If pressed after the gate already confirmed open
  (`RLX_OPEN` entered), the correct behavior is simply opening a new window
  via the `RLX_OPEN` branch — not a bug, just outside this test's window.

### TC-RACE-18: Pressing fault-clear ('f') right as a new train approaches
- **Type**: Edge case
- **Why it's a race**: `rlx_fsm_on_fault_clear()` and
  `rlx_fsm_simulate_train_approaching()` both take `fsm->lock` so are
  correctly serialized, but both come from the **same keyboard thread**
  (`rlx_sensor_reader_thread` handles both `f` and `0`/`1` serially through
  the same `scanf` loop) — what needs verifying is correct semantics when
  these two keys are pressed close together: per the code, when
  `state == RLX_FAULT`, `rlx_fsm_simulate_train_approaching()`'s
  `RLX_FAULT` case is a no-op (lines 277-280, "Latched until
  rlx_fsm_on_fault_clear() succeeds... approach events are ignored while
  faulted"). If `f` is typed right before a `0`/`1`, order decides the
  outcome: if fault-clear succeeds first, the new train must be registered
  normally (not swallowed); if `0`/`1` somehow ran before `f` finished
  (impossible since same-thread serial, but worth verifying), that approach
  event must be dropped entirely, not "queued" for after fault-clear
  finishes.
- **Related**: `rlx_fsm_on_fault_clear()` lines 289-318,
  `rlx_fsm_simulate_train_approaching()` lines 277-280.
- **Environment**: (A) single standalone `rlx_main` node.
- **Setup**: Put RLx into `RLX_FAULT` (e.g. press `x` to arm a demo gate
  fault, then press `0` to trigger a closing cycle that fails to confirm,
  leading to `FAULT_GATE_CONFIRM_MISSING` after
  `RLX_CLOSING_DEADLINE_MS`=15s). Wait for the gate to genuinely confirm
  open (`rlx_gate_poll_open()` must return 1) before testing — since
  `rlx_fsm_on_fault_clear()` only ACKs when `gates_confirmed_open()` is
  actually true.
- **Steps**:
  1. With RLx in `RLX_FAULT` and the gate truly confirmed open (demo fault
     no longer armed), type `f` then `0` in rapid succession (under 1
     second, `f` before `0`).
  2. Watch the output of `f`: `fault-clear result=... reason=...`.
  3. Watch whether the following `0` is registered (`flashers ON` should
     appear if fault-clear ACKed before `0` was processed).
- **Expected result**: Since `rlx_sensor_reader_thread` processes each
  character serially through a single `while(scanf(...))` loop, `f` is
  always fully processed (including lock/unlock) before `0` is read — so
  `0` must always see the state **after** the fault has cleared
  (`RLX_OPEN`), falling into the normal `RLX_OPEN` branch and opening a new
  window, never swallowed by the `RLX_FAULT` branch. If `0` is observed to
  be swallowed (no subsequent `flashers ON`), that's a real bug (violates
  the single-threaded sensor reader's serialization assumption).
- **Note**: Since both keys go through the same single thread (no true
  OS/thread-level race here, only a business-logic-scenario race), this
  test reproduces reliably 100% — it's mainly documenting the "never
  swallow an event" invariant rather than catching a random bug. Run 3
  times.

---

## Group E — IPC queue / Central concurrency

### TC-RACE-19: Bursty operator commands nearly filling `ipc_client_post()`'s 16-slot queue
- **Type**: Edge case
- **Why it's a race**: `ipc_client_queue_t` is a fixed-size ring buffer
  (`IPC_CLIENT_QUEUE_CAPACITY`=16), drained by **exactly one**
  `ipc_client_thread_main()` — each job must go through a blocking
  `name_open()` + `MsgSend()` before the next job is dequeued. If the
  operator sends commands in a burst faster than the client thread can
  drain them (e.g. an unresponsive target causing `MsgSend()` to hang, or
  simply many broadcasts sent back-to-back), the queue can fill and
  `ipc_client_post()` returns `-1` — need to confirm the drop is handled
  safely (warning logged, no crash, operator thread never blocked since
  `ipc_client_post()` never blocks).
- **Related**: `ipc_client_post()` (`qnet_utils.c` lines 357-385,
  specifically the guard `q->count == IPC_CLIENT_QUEUE_CAPACITY` at line
  368), the `c_comm_send_*()`/`c_comm_broadcast_timing_profile()` functions
  (`c_comm.c`) all log `"... dropped - outgoing queue full or stopping"`
  when a post fails.
- **Environment**: (B) multiple nodes on one machine: `c_main` + all 6
  `lx_main` + 3 `rlx_main` (to have a full 9 real targets, and so
  `MsgSend()` can plausibly be slower if some targets don't respond in
  time — try shutting down 1-2 `lx_main` before the test to simulate an
  unresponsive target, making `name_open()`/`MsgSend()` inside
  `ipc_client_thread_main()` take longer than usual and backing up the
  queue).
- **Setup**: Don't start `lx_main` for L6, simulating a "hung"/nonexistent
  target (which makes `name_open()` fail fast — simulating true slowness
  would need a different setup, e.g. a target that exists but whose
  `ipc_server_run()` is blocked; if a "truly slow" scenario can't be built,
  it's still possible to check just the "send a burst of more than 16
  commands before the client thread can drain any" part by sending very
  fast).
- **Steps**:
  1. Type ≥ 17 `m`/`o`/`t` commands in rapid succession (a script sending
     input if possible, or as fast as possible by hand — each `t` command
     broadcasts creating 3 `ipc_client_post()` calls for a 3-controller
     chain, so just 6 `t` commands in a row is enough to exceed 16 jobs)
     within a few seconds, without waiting for responses between commands.
  2. Watch `central_log.txt`/stdout and count the
     `"... dropped - outgoing queue full or stopping"` lines.
- **Expected result**: When the queue fills, jobs beyond 16 must be dropped
  **with a clear log line** (never silently lost), the `c_main` process
  must not deadlock/crash, and the `c_operator_reader_thread` must continue
  accepting further commands immediately (never blocked waiting for the
  queue to drain, since `ipc_client_post()` returns `-1` right away instead
  of waiting). After the client thread drains the backlog, subsequent
  commands (once the queue is no longer full) must process normally again.
- **Note**: Genuinely filling the queue is hard on internal loopback since
  `MsgSend()` to a normally running target usually responds very fast
  (under a few ms) — the client thread drains faster than a human can type.
  Prefer environment (C) over a real network (higher Qnet latency) and/or
  shutting down some targets to force `name_open()`/`MsgSend()` into a
  timeout wait, increasing the odds of a queue backlog. If a genuinely full
  queue can't be achieved, lower the goal to "confirm no crash/no data loss
  under moderate load" and note clearly in the report that the full-queue
  branch wasn't proven. Repeat N ≥ 5 times.

### TC-RACE-20: Operator typing commands continuously while the server thread handles a burst of heartbeat/status reports
- **Type**: Edge case
- **Why it's a race**: `c_mode_eng_t` (the `ctx.mode_eng` struct in
  `c_main.c`) is written by **two different threads** under the same
  `mode_eng_lock`: (1) `c_main`'s server thread, inside
  `ipc_server_run()`'s loop handling `on_request()`
  (`MSG_STATUS_REPORT`/`MSG_HEARTBEAT`/`MSG_FAULT_REPORT`/
  `MSG_CROSSING_STATUS` → `c_server_record_*()`) and `on_pulse()`
  (`IPC_PULSE_HEARTBEAT_TICK` every 1s → `c_watchdog_mon_tick()` +
  `c_hmi_render()`); and (2) the `c_operator_reader_thread`
  (`handle_set_mode()`/`handle_timing_profile()`/`handle_request_override()`/
  `handle_renew_override()`/`handle_cancel_override()`, each locking/
  unlocking `mode_eng_lock` around reading/writing
  `controllers[idx].last_commanded_mode`/`override_in_flight`/
  `last_applied_profile_id`). With 9 controllers (6 Lx + 3 RLx) sending
  status/heartbeat in a near-continuous burst, the server thread holds
  `mode_eng_lock` very frequently (though each hold is very short) — if the
  operator types a command exactly while the lock is held, the operator's
  action must **wait** (normal mutex blocking) rather than read/write stale
  data (torn read) or bypass the lock.
- **Related**: `c_main.c` lines 37-51 (field + comment explaining why
  `mode_eng_lock` was added), lines 60-89 (on_request handlers), lines
  114-117 (on_pulse: `c_watchdog_mon_tick()` + `c_hmi_render()`),
  `c_operator.c` every `handle_*()` (lines 116-330, all calling
  `pthread_mutex_lock(args->mode_eng_lock)`).
- **Environment**: (B) or full (C): `c_main` + all 6 `lx_main` + 3
  `rlx_main` running and genuinely sending periodic status/heartbeat
  (not simulated) to create natural bursty load on the server thread.
- **Setup**: Ensure all 9 controllers are connected and sending
  status/heartbeat regularly (watch `c_hmi_render()` updating continuously
  on screen every second).
- **Steps**:
  1. While the system is running with all 9 controllers, type many
     different operator commands quickly and continuously: `m` (change mode
     L1), then immediately `o` (override L2), then `r` (renew L2 if the
     override was just accepted), then `t` (broadcast timing profile chain
     1), then `c` (cancel L2) — each command under 1 second apart, without
     waiting for ACK/NACK between commands.
  2. Repeat the above sequence continuously for about 30-60 seconds while
     all 9 controllers keep sending status/heartbeat/crossing_status
     (natural background load, no extra intervention needed).
  3. Watch the HMI (`c_hmi_render()` output) and `central_log.txt`
     throughout.
- **Expected result**:
  - No deadlock: both `c_operator_reader_thread` and the server thread must
    keep making progress throughout the 30-60 seconds (HMI keeps updating
    every second, operator keeps getting a response for each command).
  - No torn write: after stopping typing, reading back
    `mode_eng.controllers[]` (via HMI or log) must show fields
    (`last_commanded_mode`, `override_in_flight`, `last_applied_profile_id`)
    in a valid state consistent with the last command sent to each
    controller — no "garbage"/out-of-enum values (a sign of reading
    mid-write without a lock).
  - `c_watchdog_mon_tick()`'s missed-heartbeat bookkeeping stays accurate:
    no controller gets falsely flagged "missed heartbeat" while actually
    still sending heartbeats regularly, just because the lock was held long
    by the operator — confirm the lock is only held for a very short time
    in each `handle_*()` (no blocking I/O inside the locked region).
- **Note**: This is a "high-frequency, subtle-consequence" race (torn
  read/write) rather than a "hit-or-miss once" race — so run continuously
  as long as practical (recommend at least 5 continuous minutes, not just
  30-60 seconds) to increase the number of observed lock contentions, and
  repeat the whole scenario N ≥ 3 separate sessions (restarting the whole
  system between sessions) before concluding pass.

---

## Group F — Terminal/console output race (`c_main.c`, `c_operator.c`)

### TC-RACE-21 (Regression, fixed): DP-01/DP-02 auto peak-hour broadcast log spliced with operator console output
- **Type**: Regression (fixed)
- **Why it was a race**: `on_pulse()`'s `IPC_PULSE_HEARTBEAT_TICK` case
  (server thread) calls `c_mode_eng_auto_check()` to detect an automatic
  time-of-day mode change (DP-01/DP-02), then calls `c_logger_log()` with a
  notice line before calling `c_comm_broadcast_set_mode()` (unicast-sending
  `MSG_SET_MODE` to all 6 Lx). The previous patch only locked
  `console_io_lock` around ONE of the two operations (either just the log
  line, or not locking during the send) — while
  `c_operator_reader_thread` also holds `console_io_lock` throughout each
  `handle_*()`. If the DP-01/DP-02 hour boundary hit exactly while the
  operator was typing a command, the "Auto peak-hour switch..." log line
  could get spliced into the middle of the operator command's output
  (terminal splicing), or worse, `c_comm_send_set_mode()`'s "outgoing queue
  full" log path (called nested inside `c_comm_broadcast_set_mode()`) could
  run without `console_io_lock`, causing torn output.
- **Related**: `app/central/src/c_main.c : on_pulse()` lines 201-219 — the
  `console_io_lock` now wraps BOTH the `c_logger_log()` line AND the entire
  `c_comm_broadcast_set_mode()` call (not just the log line), because the
  inner send function (`c_comm_send_set_mode()`) can itself call
  `c_logger_log()` on its error branch, and every other call site of it
  already runs under `console_io_lock` via `c_operator.c`'s reader-thread
  switch.
- **Environment**: (B) or full (C): `c_main` + at least 1 `lx_main`
  running; needs control over `demo_hour`/`demo_hour_override_active`
  (keys `d`/`a`) to force the DP-01/DP-02 boundary to occur on cue instead
  of waiting for real time.
- **Setup**: `c_main` + `lx_main 1..6` running normally, currently at a
  `mode` different from the one it will be forced to switch to.
- **Steps**:
  1. On C1, use `d` to set `demo_hour` close to the peak/off-peak boundary
     (e.g. 1-2 ticks away from a mode change).
  2. Right as the boundary approaches (within under 1 second), type several
     other operator commands continuously (`m`, `t`, `?`) without stopping,
     to maximize the chance of overlapping with `on_pulse()`'s auto-switch
     broadcast.
  3. Repeat steps 1-2 about 10-20 times (each time setting `demo_hour` close
     to a different boundary) to increase the odds of catching the race.
  4. Watch the terminal output and `central_log.txt` throughout.
- **Expected result**: The line `Auto peak-hour switch: hour=... ->
  mode=..., broadcasting to all Lx` in `central_log.txt` is always intact,
  never interleaved with output from any `handle_*()` (no line cut off
  mid-way or two lines merged together) — even when the 6 unicast
  `MSG_SET_MODE` sends inside `c_comm_broadcast_set_mode()` hit the
  "outgoing queue full" branch and log extra lines themselves.
- **Note**: This is a naturally rare race (DP-01/DP-02 only occurs exactly
  at a configured hour boundary) — use `demo_hour`/`demo_hour_override_active`
  (`c_operator.c` keys `d`/`a`) to force-trigger it repeatedly instead of
  waiting for real time to pass.
