# 03 - Test Plan: IPC Protocol Contract

Scope of this document: test each **verb** in `msg_type_t`
(`app/shared/includes/ipc_msg.h`) and each possible **outcome** when
that verb is sent over Qnet (`RESULT_ACK` / `RESULT_ACK_PENDING` /
`RESULT_NACK` + a specific `nack_reason_t` / `RESULT_ERROR`), checked
directly against the real logic in `lx_fsm.c`, `rlx_fsm.c`,
`c_mode_eng.c`, `c_main.c`. This is NOT a functional/timing test plan
(see other files in `docs/test-plan/`) - the sole focus here is: "for
input X, does the (result, reason) pair returned on the wire match
what the code specifies?".

## Environment convention (A/B/C/D)

| Symbol | Meaning |
|---|---|
| **(A)** | Single node, no real Qnet needed (e.g. running only `c_main`/`lx_main` alone, observing internal behavior - rarely used for protocol tests since at least 2 sender/receiver sides are needed). |
| **(B)** | Multiple processes (nodes) running on **the same QNX machine**, each node a separate executable (`c_main`, `lx_main <n>`, `rlx_main <n>`), communicating over local Qnet (no `TRAFFIC_NODE_MAP` needed, defaults to "same node as caller" - see `qnet_utils.h`). Sufficient to verify protocol-contract correctness since `MsgSend/MsgReceive/MsgReply` still go through the real kernel. |
| **(C)** | Multiple different **physical or virtual QNX machines/VMs**, on a real network, using the `TRAFFIC_NODE_MAP` env var to resolve node names (see `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`). Used for tests confirming behavior is unchanged over a real network (latency, packet loss...). For pure protocol-contract tests, (B) and (C) give **identical result/reason** - this document uses (C) only when emphasizing the project's "must run on a real distributed environment" requirement. |
| **(D)** | **Requires a dedicated test_client tool (NOT YET in the repo - proposed to be built).** `c_operator.c` (keyboard at C1) and `lx_sensor.c`/`rlx_sensor.c` (keyboard at Lx/RLx) only accept form-valid input (they prompt for numbers via `scanf`, with no way to send a malformed payload, an unknown verb, or target the wrong node type). Some protocol-contract outcomes only occur with a message that "breaks the normal rules of play" - those cases require environment (D). |

### Proposed test_client tool (for every case marked (D))

Does not yet exist in the repo. Proposal: a small executable,
`app/tools/test_client/test_client.c`, reusing
`app/shared/includes/qnet_utils.h` / `qnet_utils.c` directly (already
has `ipc_attach_name()` to build the attach point name and the
`TRAFFIC_NODE_MAP` mechanism to open connections to remote nodes) and
`ipc_msg.h`. No need for `ipc_client_queue_t`/background threads like
the real nodes - just:

```c
name_open("/net/<node>/dev/name/global/traffic/<suffix>", 0) (or same-node)
ipc_request_t req; /* set every field by hand, including "invalid" fields */
MsgSend(coid, &req, sizeof(req), &reply, sizeof(reply));
print reply.result / reply.reason
```

Since `ipc_request_t`/`ipc_reply_t` and every verb/nack_reason constant
are already shared public headers, test_client only needs ~100 lines:
parse command-line args (target node, verb, numeric payload fields),
build the `ipc_request_t` union for the given verb, send once, print
the reply. This is the only "whitebox" tool that can produce: (1) a
formally valid payload with values outside any bound `c_operator.c`
allows entering, (2) a verb sent to the wrong node type, (3) 2 requests
sent almost simultaneously from 2 independent test_client processes to
probe races, (4) a deliberately malformed payload (a string without a
terminating `\0`, an out-of-range enum).

### Other general conventions

- Log to check: `central_log.txt` (written by `c_logger_log()`, also
  printed to the `c_main` process's stdout) in the exact format used by
  `c_comm.c`'s `on_command_reply()`:
  - NACK: `C1: <VERB> to <target> -> NACK reason=<REASON>`
  - Otherwise: `C1: <VERB> to <target> -> <ACK|ACK_PENDING|ERROR>`
  For Lx/RLx verbs sent to C1 on their own (STATUS/HEARTBEAT/FAULT_REPORT/
  CROSSING_STATUS), the corresponding log lines live in the
  `on_request()` branch in `c_main.c` (some branches currently log
  nothing beyond updating `c_mode_eng_t` - see the note in each test
  case).
- Lx: run `lx_main <1..6>`; RLx: run `rlx_main <1..3>`; C1: run
  `c_main`. Control keys: see `print_help()` in `c_operator.c` /
  `lx_sensor.c` / `rlx_sensor.c`.
- Railway-intersection adjacency map (`rlx_comm.c` `ADJACENCY[]`):
  RL1 adjacent to L1,L2; RL2 adjacent to L3,L4; RL3 adjacent to L5,L6.
- Key constants: `LX_CYCLE_LENGTH_MS = 90000` (48000+4000+2000+
  30000+4000+2000, `lx_timer.h`), `LX_OVERRIDE_DURATION_CAP_MS =
  300000`, chain offsets R1 = {L1:0, L3:21000, L5:45000}ms, R2 =
  {L2:0, L4:19000, L6:42000}ms (`c_mode_eng.h`), `RLX_GATE_MOTION_MS =
  3000`, `RLX_WARNING_TO_CLOSING_MS = 5000`,
  `RLX_CLOSING_DEADLINE_MS = 15000`, `RLX_OPENING_DEADLINE_MS = 15000`
  (`rlx_timer.h`/`rlx_gate.h`).
- **Important findings to know before testing (details in the Appendix
  at the end of the file):** (1) `lx_sensor.c` has no manual fault-trigger
  key (unlike `rlx_sensor.c`, which has `x`/`f`), so there is no way to
  force an Lx into `SUPERVISORY_FAULT_SAFE` from the keyboard alone -
  PA-10 only trips for real when the server thread actually hangs for
  >= 2s; (2) `MSG_STATUS` is handled by `c_main.c` but **nothing in the
  current code actually sends** this verb (`lx_comm.c`/`rlx_comm.c`
  only send HEARTBEAT/FAULT_REPORT/CROSSING_STATUS); (3) `RESULT_ACK`
  for `MSG_REQUEST_FAULT_CLEAR` appears **unreachable** with the current
  code because no path runs the gate-open command
  (`rlx_gate_command_open()`) while in `RLX_FAULT`; (4)
  `NACK_REASON_PEDESTRIAN_ACTIVE` is declared and has a log name but
  **is never assigned** anywhere in `lx_fsm.c` (the ped-clearance case
  uses `ACK_PENDING`, not this NACK) - see Appendix.

---

## 1. MSG_SET_TIMING_PROFILE (C1 -> Lx)

Handled by `lx_fsm_on_set_timing_profile()`. Only 2 NACK branches:
`NACK_REASON_FAULT_ACTIVE` (in FAULT_SAFE) and
`NACK_REASON_STALE_OR_UNSAFE_PROFILE` (`offset_ms >= LX_CYCLE_LENGTH_MS`
= 90000). `c_operator.c`'s `t` key only sends fixed offsets from
`R1_CHAIN`/`R2_CHAIN` (0/21000/45000/19000/42000ms) - none >= 90000, so
the boundary case must use test_client.

### TC-MSG-1: Valid SET_TIMING_PROFILE - ACK
- **Type**: Positive
- **Verb**: MSG_SET_TIMING_PROFILE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1 (+ L3, L5 same R1 chain, not required but recommended to confirm the broadcast has no errors)
- **Setup**: Start `c_main`, `lx_main 1`, `lx_main 3`, `lx_main 5`.
- **Steps**: At C1: press `t` -> `chain (1=R1 L1/L3/L5, 2=R2 L2/L4/L6): 1`.
- **Expected Result**: `central_log.txt` has 3 lines `C1: SET_TIMING_PROFILE to <id> -> ACK` (id = 1, 3, 5 - CTRL_L1/L3/L5). At L1: `active_profile_id` updated to the assigned profile_id (printed in internal log if available), `offset_apply_pending=1` (observed indirectly: green-wave shifts by the correct offset at the next arterial-green cycle).

### TC-MSG-2: SET_TIMING_PROFILE while Lx is FAULT_SAFE - NACK FAULT_ACTIVE
- **Type**: Negative
- **Verb**: MSG_SET_TIMING_PROFILE
- **Related**: NACK_REASON_FAULT_ACTIVE
- **Environment**: (D) requires a helper tool to force a fault. `lx_sensor.c` currently has NO manual fault key (unlike `rlx_sensor.c`'s `x`/`f`), and PA-10 only trips for real when the Lx server thread stops ticking for >= 2s straight (`lx_watchdog.c`, `LX_WATCHDOG_CHECK_INTERVAL_S=2`) - no deterministic way to trigger it via normal keyboard/CLI. Proposal: add a temporary DEMO-ONLY key in `lx_sensor.c` that calls `lx_fsm_report_watchdog_trip(&fsm)` directly, following the pattern already used by `rlx_sensor.c`'s `f` key, so testers can force `SUPERVISORY_FAULT_SAFE` without waiting for a real watchdog trip.
- **Setup**: Once the demo key exists (or attach a debugger to pause `lx_main 1`'s server thread with a breakpoint > 2s), confirm via the log "Lx: WATCHDOG - no phase-timer activity ... reporting fault".
- **Steps**: From C1: `t` -> `1` (broadcast R1) while L1 is in FAULT_SAFE.
- **Expected Result**: `central_log.txt`: `C1: SET_TIMING_PROFILE to 1 -> NACK reason=FAULT_ACTIVE`.

### TC-MSG-3: offset_ms exactly at the upper boundary (>= LX_CYCLE_LENGTH_MS) - NACK STALE_OR_UNSAFE_PROFILE (edge case)
- **Type**: Edge case
- **Verb**: MSG_SET_TIMING_PROFILE
- **Related**: NACK_REASON_STALE_OR_UNSAFE_PROFILE; boundary `offset_ms >= 90000` (`lx_fsm_on_set_timing_profile()`)
- **Environment**: (D) - `c_operator.c` does not allow entering an arbitrary offset, only the fixed constants in `c_mode_eng.c`.
- **Setup**: `c_main`, `lx_main 1` already running, L1 in NORMAL_OPERATION (no fault).
- **Steps**: test_client sends directly to L1 an `ipc_request_t{verb=MSG_SET_TIMING_PROFILE, sender_id=CTRL_C1, target_id=CTRL_L1, payload.timing_profile={profile_id=99, offset_ms=90000}}`.
- **Expected Result**: reply `result=RESULT_NACK`, `reason=NACK_REASON_STALE_OR_UNSAFE_PROFILE`. L1's `active_profile_id`/`assigned_offset_ms` UNCHANGED.

### TC-MSG-4: offset_ms just below the boundary (LX_CYCLE_LENGTH_MS - 1) - ACK (edge case)
- **Type**: Edge case
- **Verb**: MSG_SET_TIMING_PROFILE
- **Related**: safe boundary just below NACK_REASON_STALE_OR_UNSAFE_PROFILE
- **Environment**: (D)
- **Setup**: Same as TC-MSG-3.
- **Steps**: test_client sends `payload.timing_profile={profile_id=100, offset_ms=89999}` to L1.
- **Expected Result**: reply `result=RESULT_ACK`. `active_profile_id=100`, `assigned_offset_ms=89999`, `offset_apply_pending=1` (applied at next arterial-green).

---

## 2. MSG_SET_MODE (C1 -> Lx)

Handled by `lx_fsm_on_set_mode()`. 2 NACK branches
(`NACK_REASON_FAULT_ACTIVE`, and - re-audit fix, see TC-MSG-8b -
`NACK_REASON_OUT_OF_RANGE` when `payload->mode` is neither 0
(`MODE_PEAK_FIXED`) nor 1 (`MODE_OFF_PEAK_SENSOR`), per UC-07 main flow
step 3 "validates the request against supported ranges"); 2 positive
branches: `RESULT_ACK` (sent mode == current mode, treated as a no-op)
and `RESULT_ACK_PENDING` (different mode, deferred to the next
ALL_RED boundary - SC-01A).

### TC-MSG-5: SET_MODE with mode matching current mode - ACK (no-op)
- **Type**: Positive
- **Verb**: MSG_SET_MODE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1
- **Setup**: L1 starts in `MODE_PEAK_FIXED` (default cold-start, `lx_fsm_init()`).
- **Steps**: At C1: `m` -> `Lx number: 1` -> `mode: 0` (PEAK_FIXED).
- **Expected Result**: `central_log.txt`: `C1: SET_MODE to 1 -> ACK`. `mode_change_pending` at L1 remains 0 (nothing deferred).

### TC-MSG-6: SET_MODE to a different mode - ACK_PENDING, applied correctly at the ALL_RED boundary
- **Type**: Positive
- **Verb**: MSG_SET_MODE
- **Related**: (no nack) - SC-01A
- **Environment**: (B) C1 + L1
- **Setup**: L1 currently `MODE_PEAK_FIXED`, mid-`PHASE_ARTERIAL_GREEN` or any phase not yet at ALL_RED.
- **Steps**: At C1: `m` -> `1` -> `mode: 1` (OFF_PEAK_SENSOR). Watch L1's log/console for up to 1 cycle (<= ~54s to the nearest `PHASE_ALL_RED_A_TO_B` or `PHASE_ALL_RED_B_TO_A`).
- **Expected Result**: Immediately: `central_log.txt`: `C1: SET_MODE to 1 -> ACK_PENDING`. Then, exactly at the next entry into ALL_RED (not sooner, without interrupting the running phase), `fsm->mode` becomes `MODE_OFF_PEAK_SENSOR` (observed via the 4s extension behavior instead of fixed-duration at the next `PHASE_ARTERIAL_GREEN`/`PHASE_CONNECTOR_GREEN`).

### TC-MSG-7: Cancel a pending mode-change by resending the current mode before the boundary (edge case)
- **Type**: Edge case
- **Verb**: MSG_SET_MODE
- **Related**: `mode_change_pending=0` behavior when payload->mode == current fsm->mode (`lx_fsm_on_set_mode()`)
- **Environment**: (B) C1 + L1
- **Setup**: L1 currently `MODE_PEAK_FIXED`.
- **Steps**: (1) `m` -> `1` -> `1` (request OFF_PEAK_SENSOR) -> receive ACK_PENDING. (2) Before L1 reaches the next ALL_RED boundary, send `m` -> `1` -> `0` (request PEAK_FIXED again, i.e. the actual current mode).
- **Expected Result**: Step (2) returns `result=RESULT_ACK` (not ACK_PENDING, since `payload->mode == fsm->mode` currently). `mode_change_pending` reset to 0 - at the next ALL_RED boundary L1 does NOT change mode (stays PEAK_FIXED), confirming request (1) was fully canceled rather than silently applied.

### TC-MSG-8: SET_MODE while Lx is FAULT_SAFE - NACK FAULT_ACTIVE
- **Type**: Negative
- **Verb**: MSG_SET_MODE
- **Related**: NACK_REASON_FAULT_ACTIVE
- **Environment**: (D) - same reason/proposal as TC-MSG-2 (needs a demo key to force fault on `lx_sensor.c`, not yet present).
- **Setup**: L1 in `SUPERVISORY_FAULT_SAFE`.
- **Steps**: At C1: `m` -> `1` -> `1`.
- **Expected Result**: `central_log.txt`: `C1: SET_MODE to 1 -> NACK reason=FAULT_ACTIVE`.

### TC-MSG-8b: SET_MODE with `mode` out of valid range (other than 0/1) - NACK OUT_OF_RANGE (UC-07 step 3)
- **Type**: Negative (re-audit fix)
- **Verb**: MSG_SET_MODE
- **Related**: `NACK_REASON_OUT_OF_RANGE`, UC-07 main flow step 3 ("validates
  the request against supported ranges"). `c_operator.c`'s `handle_set_mode()`
  already blocks values other than 0/1 at the console (`n != MODE_PEAK_FIXED &&
  n != MODE_OFF_PEAK_SENSOR` -> "command aborted", nothing sent) - so this
  NACK branch **cannot be reproduced via the `c_operator` keyboard**, only
  via test_client sending a raw invalid payload directly, matching the
  comment in `lx_fsm_on_set_mode()`: "this FSM (not the console) is the
  documented authoritative validator - the wire contract has no guarantee
  the sender is always a well-behaved operator".
- **Environment**: (D) - required, since `c_operator.c`'s pre-check blocks
  this exact threshold before sending.
- **Setup**: L1 has no fault, currently `MODE_PEAK_FIXED`.
- **Steps**: test_client sends directly to L1 an
  `ipc_request_t{verb=MSG_SET_MODE, sender_id=CTRL_C1, target_id=CTRL_L1,
  payload.mode={mode=2}}` (any value other than 0/1).
- **Expected Result**: reply `result=RESULT_NACK`,
  `reason=NACK_REASON_OUT_OF_RANGE`; `fsm->mode`/`fsm->mode_change_pending`
  unchanged (request fully rejected, not silently falling into the old
  `else` branch as before the re-audit fix).

---

## 3. MSG_REQUEST_OVERRIDE (C1 -> Lx)

Handled in 2 layers: (1) `c_mode_eng_validate_override_request()` at
**Central** (formal pre-check: valid target, `duration_ms` in (0, 300000],
`override_type == OVERRIDE_CLEAR_ROUTE`) - if it fails, the request
**never reaches the wire** (`c_operator.c`'s `handle_request_override()`
only logs locally at C1, no real `ipc_reply_t` from Lx); (2)
`lx_fsm_on_request_override()` at **Lx** - a deeper protection layer,
checked in this exact order in code: another CENTRAL_OVERRIDE already
running -> `NACK_REASON_OUT_OF_RANGE`; `duration_ms==0` or `>300000` ->
`NACK_REASON_INVALID_DURATION`; currently `RAILWAY_PREEMPTION` ->
`NACK_REASON_RAILWAY_CONFLICT`; currently `FAULT_SAFE` ->
`NACK_REASON_FAULT_ACTIVE`; `ped_clearance_active` ->
`RESULT_ACK_PENDING` (NOT a NACK - see Appendix on
`NACK_REASON_PEDESTRIAN_ACTIVE`); otherwise -> `RESULT_ACK`.

Since Central's duration bound (0, 300000] is identical to Lx's,
**`c_operator.c` can never actually reach the
`NACK_REASON_INVALID_DURATION` branch that truly lives in
`lx_fsm.c`** - any duration typed outside (0,300000] is already
blocked by Central before sending. The Lx-side test case must use
test_client sending directly, bypassing Central.

### TC-MSG-9: Valid REQUEST_OVERRIDE, no conflict - ACK
- **Type**: Positive
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1
- **Setup**: L1 in NORMAL_OPERATION, no fault, no railway preemption, no ped clearance running.
- **Steps**: C1: `o` -> `Lx number: 1` -> `target movement: 0` (arterial) -> `duration_ms: 30000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK`. L1: `supervisory=SUPERVISORY_CENTRAL_OVERRIDE`, `override_substate=OVR_ACTIVE`, holds green on ARTERIAL movement for 30s then ends itself (safe clearance).

### TC-MSG-10: REQUEST_OVERRIDE while ped clearance is running - ACK_PENDING (SC-03B)
- **Type**: Positive
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: (no nack - this is exactly the case the code comment suggests "NACK_REASON_PEDESTRIAN_ACTIVE" for, but in practice returns ACK_PENDING, see Appendix)
- **Environment**: (B) C1 + L1
- **Setup**: At L1's sensor console (`lx_sensor_reader_thread`), while `PHASE_ARTERIAL_GREEN` is running, press `1` (ped side 0) to start the WALK/FLASHING_DONT_WALK sequence (total 10s: 6s WALK + 4s FDW).
- **Steps**: While the WALK/FDW sequence is running (within that 10s), at C1: `o` -> `1` -> `target movement: 0` (arterial, same side being served) -> `duration_ms: 20000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK_PENDING`. L1: `override_substate=OVR_PENDING_CLEARANCE`, `supervisory=SUPERVISORY_CENTRAL_OVERRIDE` immediately, but the override only actually holds green (OVR_ACTIVE) AFTER the WALK/FDW sequence completes (`ped_clearance_active` returns to 0) - no second reply is sent when the override actually activates.

### TC-MSG-11: REQUEST_OVERRIDE while another override is already running - NACK OUT_OF_RANGE
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_OUT_OF_RANGE (used as a stand-in since "no dedicated error code exists for this case yet" - comment in `lx_fsm_on_request_override()`)
- **Environment**: (B) C1 + L1
- **Setup**: Run TC-MSG-9 first (L1 has an ACTIVE, unexpired override).
- **Steps**: While the first override is still in effect, send: C1: `o` -> `1` -> `target movement: 1` -> `duration_ms: 10000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> NACK reason=OUT_OF_RANGE`. The first override (arterial, 30s) keeps running unchanged.

### TC-MSG-12: REQUEST_OVERRIDE duration_ms=0 - NACK INVALID_DURATION (blocked at Central pre-check)
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_INVALID_DURATION (`c_mode_eng_validate_override_request()`, NOT a real reply from Lx)
- **Environment**: (B) C1 + L1
- **Setup**: L1 running normally.
- **Steps**: C1: `o` -> `1` -> `target movement: 0` -> `duration_ms: 0`.
- **Expected Result**: `central_log.txt`: `Operator: REQUEST_OVERRIDE(target=1, movement=0, duration_ms=0) rejected by Central pre-check, reason=INVALID_DURATION - not forwarded to the controller`. **NO** `C1: REQUEST_OVERRIDE to 1 -> ...` line (request never left C1 - `ipc_client_post()` is never called).

### TC-MSG-13: REQUEST_OVERRIDE duration_ms=300001 - NACK INVALID_DURATION (blocked at Central pre-check)
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_INVALID_DURATION (Central pre-check)
- **Environment**: (B) C1 + L1
- **Setup**: Same as TC-MSG-12.
- **Steps**: C1: `o` -> `1` -> `0` -> `duration_ms: 300001`.
- **Expected Result**: Same as TC-MSG-12 but with `duration_ms=300001` in the log; nothing sent to L1.

### TC-MSG-14: REQUEST_OVERRIDE duration_ms=0 sent directly to Lx, bypassing Central - NACK INVALID_DURATION (real Lx-side path)
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_INVALID_DURATION (the REAL branch in `lx_fsm_on_request_override()`, not via `c_operator.c`)
- **Environment**: (D) - required, since `c_operator.c`'s pre-check blocks this exact threshold before sending (see intro to section 3).
- **Setup**: L1 running normally, no override active.
- **Steps**: test_client sends directly to L1: `ipc_request_t{verb=MSG_REQUEST_OVERRIDE, sender_id=CTRL_C1, target_id=CTRL_L1, payload.override_request={override_type=OVERRIDE_CLEAR_ROUTE, target_movement=0, duration_ms=0}}`.
- **Expected Result**: reply `result=RESULT_NACK`, `reason=NACK_REASON_INVALID_DURATION`. This is independent proof that Lx has its own defense-in-depth validation, not just relying on Central.

### TC-MSG-15: REQUEST_OVERRIDE while Lx is in RAILWAY_PREEMPTION - NACK RAILWAY_CONFLICT
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_RAILWAY_CONFLICT
- **Environment**: (B)/(C) C1 + L1 + RL1 (L1 adjacent to RL1 per `ADJACENCY[]`)
- **Setup**: Start `rlx_main 1`. At RL1's sensor console, press `0` (TRAIN_APPROACHING direction 0) so RL1 transitions OPEN -> WARNING, triggering a `MSG_CROSSING_STATUS(WARNING)` broadcast to L1 (and L2, C1). Confirm L1 has entered `SUPERVISORY_RAILWAY_PREEMPTION`.
- **Steps**: C1: `o` -> `Lx number: 1` -> `target movement: 1` (connector - direction blocked by the railway) -> `duration_ms: 10000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> NACK reason=RAILWAY_CONFLICT`.

### TC-MSG-16: REQUEST_OVERRIDE while Lx is FAULT_SAFE - NACK FAULT_ACTIVE
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_FAULT_ACTIVE
- **Environment**: (D) - same reason as TC-MSG-2 (needs a demo key to force fault, not yet in `lx_sensor.c`).
- **Setup**: L1 in `SUPERVISORY_FAULT_SAFE`.
- **Steps**: C1: `o` -> `1` -> `0` -> `duration_ms: 10000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> NACK reason=FAULT_ACTIVE`.

### TC-MSG-17: duration_ms=1 - ACK (valid lower boundary, PA-11)
- **Type**: Edge case
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: PA-11, boundary `(0, 300000]`
- **Environment**: (B) C1 + L1
- **Setup**: L1 normal, no override/fault/preemption.
- **Steps**: C1: `o` -> `1` -> `target movement: 0` -> `duration_ms: 1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK`. Override ends almost immediately at the next 100ms tick of `lx_fsm_on_phase_timer()` (since `override_remaining_ms=1 <= LX_PHASE_TICK_MS=100`).

### TC-MSG-18: duration_ms=300000 - ACK (valid upper boundary, per PA-11)
- **Type**: Edge case
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: PA-11, upper boundary `LX_OVERRIDE_DURATION_CAP_MS = 300000`
- **Environment**: (B) C1 + L1
- **Setup**: Same as TC-MSG-17.
- **Steps**: C1: `o` -> `1` -> `0` -> `duration_ms: 300000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK` (300000 accepted, 300001 already NACKed in TC-MSG-13/equivalent at the Lx layer). `override_duration_ms=300000` at L1.

---

## 4. MSG_RENEW_OVERRIDE (C1 -> Lx)

Handled by `lx_fsm_on_renew_override()`. ACK condition: must currently
be `SUPERVISORY_CENTRAL_OVERRIDE` **and** `override_substate ==
OVR_ACTIVE` (note: `OVR_PENDING_CLEARANCE` does NOT qualify - still
NACKs `UNKNOWN_TARGET`). Second NACK: `extend_duration_ms > 300000` ->
`INVALID_DURATION`. `extend_duration_ms=0` means "renew back to exactly
the original duration" (`override_duration_ms`, not 0ms).

### TC-MSG-19: Valid RENEW_OVERRIDE on an ACTIVE override - ACK
- **Type**: Positive
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1
- **Setup**: Run TC-MSG-9 (override ACTIVE, 30000ms, still in effect).
- **Steps**: C1: `r` -> `Lx number: 1` -> `extend_duration_ms: 60000`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. L1's `override_duration_ms` and `override_remaining_ms` = 60000 (counted fresh, not added to elapsed time).

### TC-MSG-20: RENEW_OVERRIDE when no override exists - NACK UNKNOWN_TARGET
- **Type**: Negative
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: NACK_REASON_UNKNOWN_TARGET
- **Environment**: (B) C1 + L1
- **Setup**: L1 in NORMAL_OPERATION, no override ever sent.
- **Steps**: C1: `r` -> `1` -> `extend_duration_ms: 5000` (operator prints a warning "Central has no override recorded in flight" but still sends it - per BR-7 design).
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> NACK reason=UNKNOWN_TARGET`.

### TC-MSG-21: RENEW_OVERRIDE on an override still OVR_PENDING_CLEARANCE (not yet active) - NACK UNKNOWN_TARGET (edge case)
- **Type**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: NACK_REASON_UNKNOWN_TARGET; the exact condition `override_substate != OVR_ACTIVE` (includes PENDING_CLEARANCE) in `lx_fsm_on_renew_override()`
- **Environment**: (B) C1 + L1
- **Setup**: Reproduce TC-MSG-10 (override in `OVR_PENDING_CLEARANCE`, waiting for ped clearance to finish).
- **Steps**: While still PENDING_CLEARANCE (not yet ACTIVE), immediately send: C1: `r` -> `1` -> `extend_duration_ms: 0`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> NACK reason=UNKNOWN_TARGET` (even though the override business-logically "exists", the code only treats renew as valid once truly ACTIVE). The original PENDING_CLEARANCE override is unaffected by this rejected renew.

### TC-MSG-22: RENEW_OVERRIDE extend_duration_ms=300001 - NACK INVALID_DURATION
- **Type**: Negative
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: NACK_REASON_INVALID_DURATION
- **Environment**: (B) C1 + L1
- **Setup**: Override currently ACTIVE (TC-MSG-9).
- **Steps**: C1: `r` -> `1` -> `extend_duration_ms: 300001`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> NACK reason=INVALID_DURATION`. Current override expiry unchanged (code comment: "Retain the current expiry unchanged on rejection").

### TC-MSG-23: extend_duration_ms=0 - ACK, renews to exactly the ORIGINAL duration (edge case)
- **Type**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: special semantics of value 0 (`renew_override_payload_t.extend_duration_ms`)
- **Environment**: (B) C1 + L1
- **Setup**: Create an initial override with `duration_ms=20000` (TC-MSG-9 style, using 20000 instead of 30000). Wait a few seconds for `override_remaining_ms` to drop below 20000 (e.g. to ~15000ms).
- **Steps**: C1: `r` -> `1` -> `extend_duration_ms: 0`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_remaining_ms` reset to 20000 (the ORIGINAL `override_duration_ms` value, not added to the remaining ~15000, and not set to 0).

### TC-MSG-24: extend_duration_ms=300000 - ACK (valid upper boundary)
- **Type**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: upper boundary `LX_OVERRIDE_DURATION_CAP_MS`
- **Environment**: (B) C1 + L1
- **Setup**: Override currently ACTIVE.
- **Steps**: C1: `r` -> `1` -> `extend_duration_ms: 300000`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_duration_ms=override_remaining_ms=300000`.

### TC-MSG-25: extend_duration_ms=1 - ACK (lower boundary, smallest real extension >0)
- **Type**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: distinguishes from the "0 = keep unchanged" case (TC-MSG-23) - this is an explicit 1ms extension
- **Environment**: (B) C1 + L1
- **Setup**: Override currently ACTIVE.
- **Steps**: C1: `r` -> `1` -> `extend_duration_ms: 1`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_duration_ms=override_remaining_ms=1` (not 0 - this is a real 1ms, not "keep original"), override ends at the very next tick.

---

## 5. MSG_CANCEL_OVERRIDE (C1 -> Lx)

Handled by `lx_fsm_on_cancel_override()`. No payload on the wire. ACK
if `override_substate` is `OVR_ACTIVE` **or** `OVR_PENDING_CLEARANCE`
(both canceled via `lx_fsm_terminate_override_locked()`); NACK
`UNKNOWN_TARGET` if `OVR_NONE`.

### TC-MSG-26: CANCEL_OVERRIDE on an ACTIVE override - ACK
- **Type**: Positive
- **Verb**: MSG_CANCEL_OVERRIDE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1
- **Setup**: Override ACTIVE (TC-MSG-9, long duration, e.g. 60000ms to leave time to operate).
- **Steps**: C1: `c` -> `Lx number: 1`.
- **Expected Result**: `central_log.txt`: `C1: CANCEL_OVERRIDE to 1 -> ACK`. L1: `override_substate=OVR_NONE`, `supervisory` returns to `SUPERVISORY_NORMAL_OPERATION` immediately (safely - via the "safe clearance" placeholder).

### TC-MSG-27: CANCEL_OVERRIDE on an override still OVR_PENDING_CLEARANCE (not yet active) - ACK (edge case)
- **Type**: Edge case
- **Verb**: MSG_CANCEL_OVERRIDE
- **Related**: the `override_substate == OVR_PENDING_CLEARANCE` branch is also accepted for cancellation (unlike RENEW_OVERRIDE in TC-MSG-21, which rejects this case)
- **Environment**: (B) C1 + L1
- **Setup**: Reproduce TC-MSG-10 (override PENDING_CLEARANCE, waiting on ped clearance).
- **Steps**: While still PENDING_CLEARANCE, immediately send C1: `c` -> `1`.
- **Expected Result**: `central_log.txt`: `C1: CANCEL_OVERRIDE to 1 -> ACK`. Override fully canceled despite never having been active; when ped clearance ends, NO override activates (compare with TC-MSG-10 if not canceled).

### TC-MSG-28: CANCEL_OVERRIDE with no override present - NACK UNKNOWN_TARGET
- **Type**: Negative
- **Verb**: MSG_CANCEL_OVERRIDE
- **Related**: NACK_REASON_UNKNOWN_TARGET
- **Environment**: (B) C1 + L1
- **Setup**: L1 in NORMAL_OPERATION, no override.
- **Steps**: C1: `c` -> `1`.
- **Expected Result**: `central_log.txt`: `C1: CANCEL_OVERRIDE to 1 -> NACK reason=UNKNOWN_TARGET`.

### TC-MSG-29: 2 near-simultaneous CANCEL_OVERRIDE to the same Lx (fast duplicate / race)
- **Type**: Edge case
- **Verb**: MSG_CANCEL_OVERRIDE
- **Related**: idempotency and serialization under `fsm->lock` when 2 requests for the same target arrive nearly at once
- **Environment**: (D) - **required**. `c_operator.c` reads the keyboard sequentially on 1 thread (`c_operator_reader_thread`), so 2 `c` presses are always at least 1 `scanf` cycle apart - a single C1 process cannot produce 2 truly simultaneous CANCEL_OVERRIDE messages. Requires 2 independent test_client processes, each opening its own connection and calling `MsgSend()` at nearly the same instant (e.g. synchronized via a named semaphore/barrier) targeting the same L1.
- **Setup**: Override currently ACTIVE at L1.
- **Steps**: 2 test_client processes A and B, each sending `MSG_CANCEL_OVERRIDE{target_id=CTRL_L1}` within the same few-millisecond window (busy-wait to a shared time point, or fire back-to-back without waiting for a reply if test_client supports async sends).
- **Expected Result**: Since `ipc_server_run()` processes each `MsgReceive()` sequentially on 1 channel (no 2 server threads running in parallel at L1), exactly 1 of the 2 requests gets `RESULT_ACK` (the one that arrives first), the other gets `RESULT_NACK`/`NACK_REASON_UNKNOWN_TARGET` (override already canceled by the other) - no inconsistent state, no crash, no double-free/double-terminate of the override.

---

## 6. MSG_REQUEST_FAULT_CLEAR (C1 -> RLx / Lx)

Handled by `rlx_fsm_on_fault_clear()` at RLx. `NACK_REASON_UNKNOWN_TARGET` if
`state != RLX_FAULT`; if currently `RLX_FAULT`: `RESULT_ACK` if
`rlx_gate_poll_open()==1`, otherwise `NACK_REASON_FAULT_ACTIVE`.

**Update (test-plan finding now fixed)**: this verb was originally documented
as C1->RLx only; `lx_fsm_local_fault_clear()` existed on the Lx side but no
verb/case called into it, so sending `MSG_REQUEST_FAULT_CLEAR` to an Lx used to
return `RESULT_ERROR` (see old TC-MSG-33). This has now been wired up:
`lx_main.c`'s `on_request()` now has a `MSG_REQUEST_FAULT_CLEAR` case calling
`lx_fsm_on_request_fault_clear()` (`lx_fsm.h`/`lx_fsm.c`), and `c_operator.c`'s
`handle_request_fault_clear()` asks for `node type` (0=Lx, 1=RLx) before the
node number, so the `f` key at C1 can now target either node type.
TC-MSG-33 below reflects the current behavior instead of the old `RESULT_ERROR`.

**Important finding**: a close read of `enter_fault()` (`rlx_fsm.c`) shows
every path into `RLX_FAULT` calls `rlx_gate_command_close()` (never
`rlx_gate_command_open()`), and no other code calls a gate-open command
while `state == RLX_FAULT`. So `rlx_gate_poll_open()` **cannot** become 1
while in `RLX_FAULT` with the current code -> the `RESULT_ACK` branch of
this verb appears **unreproducible** by any sequence of operations on the
current build (see TC-MSG-32).

### TC-MSG-30: REQUEST_FAULT_CLEAR when RLx is not in FAULT state - NACK UNKNOWN_TARGET
- **Type**: Negative
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: NACK_REASON_UNKNOWN_TARGET
- **Environment**: (B) C1 + RL1
- **Setup**: RL1 in `RLX_OPEN` (default at startup, no train, no fault).
- **Steps**: C1: `f` -> `RLx number (1-3): 1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 7 -> NACK reason=UNKNOWN_TARGET` (target_id printed is `CTRL_RL1`'s numeric value, =7 per `controller_id_t`).

### TC-MSG-31: REQUEST_FAULT_CLEAR while RLx is FAULT but gate not confirmed open - NACK FAULT_ACTIVE
- **Type**: Negative
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: NACK_REASON_FAULT_ACTIVE
- **Environment**: (B)/(C) C1 + RL1
- **Setup**: At RL1's sensor console: press `x` (arm demo gate-fail) RIGHT BEFORE pressing `0` (TRAIN_APPROACHING direction 0). Wait `RLX_WARNING_TO_CLOSING_MS (5s)` for RL1 to enter CLOSING, then wait `RLX_CLOSING_DEADLINE_MS (15s from entering CLOSING)` for `check_closing_or_reclosing_complete()` to detect the gate never confirmed closed and call `enter_fault(FAULT_GATE_CONFIRM_MISSING)` (~20s total). Confirm via RLx log: gate "FAILED TO CONFIRM" then state -> FAULT.
- **Steps**: C1: `f` -> `1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 7 -> NACK reason=FAULT_ACTIVE` (because `enter_fault()` re-issued a close command - this time not gate-fail-armed, so it confirms CLOSED 3s later, not OPEN - `gates_confirmed_open()` still = 0).

### TC-MSG-32: REQUEST_FAULT_CLEAR -> ACK (the positive case) - CURRENTLY UNREPRODUCIBLE, requires a code fix or extra tooling
- **Type**: Positive (BLOCKED - design/implementation gap found)
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: the "ACK" path of `rlx_fsm_on_fault_clear()` (condition: `state==RLX_FAULT` and `rlx_gate_poll_open()==1`)
- **Environment**: (D) - needs a tool OR a temporary patch, not just an ordinary test_client (see explanation).
- **Setup/Explanation**: With the current code, **no execution path** ever sets `g_confirmed_open=1` while `fsm->state == RLX_FAULT`: `enter_fault()` (called from every path leading to FAULT - deadline miss during CLOSING/RECLOSING/OPENING, or watchdog trip) always calls `rlx_gate_command_close()`, never `rlx_gate_command_open()`; and `rlx_fsm_on_tick()`'s `RLX_FAULT` case is a no-op (issues nothing further). Since test_client would call the same `rlx_fsm_on_fault_clear()` against the same internal state, sending the request over the wire doesn't help - this is a limitation at the FSM/gate-simulator layer, not the IPC protocol layer.
- **Proposed fix to make this case feasible**: add a DEMO-ONLY key to `rlx_sensor.c` (same pattern as `x`/`f`) simulating "technician finished repairs and manually confirmed the gate open", calling `rlx_gate_command_open()` directly (or setting the internal flag directly) while in FAULT, then waiting `RLX_GATE_MOTION_MS=3000ms` for `rlx_gate_poll_open()` to return 1.
- **Steps (after the patch)**: Enter FAULT as in TC-MSG-31 -> press the new DEMO-ONLY key to open the gate -> wait 3s -> C1: `f` -> `1`.
- **Expected Result (after the patch)**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 7 -> ACK`; RL1 returns to `RLX_OPEN`, `faults=FAULT_NONE`, occupancy windows cleared.

### TC-MSG-33: REQUEST_FAULT_CLEAR sent to an Lx in FAULT_SAFE - ACK (fixed, no longer RESULT_ERROR)
- **Type**: Positive (formerly an Edge case/Negative "RESULT_ERROR" - that behavior is now obsolete, see section 6's "Update")
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: `lx_fsm_on_request_fault_clear()` (`lx_fsm.c`) - now has its own case in `lx_main.c`'s `on_request()`, no longer falling into `default:`. `c_operator.c`'s `f` key asks for node type (0=Lx, 1=RLx) via `handle_request_fault_clear()`, so L1 can now be targeted directly from the console without test_client.
- **Environment**: (B) C1 + L1 is enough (no longer requires (D)/test_client for this basic case).
- **Setup**: Put L1 into `SUPERVISORY_FAULT_SAFE` (e.g. via watchdog trip, see TC-SC03A-6 Part 1 of `02-state-machine-transition.md`), and make sure `last_crossing_state==CROSSING_OPEN` (no adjacent RLx pre-empting) so resume correctly goes to `NORMAL_OPERATION`.
- **Steps**: At C1: `f` -> node type `0` (Lx) -> Lx number `1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`. `fsm->faults` back to `FAULT_NONE`, L1's SUPERVISORY leaves `FAULT_SAFE` for `NORMAL_OPERATION` (`3`). This function has no NACK branch (unconditional/idempotent, unlike `rlx_fsm_on_fault_clear()` - no physical state at Lx needs re-verification).

### TC-MSG-33b: REQUEST_FAULT_CLEAR to an Lx in FAULT_SAFE while the adjacent crossing is still closed - resumes RAILWAY_PREEMPTION, not NORMAL_OPERATION (re-audit fix, safety)
- **Type**: Positive (safety-relevant regression case)
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: `last_crossing_state` (`lx_fsm.h`) - re-audit fix: a fault-clear occurring while the adjacent crossing is not yet `OPEN` must not silently drop the existing suppression, or it could allow green toward a crossing that's still closed.
- **Environment**: (B) C1 + L1 + RL1.
- **Setup**: Put L1 into real `RAILWAY_PREEMPTION` (RL1 in WARNING/CLOSED, sending a `CROSSING_STATUS` other than `CROSSING_OPEN` to L1), then trip the watchdog so L1 enters `FAULT_SAFE` while still pre-empting (SUPERVISORY jumps straight `1 -> 0`, `fsm->last_crossing_state` keeps its latest non-OPEN value since `lx_fsm_on_crossing_status()` updates this field unconditionally, even while FAULT_SAFE).
- **Steps**: At C1: `f` -> `0` (Lx) -> `1`, **before** RL1 reports `OPEN` again.
- **Expected Result**: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`, but L1's SUPERVISORY afterward must be `RAILWAY_PREEMPTION` (`1`), **not** `NORMAL_OPERATION` (`3`) - CONNECTOR_GREEN (toward the crossing) stays suppressed until L1 actually receives `CROSSING_STATUS(OPEN)` from RL1. This is the CORRECT behavior under the new design (before the fix, the old code always resumed `NORMAL_OPERATION` unconditionally, potentially allowing green toward a still-closed crossing).

---

## 7. MSG_STATUS (Lx/RLx -> C1)

`c_main.c`'s `on_request()` handles `MSG_STATUS` exactly like
`MSG_HEARTBEAT` (always `RESULT_ACK`, calls `c_server_record_status()`,
no NACK branch). **Important finding**: scanning all of `lx_comm.c` and
`rlx_comm.c` confirms **no function sends `MSG_STATUS`** - both files
only have `*_send_heartbeat()`, and (RLx) `*_send_fault_report()`,
`*_broadcast_crossing_status_if_changed()`. This verb is defined and
C1 is ready to handle it, but **no sender exists anywhere in the
current codebase**. So every test case for this verb requires
test_client.

### TC-MSG-34: Valid MSG_STATUS to C1 - ACK
- **Type**: Positive
- **Verb**: MSG_STATUS
- **Related**: (no nack - and no real sender in code, see note above)
- **Environment**: (D) - required, since nothing in the current code produces this verb.
- **Setup**: `c_main` running.
- **Steps**: test_client sends `ipc_request_t{verb=MSG_STATUS, sender_id=CTRL_L2, target_id=CTRL_C1, payload.status={role=ROLE_INTERSECTION, mode=MODE_PEAK_FIXED, signal_phase=PHASE_ARTERIAL_GREEN, supervisory_state=SUPERVISORY_NORMAL_OPERATION, ...}}` to C1.
- **Expected Result**: reply `result=RESULT_ACK`. `c_mode_eng_t.controllers[idx]` (idx for CTRL_L2) has its `last_reported_*` fields updated correctly; `missed_heartbeat_ticks` and `marked_unavailable` reset to 0 (same side-effect as receiving HEARTBEAT/CROSSING_STATUS - see `c_server_record_status()`).

### TC-MSG-35: MSG_STATUS with role not matching the real sender_id - still ACK (edge case robustness)
- **Type**: Edge case
- **Verb**: MSG_STATUS
- **Related**: no cross-validation between `sender_id` and `payload.status.role` in `c_server_record_status()`
- **Environment**: (D)
- **Setup**: Same as TC-MSG-34.
- **Steps**: test_client sends `MSG_STATUS` with `sender_id=CTRL_L2` but `payload.status.role=ROLE_RAILWAY` (deliberately wrong).
- **Expected Result**: reply still `result=RESULT_ACK` (C1 doesn't check the `role` field against the sender_id - only uses `sender_id` to look up `c_mode_eng_controller_index()`). This is CORRECT for the current code (not a bug per se), but worth noting: the HMI (`c_hmi.c`) could display incorrectly if a node misreports its own role - recommend adding validation for defense against a buggy/spoofed node.

---

## 8. MSG_FAULT_REPORT (RLx -> C1)

Sent by `rlx_comm_send_fault_report()`, triggered exactly when
`rlx_fsm_take_fault_report_pending()` returns 1 (i.e. right when
`enter_fault()` runs). C1's `on_request()` always `RESULT_ACK`, no
NACK. `c_server_record_fault_report()` is currently a **complete
no-op** (only `c_logger_log()` in `c_main.c` actually prints anything).

### TC-MSG-36: MSG_FAULT_REPORT when RLx genuinely enters FAULT - ACK
- **Type**: Positive
- **Verb**: MSG_FAULT_REPORT
- **Related**: (no nack)
- **Environment**: (B)/(C) C1 + RL1
- **Setup**: Same as TC-MSG-31's setup (press `x` then `0` at RL1, wait ~20s to reach FAULT_GATE_CONFIRM_MISSING).
- **Steps**: No action needed at C1 - RLx automatically sends `MSG_FAULT_REPORT` as soon as it enters FAULT (same tick as the armed `IPC_PULSE_RAILWAY_WARNING`, see `rlx_main.c`'s `on_pulse()`).
- **Expected Result**: `central_log.txt` has a line printed directly by `c_main.c`: `FAULT_REPORT from 7: fault_code=0x00000001 severity=1 detail="RLx fault - see fault_code bitmask"` (fault_code = `FAULT_GATE_CONFIRM_MISSING` = bit 0 = 0x1). No NACK line (this verb is always ACK).

### TC-MSG-37: MSG_FAULT_REPORT with detail[64] missing a null-terminator - memory-safety check (edge case robustness)
- **Type**: Edge case
- **Verb**: MSG_FAULT_REPORT
- **Related**: string-handling safety in `c_logger_log()`/`printf("%s", ...)` at `c_main.c` when receiving a deliberately malformed payload
- **Environment**: (D) - `rlx_comm_send_fault_report()` always `strncpy`s and force-nulls the last byte, so no real code path produces a string missing a null-terminator; must be test_client deliberately violating this.
- **Setup**: `c_main` running.
- **Steps**: test_client sends `ipc_request_t{verb=MSG_FAULT_REPORT, sender_id=CTRL_RL2, target_id=CTRL_C1, payload.fault_report={fault_code=0xFF, severity=9, detail=<64 bytes all 'A', NO trailing '\0'>}}`.
- **Expected Result**: `c_main` process does NOT crash/segfault (memory-safe), reply `result=RESULT_ACK` still returned normally. Note: since `req->payload.fault_report.detail` sits inside a larger struct (`ipc_request_t`), `printf("%s", ...)` with a missing `\0` may read past it into adjacent fields in process memory until it hits a random zero byte - the printed log line may contain garbage but the process must never crash. If a crash is observed, this is a memory-safety vulnerability to report (Auditor/Verifier agent should mark FAIL).

---

## 9. MSG_CROSSING_STATUS (RLx -> adjacent Lx; RLx -> C1)

Sent by `rlx_comm_broadcast_crossing_status_if_changed()`, ONLY when
the `crossing_state_t` derived from internal state actually differs
from the last send (deduped via the static `last_broadcast_state`
variable). The receiver (both `lx_fsm_on_crossing_status()` and C1's
`on_request()`) always `RESULT_ACK` - RC-02: "Lx only observes, never
rejects".

### TC-MSG-38: CROSSING_STATUS(WARNING) to adjacent Lx - ACK, Lx enters RAILWAY_PREEMPTION
- **Type**: Positive
- **Verb**: MSG_CROSSING_STATUS
- **Related**: (no nack)
- **Environment**: (B)/(C) L1 + RL1 (C1 not required to test RLx<->Lx alone, but recommended for clearer logs)
- **Setup**: L1, RL1 started, L1 in NORMAL_OPERATION.
- **Steps**: At RL1: press `0` (TRAIN_APPROACHING) -> RL1 transitions OPEN->WARNING, automatically sends `MSG_CROSSING_STATUS{state=CROSSING_WARNING}` to L1, L2, C1.
- **Expected Result**: L1 returns `result=RESULT_ACK`; `L1.supervisory` becomes `SUPERVISORY_RAILWAY_PREEMPTION` (if a CENTRAL_OVERRIDE was running before, it is safely canceled first - see `lx_fsm_on_crossing_status()`).

### TC-MSG-39: CROSSING_STATUS to C1 - ACK, C1 updates last_reported_crossing_state
- **Type**: Positive
- **Verb**: MSG_CROSSING_STATUS
- **Related**: (no nack)
- **Environment**: (B)/(C) C1 + RL1
- **Setup**: Same as TC-MSG-38.
- **Steps**: Observe C1 receiving `MSG_CROSSING_STATUS` at the same time RL1 sends it to L1/L2.
- **Expected Result**: C1's `on_request()` case `MSG_CROSSING_STATUS` returns `result=RESULT_ACK`; `c_server_record_crossing_status()` updates `last_reported_crossing_state=CROSSING_WARNING` for RL1's index, and resets `missed_heartbeat_ticks=0`/`marked_unavailable=0`.

### TC-MSG-40: No duplicate CROSSING_STATUS sent when state hasn't changed (dedup - edge case)
- **Type**: Edge case
- **Verb**: MSG_CROSSING_STATUS
- **Related**: the `last_broadcast_state` mechanism in `rlx_comm_broadcast_crossing_status_if_changed()`
- **Environment**: (B)/(C) C1 + RL1 + L1
- **Setup**: RL1 currently in `RLX_WARNING` (already sent CROSSING_WARNING once, as in TC-MSG-38/39).
- **Steps**: Take no further action - let RL1 tick on its own (`IPC_PULSE_RAILWAY_WARNING`, every 1s) for a few seconds while its internal state still maps to `CROSSING_WARNING` (`RLX_WARNING`/`RLX_CLOSING`/`RLX_RECLOSING` all map to the same wire value - see `map_to_crossing_state()`). E.g. wait for RL1 to auto-transition from `RLX_WARNING` to `RLX_CLOSING` after 5s (`RLX_WARNING_TO_CLOSING_MS`) - an INTERNAL state change but NOT a wire state change.
- **Expected Result**: L1/C1 logs show exactly 1 line receiving `MSG_CROSSING_STATUS(WARNING)` (from OPEN->WARNING) - **no** additional line when RL1 transitions internally WARNING->CLOSING, even though RL1 ticks every second and calls `rlx_comm_broadcast_crossing_status_if_changed()` each time. Only when the gate is genuinely confirmed closed (transitioning to `RLX_CLOSED`, mapping to `CROSSING_CLOSED` - different from `CROSSING_WARNING`) does a second CROSSING_STATUS line appear.

### TC-MSG-41: CROSSING_STATUS with state outside the valid enum range - still ACK, treated as "not OPEN" (edge case robustness)
- **Type**: Edge case
- **Verb**: MSG_CROSSING_STATUS
- **Related**: no range validation for the `state` field in `lx_fsm_on_crossing_status()` (only checks `!= CROSSING_OPEN`)
- **Environment**: (D) - nothing in `rlx_comm.c` produces a `state` outside the 4 valid `crossing_state_t` values (0-3); must be test_client crafting the payload.
- **Setup**: L1 in NORMAL_OPERATION.
- **Steps**: test_client sends `ipc_request_t{verb=MSG_CROSSING_STATUS, sender_id=CTRL_RL1, target_id=CTRL_L1, payload.crossing_status={state=999}}`.
- **Expected Result**: reply `result=RESULT_ACK` (RC-02: Lx never rejects this verb regardless of content). Since `999 != CROSSING_OPEN(0)`, L1 treats it exactly as "not OPEN" -> transitions to `SUPERVISORY_RAILWAY_PREEMPTION`. This is CORRECT per the current code, but note: there is no mechanism to detect a garbage enum value from a malfunctioning node.

---

## 10. MSG_HEARTBEAT (Lx/RLx -> C1)

Sent automatically every 1000ms by both `lx_comm_send_heartbeat()` and
`rlx_comm_send_heartbeat()` (armed timer `IPC_PULSE_HEARTBEAT_TICK`, no
keyboard action needed). C1's `on_request()` always `RESULT_ACK`.
PA-07: 3 consecutive ticks (1 tick/second at C1's
`IPC_PULSE_HEARTBEAT_TICK`, see `c_watchdog_mon.c`) with no
proof-of-life (STATUS/HEARTBEAT/CROSSING_STATUS) from a controller ->
`marked_unavailable=1`.

### TC-MSG-42: Periodic 1Hz HEARTBEAT from Lx - continuous ACK
- **Type**: Positive
- **Verb**: MSG_HEARTBEAT
- **Related**: (no nack)
- **Environment**: (A)/(B) only need C1 + 1 Lx (e.g. L1) running side by side, no action needed
- **Setup**: Start `c_main`, `lx_main 1`.
- **Steps**: Take no action - just wait and observe for >= 5 seconds.
- **Expected Result**: `c_main`'s `on_request()` case `MSG_HEARTBEAT` returns `RESULT_ACK` each time (no dedicated ACK log line for the HEARTBEAT path in `c_main.c` - confirmed indirectly via `c_mode_eng.controllers[idx].missed_heartbeat_ticks` staying at 0 and the HMI (`c_hmi_render()`, running at 1Hz) showing L1 as "alive"/not marked unavailable).

### TC-MSG-43: 3 consecutive missed heartbeats - Lx marked unavailable (PA-07, edge case)
- **Type**: Edge case
- **Verb**: MSG_HEARTBEAT
- **Related**: PA-07 (`c_watchdog_mon_tick()`, 3-tick threshold per `c_mode_eng.h`'s comment)
- **Environment**: (B)/(C) C1 + L1
- **Setup**: `c_main`, `lx_main 1` running stably (HEARTBEAT already observed at regular intervals).
- **Steps**: Abruptly stop the `lx_main 1` process (Ctrl+C or `slay lx_main`/`kill`) to simulate a total connection loss. Wait >= 3 seconds (3 ticks of `IPC_PULSE_HEARTBEAT_TICK` at C1).
- **Expected Result**: After exactly 3 ticks with no HEARTBEAT/STATUS/CROSSING_STATUS from CTRL_L1, `c_mode_eng.controllers[idx].marked_unavailable` flips from 0 to 1 (edge-triggered, logs "went stale" only once); `c_hmi_render()` (running every second) shows L1 as unavailable/disconnected starting from the next render.

### TC-MSG-44: HEARTBEAT from RLx - role=ROLE_RAILWAY, mode/signal_phase=0 not misinterpreted
- **Type**: Positive (checks correct semantics of fields shared between Lx/RLx)
- **Verb**: MSG_HEARTBEAT
- **Related**: (no nack) - `status_report_payload_t` is shared by both roles, `mode`/`signal_phase`/`supervisory_state` are "meaningful for ROLE_INTERSECTION" only (comment in `ipc_msg.h`)
- **Environment**: (B)/(C) C1 + RL1
- **Setup**: `c_main`, `rlx_main 1` running, RL1 in `RLX_OPEN`.
- **Steps**: Take no action - wait for the automatic HEARTBEAT (1Hz).
- **Expected Result**: reply `RESULT_ACK`. Received payload has `role=ROLE_RAILWAY`, `crossing_state=CROSSING_OPEN(0)`, while `mode=0`, `signal_phase=0`, `supervisory_state=0` (default memset values, MEANINGLESS for RLx - `rlx_fsm_fill_status()` only fills `role`/`crossing_state`/`faults`). Confirm `c_mode_eng.controllers[idx].last_reported_mode` for RL1 = 0 is NOT misinterpreted by the HMI as a meaningful `MODE_PEAK_FIXED` (only role-appropriate fields should be displayed).

### TC-MSG-45: HEARTBEAT with role field outside the valid enum range - still ACK (negative/robustness)
- **Type**: Negative
- **Verb**: MSG_HEARTBEAT
- **Related**: no validation of `payload.heartbeat.summary.role`/other enum fields at `c_main.c`
- **Environment**: (D) - nothing in `lx_comm.c`/`rlx_comm.c` produces a role outside `{ROLE_CENTRAL, ROLE_INTERSECTION, ROLE_RAILWAY}`; must be test_client crafting it.
- **Setup**: `c_main` running.
- **Steps**: test_client sends `ipc_request_t{verb=MSG_HEARTBEAT, sender_id=CTRL_L4, target_id=CTRL_C1, payload.heartbeat.summary={role=99, ...}}`.
- **Expected Result**: reply still `result=RESULT_ACK` (C1 never NACKs any content of HEARTBEAT/STATUS/CROSSING_STATUS/FAULT_REPORT - these 4 "reporting" verbs always ACK unconditionally, differing only in whether they update `c_mode_eng_t`). Baseline behavior to know: QA should never expect a NACK from C1 for these 4 reporting verbs, however malformed the payload - any validation (if desired) would need to be added to `c_server.c`, which doesn't currently exist.

---

## Appendix: Notable findings from reading the code (not test cases, but directly affecting testability)

1. **`NACK_REASON_PEDESTRIAN_ACTIVE` is dead code.** Declared in
   `sys_types.h` with the comment "pedestrian clearance active and
   cannot be safely deferred", and has a display name in both
   `c_comm.c` and `c_operator.c`'s `nack_reason_name()`. But grepping
   all of `app/` shows **no `lx_fsm_on_*()` ever assigns this value** -
   the case of ped-clearance running during a `REQUEST_OVERRIDE` is
   handled with `RESULT_ACK_PENDING` (see TC-MSG-10), not this NACK.
   Two possibilities: (a) the `sys_types.h` comment describes an older
   design no longer matching the code, or (b) this value was intended
   for a different situation not yet implemented (e.g. another verb
   that could also conflict with ped clearance but has no "wait"
   mechanism like override does). Proposal: the Compliance Agent should
   cross-check against the original documents (SD-07/UC-08) to confirm
   whether this is API drift needing a comment fix, or a missing code
   branch.

2. **`lx_sensor.c` lacks a manual demo fault-trigger key**, unlike
   `rlx_sensor.c` (which has `x` to arm gate-fail and `f` to
   self-clear a local fault). Consequence: **no test case involving
   `NACK_REASON_FAULT_ACTIVE` on Lx** (SET_TIMING_PROFILE, SET_MODE,
   REQUEST_OVERRIDE) can run via normal keyboard operation - must wait
   for a real PA-10 watchdog trip (server thread hung >= 2s for real)
   or add code. Recommend the Core-Engineer add a DEMO-ONLY key
   calling `lx_fsm_report_watchdog_trip(&fsm)` to `lx_sensor.c`,
   matching the pattern already used on the RLx side.

3. **`MSG_STATUS` is defined and C1 is ready to handle it, but no
   sender exists anywhere in the codebase (`lx_comm.c`, `rlx_comm.c`)
   that ever sends this verb.** All actual status reporting goes
   through `MSG_HEARTBEAT` (which reuses `status_report_payload_t`
   inside `heartbeat_payload_t`). Needs clarification with the
   Compliance Agent: is this leftover scope creep (`MSG_STATUS`
   intended for a separate UC-09 flow never wired up) or is the C1
   handler unnecessary dead code?

4. **`RESULT_ACK` for `MSG_REQUEST_FAULT_CLEAR` appears unreachable
   with the current code** (see the detailed explanation in TC-MSG-32) -
   every path into `RLX_FAULT` automatically commands the gate closed
   again (`rlx_gate_command_close()`), with no path that opens the
   gate while in FAULT. This is the document's most important finding:
   if the analysis holds, RC-09/RC-10's "operator request fault
   clearance after repair" flow **has never been able to ACK** on the
   current build, even on real QNX hardware with the correct
   procedure - needs Core-Engineer confirmation and a fix before any
   RC-09-related Phase can be marked "PASS" at step 6 (QA-Test).

5. **`c_server_record_fault_report()` is a complete no-op** (only
   `c_logger_log()` called directly from `c_main.c` actually prints
   anything) - `c_mode_eng_t` has no long-term fault history storage
   for RLx. Doesn't block protocol testing (the verb still ACKs
   correctly), but affects UC-09/HMI test planning (out of scope for
   this document).

6. **The 4 "reporting" verbs** (MSG_STATUS, MSG_HEARTBEAT,
   MSG_FAULT_REPORT, MSG_CROSSING_STATUS received at C1, and
   MSG_CROSSING_STATUS/MSG_HEARTBEAT received at Lx) **never return
   NACK** anywhere in the current codebase - `result` can only be
   `RESULT_ACK` (correct route) or `RESULT_ERROR` (verb sent to a node
   that doesn't support it, e.g. TC-MSG-33). All 8 `nack_reason_t`
   values in `sys_types.h` are only actually used by the 5 "command"
   verbs (SET_TIMING_PROFILE, SET_MODE, REQUEST_OVERRIDE,
   RENEW_OVERRIDE, CANCEL_OVERRIDE, REQUEST_FAULT_CLEAR) - and of
   those, `NACK_REASON_PEDESTRIAN_ACTIVE` is still never used anywhere
   (Appendix item 1).
