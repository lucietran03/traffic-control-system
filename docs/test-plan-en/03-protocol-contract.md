# 03 - Test Plan: IPC Protocol Contract

Scope of this document: test each **verb** in `msg_type_t`
(`app/shared/includes/ipc_msg.h`) and each possible **outcome** when that
verb is sent over Qnet (`RESULT_ACK` / `RESULT_ACK_PENDING` /
`RESULT_NACK` + a specific `nack_reason_t` / `RESULT_ERROR`), checked
directly against the real logic in `lx_fsm.c`, `rlx_fsm.c`,
`c_mode_eng.c`, `c_main.c`. This is NOT a functional/timing test plan
(see the other files under `docs/test-plan/`) - the sole focus here is:
"for input X, does the (result, reason) pair returned on the wire match
what the code specifies?".

## Environment convention (A/B/C/D)

| Symbol | Meaning |
|---|---|
| **(A)** | Single node, no real Qnet needed (e.g. running only `c_main`/`lx_main` alone, observing internal behavior - rarely used for protocol tests since at least 2 sides sending/receiving are needed). |
| **(B)** | Multiple processes (nodes) running on **the same QNX machine**, each node its own executable (`c_main`, `lx_main <n>`, `rlx_main <n>`), communicating over local Qnet (no `TRAFFIC_NODE_MAP` needed, defaults to "same node as caller" - see `qnet_utils.h`). Sufficient to verify the protocol contract's correctness since `MsgSend/MsgReceive/MsgReply` still go through the real kernel. |
| **(C)** | Multiple **physical or virtual QNX machines/VMs**, connected over a real network, using the `TRAFFIC_NODE_MAP` environment variable to resolve node names (see `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`). Used for tests that must confirm behavior is unchanged over a real network (latency, packet loss...). For pure protocol-contract tests, (B) and (C) give **identical result/reason** outcomes - this document uses (C) only when it needs to emphasize the project's "must run on a real distributed environment" requirement. |
| **(D)** | **Requires a dedicated test_client tool (NOT YET in the repo - proposed to be built).** `c_operator.c` (keyboard at C1) and `lx_sensor.c`/`rlx_sensor.c` (keyboard at Lx/RLx) only allow entering values that are formally valid (they prompt for numbers via `scanf`, with no path to self-send a wrong-typed payload, an unknown verb, or target the wrong node type). Some outcomes in the protocol contract only occur with a message that "doesn't play by the normal rules" - those cases require environment (D). |

### Proposed test_client tool (for every case marked (D))

Does not yet exist in the repo. Proposal: a small executable,
`app/tools/test_client/test_client.c`, reusing
`app/shared/includes/qnet_utils.h` / `qnet_utils.c` directly (already
has `ipc_attach_name()` to build the attach-point name and the
`TRAFFIC_NODE_MAP` mechanism to open a connection to a remote node) and
`ipc_msg.h`. No need for `ipc_client_queue_t`/a background thread like
the real nodes - just:

```c
name_open("/net/<node>/dev/name/global/traffic/<suffix>", 0) (or same-node)
ipc_request_t req; /* manually set every field, including "invalid" ones */
MsgSend(coid, &req, sizeof(req), &reply, sizeof(reply));
print reply.result / reply.reason
```

Since `ipc_request_t`/`ipc_reply_t` and all verb/nack_reason constants
are already shared public headers, test_client only needs ~100 lines:
parse command-line arguments (target node, verb, numeric payload
fields), build the `ipc_request_t` union for the given verb, send once,
print the reply. This is the only "whitebox" tool that can produce: (1)
a formally valid payload with a value outside every bound that
`c_operator.c` allows entering, (2) a verb sent to the wrong node type,
(3) 2 requests sent nearly simultaneously from 2 independent
test_client processes to probe races, (4) a payload that is
deliberately malformed (a string without a terminating `\0`, an enum
out of range).

### Other general conventions

- Log to check: `central_log.txt` (written by `c_logger_log()`, also
  printed to the `c_main` process's stdout) following `c_comm.c`'s
  `on_command_reply()` format exactly:
  - NACK: `C1: <VERB> to <target> -> NACK reason=<REASON>`
  - Other: `C1: <VERB> to <target> -> <ACK|ACK_PENDING|ERROR>`
  For verbs that Lx/RLx send to C1 themselves (STATUS/HEARTBEAT/
  FAULT_REPORT/CROSSING_STATUS), the corresponding log lives in the
  `on_request()` branch in `c_main.c` (some branches currently log
  nothing beyond updating `c_mode_eng_t` - see the note in each test
  case).
- Lx: run `lx_main <1..6>`; RLx: run `rlx_main <1..3>`; C1: run
  `c_main`. Control keys: see `print_help()` in `c_operator.c` /
  `lx_sensor.c` / `rlx_sensor.c`.
- Railway-intersection adjacency map (`rlx_comm.c`'s `ADJACENCY[]`):
  RL1 adjacent to L1,L2; RL2 adjacent to L3,L4; RL3 adjacent to L5,L6.
- Important constants: `LX_CYCLE_LENGTH_MS = 90000` (48000+4000+2000+
  30000+4000+2000, `lx_timer.h`), `LX_OVERRIDE_DURATION_CAP_MS =
  300000`, chain R1 offsets = {L1:0, L3:21000, L5:45000}ms, R2 =
  {L2:0, L4:19000, L6:42000}ms (`c_mode_eng.h`), `RLX_GATE_MOTION_MS =
  3000`, `RLX_WARNING_TO_CLOSING_MS = 5000`,
  `RLX_CLOSING_DEADLINE_MS = 15000`, `RLX_OPENING_DEADLINE_MS = 15000`
  (`rlx_timer.h`/`rlx_gate.h`).
- **Important findings to know before testing (details in the Appendix
  at the end of this file):** (1) `lx_sensor.c` has no manual
  fault-trigger key (unlike `rlx_sensor.c`, which already has `x`/`f`),
  so there is no way to force an Lx into `SUPERVISORY_FAULT_SAFE` by
  keyboard alone - PA-10 only actually trips when the server thread
  really hangs for >= 2s; (2) `MSG_STATUS` is handled by `c_main.c` but
  **no code path currently actually sends** this verb
  (`lx_comm.c`/`rlx_comm.c` only send HEARTBEAT/FAULT_REPORT/
  CROSSING_STATUS); (3) `RESULT_ACK` for `MSG_REQUEST_FAULT_CLEAR`
  appears **unreachable** with the current code because there is no
  path where the open-gate command (`rlx_gate_command_open()`) runs
  while in `RLX_FAULT`; (4) `NACK_REASON_PEDESTRIAN_ACTIVE` is declared
  and has a log name but **is never assigned** anywhere in `lx_fsm.c`
  (the ped-clearance case uses `ACK_PENDING`, not this NACK) - see
  Appendix.

---

## 1. MSG_SET_TIMING_PROFILE (C1 -> Lx)

Handled by `lx_fsm_on_set_timing_profile()`. Only 2 NACK branches:
`NACK_REASON_FAULT_ACTIVE` (currently FAULT_SAFE) and
`NACK_REASON_STALE_OR_UNSAFE_PROFILE` (`offset_ms >= LX_CYCLE_LENGTH_MS`
= 90000). `c_operator.c`'s `t` key only sends fixed offsets from
`R1_CHAIN`/`R2_CHAIN` (0/21000/45000/19000/42000ms) - none >= 90000, so
the boundary case must use test_client.

### TC-MSG-1: Valid SET_TIMING_PROFILE - ACK
- **Type**: Positive
- **Verb**: MSG_SET_TIMING_PROFILE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1 (+ L3, L5 in the same R1 chain, not mandatory but recommended so the broadcast runs error-free)
- **Setup**: Start `c_main`, `lx_main 1`, `lx_main 3`, `lx_main 5`.
- **Steps**: At C1: press `t` -> `chain (1=R1 L1/L3/L5, 2=R2 L2/L4/L6): 1`.
- **Expected Result**: `central_log.txt` has 3 lines `C1: SET_TIMING_PROFILE to <id> -> ACK` (id = 1, 3, 5 - CTRL_L1/L3/L5). At L1: `active_profile_id` updates to the newly assigned profile_id (printed in internal log if available), `offset_apply_pending=1` (observed indirectly: the green wave shifts by the correct offset at the next arterial-green cycle).

### TC-MSG-2: SET_TIMING_PROFILE while Lx is FAULT_SAFE - NACK FAULT_ACTIVE
- **Type**: Negative
- **Verb**: MSG_SET_TIMING_PROFILE
- **Related**: NACK_REASON_FAULT_ACTIVE
- **Environment**: (B) — sending `SET_TIMING_PROFILE` itself does NOT need test_client (the real `t` key of `c_operator.c` works fine). The real problem is the **precondition**: `lx_sensor.c` currently has NO manual fault key (unlike `rlx_sensor.c`'s `x`/`f`), and PA-10 only really trips when the Lx server thread actually stops ticking for >= 2s continuously (`lx_watchdog.c`, `LX_WATCHDOG_CHECK_INTERVAL_S=2`) - the only path is `lx_fsm_report_watchdog_trip()` called from `lx_watchdog_thread()` (`05-fault-safety.md:402`), with no deterministic way to trigger it via ordinary keyboard/CLI action. **This case is Skip not because test_client is missing, but because there is no way to force Lx into `SUPERVISORY_FAULT_SAFE`** - both feasible paths are still unusable: a debugger suspending the server thread specifically (not yet available, see TC-FAULT-17/18), or `kill -STOP`/`kill -CONT` (result UNDETERMINED, see the TC-SC01A-3 vs TC-FAULT-16 contradiction). Long-term suggestion: add a DEMO-ONLY key to `lx_sensor.c` that calls `lx_fsm_report_watchdog_trip(&fsm)` directly, following the pattern already in `rlx_sensor.c`'s `f` key.
- **Setup**: L1 in `SUPERVISORY_FAULT_SAFE` — **currently no reliable way to establish this** (see explanation above).
- **Steps**: From C1: `t` -> `1` (broadcast R1) while L1 is FAULT_SAFE.
- **Expected Result**: `central_log.txt`: `C1: SET_TIMING_PROFILE to 1 -> NACK reason=FAULT_ACTIVE`.

### TC-MSG-3: offset_ms exactly at the upper boundary (>= LX_CYCLE_LENGTH_MS) - NACK STALE_OR_UNSAFE_PROFILE (edge case)
- **Type**: Edge case
- **Verb**: MSG_SET_TIMING_PROFILE
- **Related**: NACK_REASON_STALE_OR_UNSAFE_PROFILE; boundary `offset_ms >= 90000` (`lx_fsm_on_set_timing_profile()`)
- **Environment**: (D) - `c_operator.c` does not allow entering an arbitrary offset, only sends constants already fixed in `c_mode_eng.c`.
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
- **Expected Result**: reply `result=RESULT_ACK`. `active_profile_id=100`, `assigned_offset_ms=89999`, `offset_apply_pending=1` (applied at the next arterial-green).

---

## 2. MSG_SET_MODE (C1 -> Lx)

Handled by `lx_fsm_on_set_mode()`. 2 NACK branches
(`NACK_REASON_FAULT_ACTIVE`, and - re-audit fix, see TC-MSG-8b -
`NACK_REASON_OUT_OF_RANGE` when `payload->mode` is neither 0
(`MODE_PEAK_FIXED`) nor 1 (`MODE_OFF_PEAK_SENSOR`), matching UC-07 main
flow step 3, "validates the request against supported ranges"); 2
positive branches: `RESULT_ACK` (sent mode == current mode, treated as
a no-op) and `RESULT_ACK_PENDING` (different mode, deferred to the next
ALL_RED boundary - SC-01A).

### TC-MSG-5: SET_MODE with mode matching the current mode - ACK (no-op)
- **Type**: Positive
- **Verb**: MSG_SET_MODE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1
- **Setup**: L1 starts in `MODE_PEAK_FIXED` (default cold-start, `lx_fsm_init()`).
- **Steps**: At C1: `m` -> `Lx number: 1` -> `mode: 0` (PEAK_FIXED).
- **Expected Result**: `central_log.txt`: `C1: SET_MODE to 1 -> ACK`. `mode_change_pending` at L1 stays 0 (nothing deferred).

### TC-MSG-6: SET_MODE to a different mode - ACK_PENDING, applied exactly at the ALL_RED boundary
- **Type**: Positive
- **Verb**: MSG_SET_MODE
- **Related**: (no nack) - PA-... SC-01A
- **Environment**: (B) C1 + L1
- **Setup**: L1 currently `MODE_PEAK_FIXED`, in the middle of `PHASE_ARTERIAL_GREEN` or any phase not yet at ALL_RED.
- **Steps**: At C1: `m` -> `1` -> `mode: 1` (OFF_PEAK_SENSOR). Watch L1's log/console for up to 1 cycle (<= ~54s to the nearest `PHASE_ALL_RED_A_TO_B` or `PHASE_ALL_RED_B_TO_A`).
- **Expected Result**: Immediately: `central_log.txt`: `C1: SET_MODE to 1 -> ACK_PENDING`. Then, exactly at the next entry into ALL_RED (not sooner, not interrupting the running phase), `fsm->mode` becomes `MODE_OFF_PEAK_SENSOR` (observed via the 4s extension behavior instead of fixed-duration at the next `PHASE_ARTERIAL_GREEN`/`PHASE_CONNECTOR_GREEN`).

### TC-MSG-7: Cancel a pending mode-change by resending the current mode before the boundary (edge case)
- **Type**: Edge case
- **Verb**: MSG_SET_MODE
- **Related**: `mode_change_pending=0` behavior when payload->mode == fsm's current mode (`lx_fsm_on_set_mode()`)
- **Environment**: (B) C1 + L1
- **Setup**: L1 currently `MODE_PEAK_FIXED`.
- **Steps**: (1) `m` -> `1` -> `1` (request OFF_PEAK_SENSOR) -> receive ACK_PENDING. (2) Before L1 reaches the next ALL_RED boundary, send `m` -> `1` -> `0` (request PEAK_FIXED again, i.e. the actual current mode).
- **Expected Result**: Step (2) returns `result=RESULT_ACK` (not ACK_PENDING, since `payload->mode == fsm`'s current mode). `mode_change_pending` is reset to 0 - at the next ALL_RED boundary, L1 does NOT change mode (stays PEAK_FIXED), confirming request (1) was fully cancelled rather than silently applied.

### TC-MSG-8: SET_MODE while Lx is FAULT_SAFE - NACK FAULT_ACTIVE
- **Type**: Negative
- **Verb**: MSG_SET_MODE
- **Related**: NACK_REASON_FAULT_ACTIVE
- **Environment**: (B) — same reasoning as TC-MSG-2: sending `SET_MODE` doesn't need test_client, but **there is no way to force Lx into `SUPERVISORY_FAULT_SAFE`** (no demo key on `lx_sensor.c`, debugger not yet used, `kill -STOP` result undetermined — see the full explanation in TC-MSG-2).
- **Setup**: L1 in `SUPERVISORY_FAULT_SAFE` — currently no reliable way to establish this.
- **Steps**: At C1: `m` -> `1` -> `1`.
- **Expected Result**: `central_log.txt`: `C1: SET_MODE to 1 -> NACK reason=FAULT_ACTIVE`.

### TC-MSG-8b: SET_MODE with `mode` out of valid range (other than 0/1) - NACK OUT_OF_RANGE (UC-07 step 3)
- **Type**: Negative (re-audit fix)
- **Verb**: MSG_SET_MODE
- **Related**: `NACK_REASON_OUT_OF_RANGE`, UC-07 main flow step 3 ("validates
  the request against supported ranges"). `c_operator.c`'s `handle_set_mode()`
  already blocks any value other than 0/1 right at the console
  (`n != MODE_PEAK_FIXED && n != MODE_OFF_PEAK_SENSOR` -> "command aborted",
  nothing sent) - so this NACK branch **cannot be reproduced via the
  `c_operator` keyboard**, only via test_client sending a raw invalid
  payload directly, exactly as the comment in `lx_fsm_on_set_mode()` states:
  "this FSM (not the console) is the documented authoritative validator -
  the wire contract has no guarantee the sender is always a well-behaved
  operator".
- **Environment**: (D) - mandatory, since `c_operator.c`'s pre-check blocks
  this exact threshold before sending.
- **Setup**: L1 has no fault, currently `MODE_PEAK_FIXED`.
- **Steps**: test_client sends directly to L1 an
  `ipc_request_t{verb=MSG_SET_MODE, sender_id=CTRL_C1, target_id=CTRL_L1,
  payload.mode={mode=2}}` (any value other than 0/1).
- **Expected Result**: reply `result=RESULT_NACK`,
  `reason=NACK_REASON_OUT_OF_RANGE`; `fsm->mode`/`fsm->mode_change_pending`
  unchanged (request fully rejected, not silently falling into the old
  `else` branch as it did before the re-audit fix).

---

## 3. MSG_REQUEST_OVERRIDE (C1 -> Lx)

Handled in 2 layers: (1) `c_mode_eng_validate_override_request()` at
**Central** (formal pre-check: valid target, `duration_ms` in
(0, 300000], `override_type == OVERRIDE_CLEAR_ROUTE`) - if it fails,
the request **never leaves C1 on the wire** (`c_operator.c`'s
`handle_request_override()` just logs locally at C1, with no real
`ipc_reply_t` from Lx); (2) `lx_fsm_on_request_override()` at **Lx** -
a deeper protection layer, exact check order in the code: another
CENTRAL_OVERRIDE already running ->
`NACK_REASON_OUT_OF_RANGE`; `duration_ms==0` or `>300000` ->
`NACK_REASON_INVALID_DURATION`; currently `RAILWAY_PREEMPTION` ->
`NACK_REASON_RAILWAY_CONFLICT`; currently `FAULT_SAFE` ->
`NACK_REASON_FAULT_ACTIVE`; currently `ped_clearance_active` ->
`RESULT_ACK_PENDING` (NOT NACK - see the Appendix on
`NACK_REASON_PEDESTRIAN_ACTIVE`); otherwise -> `RESULT_ACK`.

Since the duration thresholds at Central (0, 300000] and at Lx are
identical, **`c_operator.c` can never actually reach the
`NACK_REASON_INVALID_DURATION` branch that really lives in
`lx_fsm.c`** - any duration typed outside (0,300000] is already blocked
by Central before being sent. The Lx-side test cases must send directly
via test_client, bypassing Central.

### TC-MSG-9: Valid REQUEST_OVERRIDE, no conflict - ACK
- **Type**: Positive
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1
- **Setup**: L1 in NORMAL_OPERATION, no fault, no railway preemption, no ped clearance running.
- **Steps**: C1: `o` -> `Lx number: 1` -> `target movement: 0` (arterial) -> `duration_ms: 30000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK`. L1: `supervisory=SUPERVISORY_CENTRAL_OVERRIDE`, `override_substate=OVR_ACTIVE`, holds green on the ARTERIAL movement for 30s then ends automatically (safe clearance).

### TC-MSG-10: REQUEST_OVERRIDE while ped clearance is running - ACK_PENDING (SC-03B)
- **Type**: Positive
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: (no nack - this is exactly the case the code comment hints at "NACK_REASON_PEDESTRIAN_ACTIVE" for but which actually returns ACK_PENDING, see Appendix)
- **Environment**: (B) C1 + L1
- **Setup**: At L1's sensor console (`lx_sensor_reader_thread`), while `PHASE_ARTERIAL_GREEN` is running, press `1` (ped side 0) to start the WALK/FLASHING_DONT_WALK sequence (total 10s: 6s WALK + 4s FDW).
- **Steps**: While the WALK/FDW sequence is still running (within that 10s), at C1: `o` -> `1` -> `target movement: 0` (arterial, the side currently being served) -> `duration_ms: 20000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK_PENDING`. L1: `override_substate=OVR_PENDING_CLEARANCE`, `supervisory=SUPERVISORY_CENTRAL_OVERRIDE` immediately, but the override only truly holds green (OVR_ACTIVE) AFTER the WALK/FDW sequence finishes (`ped_clearance_active` back to 0) - no second reply is sent when the override actually activates.

### TC-MSG-11: REQUEST_OVERRIDE while another override is already running - NACK OUT_OF_RANGE
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_OUT_OF_RANGE (used as a stand-in since "there is no dedicated error code for this case" - comment in `lx_fsm_on_request_override()`)
- **Environment**: (B) C1 + L1
- **Setup**: Run TC-MSG-9 first (L1 has an ACTIVE override, not yet expired).
- **Steps**: While the first override is still in effect, send: C1: `o` -> `1` -> `target movement: 1` -> `duration_ms: 10000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> NACK reason=OUT_OF_RANGE`. The first override (arterial, 30s) keeps running unchanged.

### TC-MSG-12: REQUEST_OVERRIDE duration_ms=0 - NACK INVALID_DURATION (blocked at the Central pre-check)
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_INVALID_DURATION (`c_mode_eng_validate_override_request()`, NOT a real reply from Lx)
- **Environment**: (B) C1 + L1
- **Setup**: L1 running normally.
- **Steps**: C1: `o` -> `1` -> `target movement: 0` -> `duration_ms: 0`.
- **Expected Result**: `central_log.txt`: `Operator: REQUEST_OVERRIDE(target=1, movement=0, duration_ms=0) rejected by Central pre-check, reason=INVALID_DURATION - not forwarded to the controller`. **NO** `C1: REQUEST_OVERRIDE to 1 -> ...` line at all (the request never left C1 - `ipc_client_post()` is never called).

### TC-MSG-13: REQUEST_OVERRIDE duration_ms=300001 - NACK INVALID_DURATION (blocked at the Central pre-check)
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_INVALID_DURATION (Central pre-check)
- **Environment**: (B) C1 + L1
- **Setup**: Same as TC-MSG-12.
- **Steps**: C1: `o` -> `1` -> `0` -> `duration_ms: 300001`.
- **Expected Result**: Same as TC-MSG-12 but with `duration_ms=300001` in the log; no message sent to L1.

### TC-MSG-14: REQUEST_OVERRIDE duration_ms=0 sent directly to Lx, bypassing Central - NACK INVALID_DURATION (the real Lx-side branch)
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_INVALID_DURATION (the REAL branch in `lx_fsm_on_request_override()`, not via `c_operator.c`)
- **Environment**: (D) - mandatory, since `c_operator.c`'s pre-check blocks this exact threshold before sending (see the intro to section 3).
- **Setup**: L1 running normally, no override currently active.
- **Steps**: test_client sends directly to L1: `ipc_request_t{verb=MSG_REQUEST_OVERRIDE, sender_id=CTRL_C1, target_id=CTRL_L1, payload.override_request={override_type=OVERRIDE_CLEAR_ROUTE, target_movement=0, duration_ms=0}}`.
- **Expected Result**: reply `result=RESULT_NACK`, `reason=NACK_REASON_INVALID_DURATION`. This is independent proof that Lx has its own defense-in-depth validation, not relying solely on Central.

### TC-MSG-15: REQUEST_OVERRIDE while Lx is in RAILWAY_PREEMPTION - NACK RAILWAY_CONFLICT
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_RAILWAY_CONFLICT
- **Environment**: (B)/(C) C1 + L1 + RL1 (L1 adjacent to RL1 per `ADJACENCY[]`)
- **Setup**: Start `rlx_main 1`. At RL1's sensor console, press `0` (TRAIN_APPROACHING direction 0) so RL1 goes OPEN -> WARNING, triggering a `MSG_CROSSING_STATUS(WARNING)` broadcast to L1 (and L2, C1). Confirm L1 has entered `SUPERVISORY_RAILWAY_PREEMPTION`.
- **Steps**: C1: `o` -> `Lx number: 1` -> `target movement: 1` (connector - the direction blocked by the railway) -> `duration_ms: 10000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> NACK reason=RAILWAY_CONFLICT`.

### TC-MSG-16: REQUEST_OVERRIDE while Lx is FAULT_SAFE - NACK FAULT_ACTIVE
- **Type**: Negative
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: NACK_REASON_FAULT_ACTIVE
- **Environment**: (B) — same reasoning as TC-MSG-2: sending `REQUEST_OVERRIDE` doesn't need test_client, but **there is no way to force Lx into `SUPERVISORY_FAULT_SAFE`** (see the full explanation in TC-MSG-2).
- **Setup**: L1 in `SUPERVISORY_FAULT_SAFE` — currently no reliable way to establish this.
- **Steps**: C1: `o` -> `1` -> `0` -> `duration_ms: 10000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> NACK reason=FAULT_ACTIVE`.

### TC-MSG-17: duration_ms=1 - ACK (valid lower boundary, PA-11)
- **Type**: Edge case
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: PA-11, boundary `(0, 300000]`
- **Environment**: (B) C1 + L1
- **Setup**: L1 normal, no override/fault/preemption.
- **Steps**: C1: `o` -> `1` -> `target movement: 0` -> `duration_ms: 1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK`. The override ends almost immediately at the next 100ms tick of `lx_fsm_on_phase_timer()` (since `override_remaining_ms=1 <= LX_PHASE_TICK_MS=100`).

### TC-MSG-18: duration_ms=300000 - ACK (valid upper boundary, exactly PA-11)
- **Type**: Edge case
- **Verb**: MSG_REQUEST_OVERRIDE
- **Related**: PA-11, upper boundary `LX_OVERRIDE_DURATION_CAP_MS = 300000`
- **Environment**: (B) C1 + L1
- **Setup**: Same as TC-MSG-17.
- **Steps**: C1: `o` -> `1` -> `0` -> `duration_ms: 300000`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_OVERRIDE to 1 -> ACK` (300000 accepted, 300001 already NACK'd in TC-MSG-13/the Lx-layer equivalent). `override_duration_ms=300000` at L1.

### TC-MSG-18b (Regression, fixed): REQUEST_OVERRIDE with target_movement outside the valid enum range - NACK OUT_OF_RANGE
- **Type**: Regression (fixed) — defense-in-depth, unreachable from `c_operator.c` (only lets you pick 0/1) but must still be blocked at the FSM layer since that is the documented "authoritative validator" - the wire contract does not guarantee a well-behaved sender.
- **Verb**: MSG_REQUEST_OVERRIDE
- **Why this was a bug**: `lx_fsm_on_request_override()` previously did not check whether `payload->target_movement` was `OVERRIDE_MOVEMENT_ARTERIAL`(0) or `OVERRIDE_MOVEMENT_CONNECTOR`(1) - an out-of-range value (e.g. 2, 255) would be silently misread as ARTERIAL by every call site (since they all only ever checked `== OVERRIDE_MOVEMENT_CONNECTOR`) instead of being rejected. Fixed by mirroring the check already present in `lx_fsm_on_set_mode()`.
- **Related**: `app/intersection/src/lx_fsm.c : lx_fsm_on_request_override()` (the `target_movement != OVERRIDE_MOVEMENT_ARTERIAL && != OVERRIDE_MOVEMENT_CONNECTOR` branch -> `NACK_REASON_OUT_OF_RANGE`).
- **Environment**: (D) needs test_client to send a `target_movement` value that `c_operator.c` never generates (console only asks for 0/1).
- **Setup**: L1 normal, no override/fault/preemption.
- **Steps**: test_client sends directly to L1 (or via C1 if C1 does not itself pre-block it): `ipc_request_t{verb=MSG_REQUEST_OVERRIDE, sender_id=CTRL_C1, target_id=CTRL_L1, payload.override_request={override_type=OVERRIDE_CLEAR_ROUTE, target_movement=2, duration_ms=5000}}`.
- **Expected Result**: `RESULT_NACK`, `reason=NACK_REASON_OUT_OF_RANGE`. `fsm->supervisory` stays `NORMAL_OPERATION`, `override_target_movement` not overwritten.

---

## 4. MSG_RENEW_OVERRIDE (C1 -> Lx)

Handled by `lx_fsm_on_renew_override()`. ACK condition: must currently
be `SUPERVISORY_CENTRAL_OVERRIDE` **and** `override_substate ==
OVR_ACTIVE` (note: `OVR_PENDING_CLEARANCE` does NOT qualify - still
NACK `UNKNOWN_TARGET`). Second NACK: `extend_duration_ms > 300000` ->
`INVALID_DURATION`. `extend_duration_ms=0` means "renew back to exactly
the original duration" (`override_duration_ms`, not 0ms).

### TC-MSG-19: Valid RENEW_OVERRIDE on an ACTIVE override - ACK
- **Type**: Positive
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1
- **Setup**: Run TC-MSG-9 (override ACTIVE, 30000ms, still in effect).
- **Steps**: C1: `r` -> `Lx number: 1` -> `extend_duration_ms: 60000`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_duration_ms` and `override_remaining_ms` at L1 = 60000 (restarted from scratch, not added on top of elapsed time).

### TC-MSG-20: RENEW_OVERRIDE with no override present - NACK UNKNOWN_TARGET
- **Type**: Negative
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: NACK_REASON_UNKNOWN_TARGET
- **Environment**: (B) C1 + L1
- **Setup**: L1 in NORMAL_OPERATION, no override ever sent.
- **Steps**: C1: `r` -> `1` -> `extend_duration_ms: 5000` (the operator prints a warning "Central has no override recorded in flight" but still sends it - as designed per BR-7).
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> NACK reason=UNKNOWN_TARGET`.

### TC-MSG-21: RENEW_OVERRIDE on an override still OVR_PENDING_CLEARANCE (not yet active) - NACK UNKNOWN_TARGET (edge case)
- **Type**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: NACK_REASON_UNKNOWN_TARGET; the exact condition `override_substate != OVR_ACTIVE` (which includes PENDING_CLEARANCE) in `lx_fsm_on_renew_override()`
- **Environment**: (B) C1 + L1
- **Setup**: Reproduce TC-MSG-10 (override in `OVR_PENDING_CLEARANCE`, waiting for ped clearance to finish).
- **Steps**: While the override is still PENDING_CLEARANCE (not yet ACTIVE), immediately send: C1: `r` -> `1` -> `extend_duration_ms: 0`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> NACK reason=UNKNOWN_TARGET` (even though from a business-logic standpoint the override "exists", the code only considers a renew valid once it is truly ACTIVE). The original PENDING_CLEARANCE override is unaffected by this rejected renew.

### TC-MSG-22: RENEW_OVERRIDE extend_duration_ms=300001 - NACK INVALID_DURATION
- **Type**: Negative
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: NACK_REASON_INVALID_DURATION
- **Environment**: (B) C1 + L1
- **Setup**: Override currently ACTIVE (TC-MSG-9).
- **Steps**: C1: `r` -> `1` -> `extend_duration_ms: 300001`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> NACK reason=INVALID_DURATION`. The current override expiry is left unchanged (code comment: "Retain the current expiry unchanged on rejection").

### TC-MSG-23: extend_duration_ms=0 - ACK, renews to exactly the ORIGINAL duration (edge case)
- **Type**: Edge case
- **Verb**: MSG_RENEW_OVERRIDE
- **Related**: the special semantics of the value 0 (`renew_override_payload_t.extend_duration_ms`)
- **Environment**: (B) C1 + L1
- **Setup**: Create an initial override with `duration_ms=20000` (TC-MSG-9 style, using 20000 instead of 30000). Wait a few seconds for `override_remaining_ms` to drop below 20000 (e.g. to ~15000ms).
- **Steps**: C1: `r` -> `1` -> `extend_duration_ms: 0`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_remaining_ms` is RESET to 20000 (the ORIGINAL `override_duration_ms`, not added to the remaining ~15000, and not set to 0).

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
- **Related**: distinguishes from the "0 = keep unchanged" case (TC-MSG-23) - this is an explicit 1ms renewal
- **Environment**: (B) C1 + L1
- **Setup**: Override currently ACTIVE.
- **Steps**: C1: `r` -> `1` -> `extend_duration_ms: 1`.
- **Expected Result**: `central_log.txt`: `C1: RENEW_OVERRIDE to 1 -> ACK`. `override_duration_ms=override_remaining_ms=1` (different from 0 - this is a real 1ms, not "keep original"), the override ends at the very next tick.

---

## 5. MSG_CANCEL_OVERRIDE (C1 -> Lx)

Handled by `lx_fsm_on_cancel_override()`. No payload on the wire. ACK
if `override_substate` is `OVR_ACTIVE` **or** `OVR_PENDING_CLEARANCE`
(both cancelled via `lx_fsm_terminate_override_locked()`); NACK
`UNKNOWN_TARGET` if `OVR_NONE`.

### TC-MSG-26: CANCEL_OVERRIDE on an ACTIVE override - ACK
- **Type**: Positive
- **Verb**: MSG_CANCEL_OVERRIDE
- **Related**: (no nack)
- **Environment**: (B) C1 + L1
- **Setup**: Override ACTIVE (TC-MSG-9, long duration, e.g. 60000ms to leave time to act).
- **Steps**: C1: `c` -> `Lx number: 1`.
- **Expected Result**: `central_log.txt`: `C1: CANCEL_OVERRIDE to 1 -> ACK`. L1: `override_substate=OVR_NONE`, `supervisory` returns to `SUPERVISORY_NORMAL_OPERATION` immediately (safely - via the "safe clearance" placeholder).

### TC-MSG-27: CANCEL_OVERRIDE on an override still OVR_PENDING_CLEARANCE (not yet active) - ACK (edge case)
- **Type**: Edge case
- **Verb**: MSG_CANCEL_OVERRIDE
- **Related**: the `override_substate == OVR_PENDING_CLEARANCE` branch is also accepted for cancellation (unlike RENEW_OVERRIDE in TC-MSG-21, which rejects this case)
- **Environment**: (B) C1 + L1
- **Setup**: Reproduce TC-MSG-10 (override PENDING_CLEARANCE, waiting on ped clearance).
- **Steps**: While still PENDING_CLEARANCE, immediately send C1: `c` -> `1`.
- **Expected Result**: `central_log.txt`: `C1: CANCEL_OVERRIDE to 1 -> ACK`. The override is fully cancelled despite never having activated; once ped clearance finishes, NO override activates (contrast with TC-MSG-10 if not cancelled).

### TC-MSG-28: CANCEL_OVERRIDE with no override present - NACK UNKNOWN_TARGET
- **Type**: Negative
- **Verb**: MSG_CANCEL_OVERRIDE
- **Related**: NACK_REASON_UNKNOWN_TARGET
- **Environment**: (B) C1 + L1
- **Setup**: L1 in NORMAL_OPERATION, no override present.
- **Steps**: C1: `c` -> `1`.
- **Expected Result**: `central_log.txt`: `C1: CANCEL_OVERRIDE to 1 -> NACK reason=UNKNOWN_TARGET`.

### TC-MSG-29: Sending 2 near-simultaneous CANCEL_OVERRIDE to the same Lx (rapid duplicate / race)
- **Type**: Edge case
- **Verb**: MSG_CANCEL_OVERRIDE
- **Related**: idempotency and serialization under `fsm->lock` when 2 requests to the same target arrive nearly at the same time
- **Environment**: (D) - **mandatory**. `c_operator.c` reads the keyboard sequentially on a single thread (`c_operator_reader_thread`), so 2 presses of `c` are always separated by at least one `scanf` round - it's impossible to produce 2 truly simultaneous CANCEL_OVERRIDE messages from a single C1 process. Requires 2 independent test_client processes, each opening its own connection and sending `MsgSend()` at nearly the same instant (e.g. synchronized via a named semaphore/barrier) to the same L1.
- **Setup**: Override currently ACTIVE at L1.
- **Steps**: 2 test_client processes A and B, each sending `MSG_CANCEL_OVERRIDE{target_id=CTRL_L1}` within the same few-millisecond window (busy-wait to a shared time mark, or fire back-to-back without waiting for the reply if test_client supports async sends).
- **Expected Result**: Since `ipc_server_run()` processes each `MsgReceive()` sequentially on a single channel (no 2 server threads running in parallel at L1), exactly 1 of the 2 requests gets `RESULT_ACK` (whichever arrives first), the other gets `RESULT_NACK`/`NACK_REASON_UNKNOWN_TARGET` (the override was already cancelled by the other request) - no inconsistent state, no crash, no double-free/double-terminate of the override.

---

## 6. MSG_REQUEST_FAULT_CLEAR (C1 -> RLx / Lx)

Handled by `rlx_fsm_on_fault_clear()` at RLx. `NACK_REASON_UNKNOWN_TARGET`
if `state != RLX_FAULT`; if currently `RLX_FAULT`: `RESULT_ACK` if
`rlx_gate_poll_open()==1`, otherwise `NACK_REASON_FAULT_ACTIVE`.

**Update (test-plan finding, now fixed)**: this verb was originally
documented as C1->RLx only; `lx_fsm_local_fault_clear()` existed on the
Lx side but no verb/case ever called it, so sending
`MSG_REQUEST_FAULT_CLEAR` to an Lx used to return `RESULT_ERROR` (see
old TC-MSG-33). This has since been wired up: `lx_main.c`'s
`on_request()` now has a `MSG_REQUEST_FAULT_CLEAR` case calling
`lx_fsm_on_request_fault_clear()` (`lx_fsm.h`/`lx_fsm.c`), and
`c_operator.c`'s `handle_request_fault_clear()` asks for `node type`
(0=Lx, 1=RLx) before asking for the number, so the `f` key at C1 can now
target either node type. TC-MSG-33 below reflects the current behavior
instead of the old `RESULT_ERROR`.

**Important finding (a way around already exists via an existing key)**:
reading `enter_fault()` (`rlx_fsm.c`) closely shows that every path into
`RLX_FAULT` **driven by the FSM itself** calls
`rlx_gate_command_close()` (never `rlx_gate_command_open()`), and
`rlx_fsm_on_tick()`'s `RLX_FAULT` case is a no-op - so **considering
FSM-driven paths only**, `rlx_gate_poll_open()` can never become 1 while
in `RLX_FAULT`. However, `rlx_sensor.c` (the keyboard at RLx) already
has a key `r` (`case 'r'`, lines 43-45) that calls
`rlx_gate_force_confirmed_open()` (`rlx_gate.c`) directly - this
function sets `g_confirmed_open=1`/`g_confirmed_closed=0` immediately,
entirely independent of `fsm->state` (bypassing
`rlx_gate_command_open()`/the motion timer). This is an existing demo
escape hatch already in the repo, so the `RESULT_ACK` branch of this
verb **is reproducible** via ordinary keyboard action, with no need for
a tool or additional patch (see TC-MSG-32).

### TC-MSG-30: REQUEST_FAULT_CLEAR while RLx is not in FAULT - NACK UNKNOWN_TARGET
- **Type**: Negative
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: NACK_REASON_UNKNOWN_TARGET
- **Environment**: (B) C1 + RL1
- **Setup**: RL1 in `RLX_OPEN` (default at startup, no train, no fault).
- **Steps**: C1: `f` -> `RLx number (1-3): 1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 7 -> NACK reason=UNKNOWN_TARGET` (target_id printed is the numeric value of `CTRL_RL1`, =7 per `controller_id_t`).

### TC-MSG-31: REQUEST_FAULT_CLEAR while RLx is FAULT but the gate is not confirmed open - NACK FAULT_ACTIVE
- **Type**: Negative
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: NACK_REASON_FAULT_ACTIVE
- **Environment**: (B)/(C) C1 + RL1
- **Setup**: At RL1's sensor console: press `x` (arm demo gate-fail) RIGHT BEFORE pressing `0` (TRAIN_APPROACHING direction 0). Wait the full `RLX_WARNING_TO_CLOSING_MS` (5s) for RL1 to enter CLOSING, then wait a further `RLX_CLOSING_DEADLINE_MS` (15s from entering CLOSING) for `check_closing_or_reclosing_complete()` to detect the gate failed to confirm closed and call `enter_fault(FAULT_GATE_CONFIRM_MISSING)` (~20s total wait). Confirm via RLx log: gate "FAILED TO CONFIRM" then state transitions to FAULT.
- **Steps**: C1: `f` -> `1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 7 -> NACK reason=FAULT_ACTIVE` (because `enter_fault()` itself issued another close command - this time not armed to fail, so 3s later it will confirm CLOSED, not OPEN - `gates_confirmed_open()` is still 0).

### TC-MSG-32: REQUEST_FAULT_CLEAR -> ACK (the positive case) - reproducible via the existing `r` key at the RLx console
- **Type**: Positive
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: the "ACK" path of `rlx_fsm_on_fault_clear()` (condition: `state==RLX_FAULT` and `rlx_gate_poll_open()==1`); the `r` key in `rlx_sensor.c` (`case 'r'`, lines 43-45, calling `rlx_gate_force_confirmed_open()` in `rlx_gate.c`).
- **Environment**: (B)/(C) C1 + RL1 - no test_client or additional patch needed, the `r` key already exists in the repo.
- **Setup/Explanation**: Under the current code, **no path driven by `rlx_fsm.c` itself** ever sets `g_confirmed_open=1` while `fsm->state == RLX_FAULT`: `enter_fault()` (called from every route leading to FAULT - deadline miss during CLOSING/RECLOSING/OPENING, or a watchdog trip) always calls `rlx_gate_command_close()`, never `rlx_gate_command_open()`; and `rlx_fsm_on_tick()`'s `RLX_FAULT` case is a no-op. However `rlx_sensor.c` has the ready-made demo key `r`, independent of the FSM: it calls `rlx_gate_force_confirmed_open()` directly (`rlx_gate.c` lines 123-134), which locks `g_gate_lock`, sets `g_motion=GATE_IDLE`, `g_confirmed_closed=0`, `g_confirmed_open=1` **immediately** (no need to wait for `RLX_GATE_MOTION_MS`), then logs `"[DEMO] Gate mechanism simulated as physically repaired - now confirmed OPEN"`. Since this function does not check `fsm->state`, it can set `g_confirmed_open=1` regardless of whether RLx is in `RLX_FAULT` - creating exactly the condition needed for `rlx_gate_poll_open()==1` when `rlx_fsm_on_fault_clear()` is called.
- **Steps**: Enter FAULT as in TC-MSG-31 (RL1's sensor console: `x` -> `0`, wait ~20s for RL1's log to show FAULT) -> at RL1's sensor console press `r` (gate confirmed OPEN immediately, no further wait) -> at C1: `f` -> `1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 7 -> ACK`; RL1 returns to `RLX_OPEN`, `faults=FAULT_NONE`, occupancy windows cleared. This is a genuine `RESULT_ACK` outcome, fully confirmed by keyboard action on `rlx_sensor.c`/`c_operator.c` - no debugger, no test_client needed.

### TC-MSG-33: REQUEST_FAULT_CLEAR sent to an Lx that is FAULT_SAFE - ACK (fixed, no longer RESULT_ERROR)
- **Type**: Positive (formerly Edge case/Negative "RESULT_ERROR" - that behavior is now obsolete, see section 6's "Update")
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: `lx_fsm_on_request_fault_clear()` (`lx_fsm.c`) - now has its own case in `lx_main.c`'s `on_request()`, no longer falling into `default:`. `c_operator.c`'s `f` key asks for node type (0=Lx, 1=RLx) via `handle_request_fault_clear()`, so L1 can be targeted directly from the console without test_client.
- **Environment**: (B) C1 + L1 is enough for sending the command (no longer requires (D)/test_client for this basic case). **But the precondition (getting L1 into `SUPERVISORY_FAULT_SAFE`) currently has no reliable setup path** — see the explanation in TC-MSG-2 (no demo key, debugger not yet used, `kill -STOP` result undetermined) — so this case is **Skip**, not Pass, until one of those paths exists.
- **Setup**: Get L1 into `SUPERVISORY_FAULT_SAFE` (e.g. via a watchdog trip, see TC-SC03A-6 Part 1 in `02-state-machine-transition.md`), and ensure `last_crossing_state==CROSSING_OPEN` (no adjacent RLx currently pre-empting) so resume is expected to go to `NORMAL_OPERATION`.
- **Steps**: At C1: `f` -> node type `0` (Lx) -> Lx number `1`.
- **Expected Result**: `central_log.txt`: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`. `fsm->faults` back to `FAULT_NONE`, L1's SUPERVISORY leaves `FAULT_SAFE` for `NORMAL_OPERATION` (`3`). This function has no NACK branch (unconditional/idempotent, unlike `rlx_fsm_on_fault_clear()` - there is no physical state at Lx that needs re-verification).

### TC-MSG-33b: REQUEST_FAULT_CLEAR to an Lx that is FAULT_SAFE while the adjacent crossing is still closed - resumes RAILWAY_PREEMPTION, not NORMAL_OPERATION (re-audit fix, safety)
- **Type**: Positive (safety-relevant regression case)
- **Verb**: MSG_REQUEST_FAULT_CLEAR
- **Related**: `last_crossing_state` (`lx_fsm.h`) - re-audit fix: a fault-clear occurring while the adjacent crossing is still not `OPEN` must not be allowed to silently forget the active suppression, which would otherwise permit green toward a crossing that is still closed.
- **Environment**: (B) C1 + L1 + RL1 for sending the command. **Same precondition issue as TC-MSG-33** — requires a watchdog trip to enter `SUPERVISORY_FAULT_SAFE`, currently no reliable setup path — this case is **Skip**, not Pass.
- **Setup**: Put L1 into a real `RAILWAY_PREEMPTION` (RL1 in WARNING/CLOSED, sending a `CROSSING_STATUS` other than `CROSSING_OPEN` to L1), then trip the watchdog so L1 enters `FAULT_SAFE` while still pre-empting (SUPERVISORY goes directly `1 -> 0`, `fsm->last_crossing_state` keeps its latest non-OPEN value since `lx_fsm_on_crossing_status()` updates this field unconditionally, even while FAULT_SAFE).
- **Steps**: At C1: `f` -> `0` (Lx) -> `1`, **before** RL1 has a chance to report `OPEN` again.
- **Expected Result**: `C1: REQUEST_FAULT_CLEAR to 1 -> ACK`, but L1's SUPERVISORY afterward must be `RAILWAY_PREEMPTION` (`1`), **not** `NORMAL_OPERATION` (`3`) - CONNECTOR_GREEN (toward the crossing) remains suppressed until L1 actually receives `CROSSING_STATUS(OPEN)` from RL1. This is the CORRECT behavior under the new design (before the fix, the old code always resumed `NORMAL_OPERATION` unconditionally, potentially allowing green toward a still-closed crossing).

---

## 7. MSG_STATUS (Lx/RLx -> C1)

`c_main.c`'s `on_request()` handles `MSG_STATUS` exactly like
`MSG_HEARTBEAT` (always `RESULT_ACK`, calls `c_server_record_status()`,
no NACK branch). **Important finding**: a full scan of `lx_comm.c` and
`rlx_comm.c` confirms **no function ever sends `MSG_STATUS`** - both
files only have `*_send_heartbeat()`, and (RLx) `*_send_fault_report()`,
`*_broadcast_crossing_status_if_changed()`. This verb is defined, C1 is
ready to handle it, but **no sender exists anywhere in the current
codebase**. So every test case for this verb requires test_client.

### TC-MSG-34: Valid MSG_STATUS to C1 - ACK
- **Type**: Positive
- **Verb**: MSG_STATUS
- **Related**: (no nack - and no real sender in the code, see note above)
- **Environment**: (D) - mandatory, since nothing in the current codebase generates this verb.
- **Setup**: `c_main` running.
- **Steps**: test_client sends `ipc_request_t{verb=MSG_STATUS, sender_id=CTRL_L2, target_id=CTRL_C1, payload.status={role=ROLE_INTERSECTION, mode=MODE_PEAK_FIXED, signal_phase=PHASE_ARTERIAL_GREEN, supervisory_state=SUPERVISORY_NORMAL_OPERATION, ...}}` to C1.
- **Expected Result**: reply `result=RESULT_ACK`. `c_mode_eng_t.controllers[idx]` (idx corresponding to CTRL_L2) is updated correctly with the `last_reported_*` fields; `missed_heartbeat_ticks` and `marked_unavailable` reset to 0 (same side effect as receiving HEARTBEAT/CROSSING_STATUS - see `c_server_record_status()`).

### TC-MSG-35: MSG_STATUS with role not matching the real sender_id - still ACK (edge case robustness)
- **Type**: Edge case
- **Verb**: MSG_STATUS
- **Related**: no cross-validation between `sender_id` and `payload.status.role` in `c_server_record_status()`
- **Environment**: (D)
- **Setup**: Same as TC-MSG-34.
- **Steps**: test_client sends `MSG_STATUS` with `sender_id=CTRL_L2` but `payload.status.role=ROLE_RAILWAY` (deliberately wrong).
- **Expected Result**: reply is still `result=RESULT_ACK` (C1 does not check that the `role` field matches the sender_id - it only uses `sender_id` to look up `c_mode_eng_controller_index()`). This is CORRECT for the current code (not a bug per se), but worth noting: the HMI (`c_hmi.c`) could display incorrectly if a node misreports its own role - additional validation is recommended if defending against a buggy/spoofed node is desired.

---

## 8. MSG_FAULT_REPORT (RLx -> C1)

Sent by `rlx_comm_send_fault_report()`, triggered exactly when
`rlx_fsm_take_fault_report_pending()` returns 1 (i.e. the moment
`enter_fault()` runs). C1's `on_request()` is always `RESULT_ACK`, no
NACK. `c_server_record_fault_report()` is currently a **complete
no-op** (only `c_logger_log()` in `c_main.c` actually prints anything).

### TC-MSG-36: MSG_FAULT_REPORT when RLx genuinely enters FAULT - ACK
- **Type**: Positive
- **Verb**: MSG_FAULT_REPORT
- **Related**: (no nack)
- **Environment**: (B)/(C) C1 + RL1
- **Setup**: Same setup as TC-MSG-31 (press `x` then `0` at RL1, wait ~20s to enter FAULT_GATE_CONFIRM_MISSING).
- **Steps**: No further action needed at C1 - RLx automatically sends `MSG_FAULT_REPORT` as soon as it enters FAULT (in the same armed `IPC_PULSE_RAILWAY_WARNING` tick, see `rlx_main.c`'s `on_pulse()`).
- **Expected Result**: `central_log.txt` has a line printed directly by `c_main.c`: `FAULT_REPORT from 7: fault_code=0x00000001 severity=1 detail="RLx fault - see fault_code bitmask"` (fault_code = `FAULT_GATE_CONFIRM_MISSING` = bit 0 = 0x1). No NACK line at all (this verb is always ACK).

### TC-MSG-37: MSG_FAULT_REPORT with detail[64] lacking a null-terminator - memory safety check (edge case robustness)
- **Type**: Edge case
- **Verb**: MSG_FAULT_REPORT
- **Related**: string-handling safety in `c_logger_log()`/`printf("%s", ...)` at `c_main.c` when receiving a deliberately malformed payload
- **Environment**: (D) - `rlx_comm_send_fault_report()` always `strncpy`s and forces the last byte to `\0`; no path in real code produces a string missing a null-terminator - must be a test_client deliberately violating this.
- **Setup**: `c_main` running.
- **Steps**: test_client sends `ipc_request_t{verb=MSG_FAULT_REPORT, sender_id=CTRL_RL2, target_id=CTRL_C1, payload.fault_report={fault_code=0xFF, severity=9, detail=<64 bytes all 'A', WITHOUT a trailing '\0'>}}`.
- **Expected Result**: the `c_main` process does NOT crash/segfault (memory safety), reply `result=RESULT_ACK` is still returned normally. Separate note: since `req->payload.fault_report.detail` sits inside a larger struct (`ipc_request_t`), `printf("%s", ...)` when missing `\0` may read across into the next field in process memory until it hits a random zero byte - the printed log line may contain garbage but the process must never crash. If a crash is observed, this is a memory-safety vulnerability that must be reported (the Auditor/Verifier agent should mark it FAIL).

---

## 9. MSG_CROSSING_STATUS (RLx -> adjacent Lx; RLx -> C1)

Sent by `rlx_comm_broadcast_crossing_status_if_changed()`, ONLY when
the `crossing_state_t` derived from internal state actually changes
compared to the last send (deduped via the static variable
`last_broadcast_state`). Receivers (both
`lx_fsm_on_crossing_status()` and C1's `on_request()`) are always
`RESULT_ACK` - RC-02: "Lx only observes, never rejects".

### TC-MSG-38: CROSSING_STATUS(WARNING) to the adjacent Lx - ACK, Lx enters RAILWAY_PREEMPTION
- **Type**: Positive
- **Verb**: MSG_CROSSING_STATUS
- **Related**: (no nack)
- **Environment**: (B)/(C) L1 + RL1 (C1 not required to test RLx<->Lx alone, but recommended so the log is clear)
- **Setup**: L1, RL1 started, L1 in NORMAL_OPERATION.
- **Steps**: At RL1: press `0` (TRAIN_APPROACHING) -> RL1 goes OPEN->WARNING, automatically sending `MSG_CROSSING_STATUS{state=CROSSING_WARNING}` to L1, L2, C1.
- **Expected Result**: L1 replies `result=RESULT_ACK`; `L1.supervisory` becomes `SUPERVISORY_RAILWAY_PREEMPTION` (if a CENTRAL_OVERRIDE was running beforehand, it is safely cancelled first - see `lx_fsm_on_crossing_status()`).

### TC-MSG-39: CROSSING_STATUS to C1 - ACK, C1 updates last_reported_crossing_state
- **Type**: Positive
- **Verb**: MSG_CROSSING_STATUS
- **Related**: (no nack)
- **Environment**: (B)/(C) C1 + RL1
- **Setup**: Same as TC-MSG-38.
- **Steps**: Observe C1 receiving `MSG_CROSSING_STATUS` at the same time RL1 sends it to L1/L2.
- **Expected Result**: C1's `on_request()` case `MSG_CROSSING_STATUS` returns `result=RESULT_ACK`; `c_server_record_crossing_status()` updates `last_reported_crossing_state=CROSSING_WARNING` for RL1's index, also resetting `missed_heartbeat_ticks=0`/`marked_unavailable=0`.

### TC-MSG-40: No repeated CROSSING_STATUS sent when the state does not change (dedup - edge case)
- **Type**: Edge case
- **Verb**: MSG_CROSSING_STATUS
- **Related**: the `last_broadcast_state` mechanism in `rlx_comm_broadcast_crossing_status_if_changed()`
- **Environment**: (B)/(C) C1 + RL1 + L1
- **Setup**: RL1 currently in `RLX_WARNING` (already sent CROSSING_WARNING once, as in TC-MSG-38/39).
- **Steps**: Take no further action - let RL1 tick on its own (`IPC_PULSE_RAILWAY_WARNING`, every 1s) for a few seconds while its internal state still maps to `CROSSING_WARNING` (`RLX_WARNING`/`RLX_CLOSING`/`RLX_RECLOSING` all map to the same wire value - see `map_to_crossing_state()`). E.g. wait for RL1 to transition itself from `RLX_WARNING` to `RLX_CLOSING` after 5s (`RLX_WARNING_TO_CLOSING_MS`) - this is an INTERNAL state change but NOT a wire-level state change.
- **Expected Result**: Logs at L1/C1 show exactly 1 line receiving `MSG_CROSSING_STATUS(WARNING)` (from the OPEN->WARNING transition) - **no** further line when RL1 internally transitions WARNING->CLOSING, even though RL1 ticks every second and calls `rlx_comm_broadcast_crossing_status_if_changed()` each time. Only once the gate is genuinely confirmed closed (transitioning to `RLX_CLOSED`, mapping to `CROSSING_CLOSED` - different from `CROSSING_WARNING`) does a second CROSSING_STATUS line appear.

### TC-MSG-41: CROSSING_STATUS with state outside the valid enum range - still ACK, treated as "not OPEN" (edge case robustness)
- **Type**: Edge case
- **Verb**: MSG_CROSSING_STATUS
- **Related**: no range validation for the `state` field in `lx_fsm_on_crossing_status()` (only compares `!= CROSSING_OPEN`)
- **Environment**: (D) - nothing in `rlx_comm.c` produces a `state` value outside the 4 valid values of `crossing_state_t` (0-3); must be a test_client-crafted payload.
- **Setup**: L1 in NORMAL_OPERATION.
- **Steps**: test_client sends `ipc_request_t{verb=MSG_CROSSING_STATUS, sender_id=CTRL_RL1, target_id=CTRL_L1, payload.crossing_status={state=999}}`.
- **Expected Result**: reply `result=RESULT_ACK` (RC-02: Lx never rejects this verb regardless of content). Since `999 != CROSSING_OPEN(0)`, L1 processes it exactly as "not OPEN" -> transitions to `SUPERVISORY_RAILWAY_PREEMPTION`. This is CORRECT behavior under the current code, but worth noting: there is no mechanism to detect a garbage enum value from a malfunctioning node.

---

## 10. MSG_HEARTBEAT (Lx/RLx -> C1)

Sent automatically every 1000ms by both `lx_comm_send_heartbeat()` and
`rlx_comm_send_heartbeat()` (armed timer `IPC_PULSE_HEARTBEAT_TICK`, no
keyboard action needed for this to occur). C1's `on_request()` is
always `RESULT_ACK`. PA-07: 3 consecutive ticks (1 tick/second at C1's
`IPC_PULSE_HEARTBEAT_TICK`, see `c_watchdog_mon.c`) with no
proof-of-life (STATUS/HEARTBEAT/CROSSING_STATUS) from a controller ->
`marked_unavailable=1`.

### TC-MSG-42: Periodic 1Hz HEARTBEAT from Lx - continuous ACK
- **Type**: Positive
- **Verb**: MSG_HEARTBEAT
- **Related**: (no nack)
- **Environment**: (A)/(B) only needs C1 + 1 Lx (e.g. L1) running together, no action needed
- **Setup**: Start `c_main`, `lx_main 1`.
- **Steps**: Take no action - just wait and observe for >= 5 seconds.
- **Expected Result**: `c_main`'s `on_request()` case `MSG_HEARTBEAT` returns `RESULT_ACK` on every receipt (no dedicated ACK log line on the HEARTBEAT path in `c_main.c` - confirmed indirectly via `c_mode_eng.controllers[idx].missed_heartbeat_ticks` staying at 0 and the HMI (`c_hmi_render()`, running at 1Hz) showing L1 as "alive"/not marked unavailable).

### TC-MSG-43: Missing 3 consecutive heartbeats - Lx marked unavailable (PA-07, edge case)
- **Type**: Edge case
- **Verb**: MSG_HEARTBEAT
- **Related**: PA-07 (`c_watchdog_mon_tick()`, 3-tick threshold in `c_mode_eng.h`'s comment)
- **Environment**: (B)/(C) C1 + L1
- **Setup**: `c_main`, `lx_main 1` running stably (HEARTBEAT already observed arriving regularly).
- **Steps**: Abruptly stop the `lx_main 1` process (Ctrl+C or `slay lx_main`/`kill`) to simulate a total connection loss. Wait >= 3 seconds (3 ticks of `IPC_PULSE_HEARTBEAT_TICK` at C1).
- **Expected Result**: After exactly 3 ticks with no HEARTBEAT/STATUS/CROSSING_STATUS received from CTRL_L1, `c_mode_eng.controllers[idx].marked_unavailable` flips from 0 to 1 (edge-triggered, logs "went stale" only once); `c_hmi_render()` (running every second) shows L1 as unavailable/disconnected starting from the next render.

### TC-MSG-44: HEARTBEAT from RLx - role=ROLE_RAILWAY, mode/signal_phase=0 not misinterpreted
- **Type**: Positive (checks correct semantics of fields shared between Lx/RLx)
- **Verb**: MSG_HEARTBEAT
- **Related**: (no nack) - `status_report_payload_t` is shared by both roles, `mode`/`signal_phase`/`supervisory_state` are "meaningful for ROLE_INTERSECTION" only (comment in `ipc_msg.h`)
- **Environment**: (B)/(C) C1 + RL1
- **Setup**: `c_main`, `rlx_main 1` running, RL1 in `RLX_OPEN`.
- **Steps**: Take no action - wait for the automatic HEARTBEAT (1Hz).
- **Expected Result**: reply `RESULT_ACK`. The received payload has `role=ROLE_RAILWAY`, `crossing_state=CROSSING_OPEN(0)`, while `mode=0`, `signal_phase=0`, `supervisory_state=0` (default memset values, with NO meaning for RLx - `rlx_fsm_fill_status()` only fills in `role`/`crossing_state`/`faults`). Confirm `c_mode_eng.controllers[idx].last_reported_mode` for RL1 = 0 is NOT misread by the HMI as a meaningful `MODE_PEAK_FIXED` (only role-appropriate fields should be displayed).

### TC-MSG-45: HEARTBEAT with a role field outside the valid enum range - still ACK (negative/robustness)
- **Type**: Negative
- **Verb**: MSG_HEARTBEAT
- **Related**: no validation of `payload.heartbeat.summary.role`/other enum fields at `c_main.c`
- **Environment**: (D) - nothing in `lx_comm.c`/`rlx_comm.c` produces a role outside `{ROLE_CENTRAL, ROLE_INTERSECTION, ROLE_RAILWAY}`; must be test_client-crafted.
- **Setup**: `c_main` running.
- **Steps**: test_client sends `ipc_request_t{verb=MSG_HEARTBEAT, sender_id=CTRL_L4, target_id=CTRL_C1, payload.heartbeat.summary={role=99, ...}}`.
- **Expected Result**: reply is still `result=RESULT_ACK` (C1 never NACKs any content of HEARTBEAT/STATUS/CROSSING_STATUS/FAULT_REPORT - these 4 "reporting" verbs are always unconditionally ACK, differing only in whether they get recorded into `c_mode_eng_t`). Baseline behavior worth knowing: QA should not expect any NACK from C1 for these 4 reporting verbs, no matter how malformed the payload - any validation (if desired) would have to be added to `c_server.c`, which currently does not exist.

---

## Appendix: Notable findings from code review (not test cases, but directly affecting testability)

1. **`NACK_REASON_PEDESTRIAN_ACTIVE` is dead code.** Declared in
   `sys_types.h` with the comment "pedestrian clearance active and
   cannot be safely deferred", and has a display name in both
   `c_comm.c` and `c_operator.c`'s `nack_reason_name()`. But a full
   grep of `app/` shows **no `lx_fsm_on_*()` ever assigns this value** -
   the case of a running ped-clearance colliding with a
   `REQUEST_OVERRIDE` is handled via `RESULT_ACK_PENDING` (see
   TC-MSG-10), not a NACK. This is one of 2 possibilities: (a) the
   comment in `sys_types.h` describes an older design that no longer
   matches the code, or (b) this value is reserved for a different
   situation not yet implemented (e.g. another verb that could also
   conflict with ped clearance but has no "wait" mechanism like
   override does). Suggestion: the Compliance Agent should cross-check
   against the original documentation (SD-07/UC-08) to confirm whether
   this is API drift needing a comment fix, or a still-missing code
   branch.

2. **`lx_sensor.c` lacks a manual fault-trigger demo key**, unlike
   `rlx_sensor.c` (which already has `x` to arm a gate-fail and `f` to
   self-clear a local fault). Consequence: **no test case involving
   `NACK_REASON_FAULT_ACTIVE` on Lx** (SET_TIMING_PROFILE, SET_MODE,
   REQUEST_OVERRIDE) can be run via ordinary keyboard action - it must
   wait for a real PA-10 watchdog trip (server thread genuinely hung
   for >= 2s) or additional code. Recommend the Core-Engineer add a
   DEMO-ONLY key calling `lx_fsm_report_watchdog_trip(&fsm)` to
   `lx_sensor.c`, matching the existing RLx pattern.

3. **`MSG_STATUS` is defined and C1 is ready to handle it, but no
   sender exists anywhere in the codebase (`lx_comm.c`, `rlx_comm.c`)
   that ever sends this verb.** All real status reporting goes through
   `MSG_HEARTBEAT` (which shares `status_report_payload_t` inside
   `heartbeat_payload_t`). Needs clarification with the Compliance
   Agent: is this leftover scope creep (`MSG_STATUS` reserved for a
   separate UC-09 flow never wired up) or is the handler at C1
   unnecessary dead code?

4. **`RESULT_ACK` for `MSG_REQUEST_FAULT_CLEAR` has been confirmed
   achievable on the current build, via the existing `r` key in
   `rlx_sensor.c`** (see TC-MSG-32). It is true that every path into
   `RLX_FAULT` **driven by `rlx_fsm.c` itself** automatically commands
   the gate closed (`rlx_gate_command_close()`), with no FSM-driven
   path ever opening the gate while FAULT - but `rlx_sensor.c`'s
   `case 'r'` calls `rlx_gate_force_confirmed_open()` (`rlx_gate.c`)
   directly, an FSM-independent demo escape hatch that sets
   `g_confirmed_open=1` regardless of `fsm->state`. So RC-09/RC-10's
   "operator request fault clearance after repair" flow **is fully
   reproducible via keyboard action** (RL1's sensor console: `x` ->
   `0` -> wait to enter FAULT -> `r`; C1: `f` -> `1`), with no
   debugger, no test_client, and no additional Core-Engineer patch
   needed for this protocol-testing purpose. The related RC-09 case can
   be considered testable (no longer BLOCKED) at step 6 (QA-Test).

5. **`c_server_record_fault_report()` is a complete no-op** (only the
   direct `c_logger_log()` call from `c_main.c` prints anything) -
   `c_mode_eng_t` has no long-term fault history storage for RLx. Does
   not block protocol testing (the verb still ACKs correctly), but
   affects the UC-09/HMI test plan (out of scope for this document).

6. **The 4 "reporting" verbs** (MSG_STATUS, MSG_HEARTBEAT,
   MSG_FAULT_REPORT, MSG_CROSSING_STATUS when received at C1, and
   MSG_CROSSING_STATUS/MSG_HEARTBEAT when received at Lx) **never
   return NACK** anywhere in the current codebase - `result` can only
   be `RESULT_ACK` (correct route) or `RESULT_ERROR` (verb sent to a
   node that doesn't support it, e.g. TC-MSG-33). All 8 `nack_reason_t`
   values in `sys_types.h` are only ever actually used by the 5
   "command" verbs (SET_TIMING_PROFILE, SET_MODE, REQUEST_OVERRIDE,
   RENEW_OVERRIDE, CANCEL_OVERRIDE, REQUEST_FAULT_CLEAR) - and among
   those, `NACK_REASON_PEDESTRIAN_ACTIVE` is still never used anywhere
   (Appendix item 1).
