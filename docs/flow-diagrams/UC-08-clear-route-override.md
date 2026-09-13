# UC-08 — Apply a Bounded Clear-Route Override: Function-Level Flow

## What this feature does

Per `usecase.md` section 3.2.8 (UC-08), the Control Room Operator can temporarily
prioritise a chosen through-movement ("clear route") at one target intersection
(Lx) without ever taking output authority away from that intersection's own
controller. The override must be revalidated locally against the current
conflict matrix, fault state, and pedestrian clearance (BR-3/`PA-09`); it is
always bounded and auto-expiring, and a renewal is independently re-validated
without disturbing the existing expiry if it fails (BR-5/BR-7, `PA-11`); and a
request that arrives behind an in-progress pedestrian clearance is still
`ACK`'d within the standard command-response deadline even though its
*activation* is deferred (BR-6, `PA-12`).

## Entry point

The lifecycle begins at the operator console in `app/central/src/c_operator.c`,
`handle_request_override()` (around line 218), wired to the `'o'` key in
`c_operator_reader_thread()`'s switch (around line 488). The same file's
`handle_renew_override()` (around line 296, key `'r'`, around line 493) and
`handle_cancel_override()` (around line 334, key `'c'`, around line 498)
continue this same override lifecycle later on — renewal before expiry, or
operator-initiated cancellation — rather than starting a new one.

## Function call chain

1. **Central console thread — collect and pre-check the request.**
   `c_operator_reader_thread()` reads the `'o'` keypress, locks
   `console_io_lock` (`c_operator.c` around line 489), and calls
   `handle_request_override()` (line 218). It prompts for the target Lx
   (1-6), `target_movement` (0=arterial, 1=connector), and `duration_ms`,
   then locks `mode_eng_lock` (line 254 — held only for the validate/
   bookkeeping step, an inner lock nested under the already-held
   `console_io_lock`) and calls
   `c_mode_eng_validate_override_request()` in `c_mode_eng.c` (line 141).

2. **Central's own surface-level pre-check.**
   `c_mode_eng_validate_override_request()` (`c_mode_eng.c` line 141) checks
   only what Central itself can judge without asking the Lx: the target is a
   real `CTRL_L1..CTRL_L6` (else `NACK_REASON_UNKNOWN_TARGET`), `duration_ms`
   is in `(0, 300000]` — the PA-11 duration cap, checked a second time,
   authoritatively, on the Lx side in step 4 — (else
   `NACK_REASON_INVALID_DURATION`), and `override_type ==
   OVERRIDE_CLEAR_ROUTE` (else `NACK_REASON_OUT_OF_RANGE`). If this rejects,
   `handle_request_override()` logs the rejection and returns without ever
   calling into `c_comm.c` — per its own doc comment, a request Central
   rejects outright is never forwarded to the Lx at all. If accepted, Central
   optimistically marks `override_in_flight = 1` for that controller (still
   under `mode_eng_lock`, line 254-273) purely as its own bookkeeping so a
   second `'o'` on the same target isn't issued blindly; this flag is not
   synchronised with the Lx's real answer.

3. **Central console thread — send the wire request.**
   Still inside `handle_request_override()`, Central calls
   `c_comm_send_request_override()` in `c_comm.c` (line 191), which fills an
   `ipc_request_t` with `verb = MSG_REQUEST_OVERRIDE` and a
   `request_override_payload_t{override_type, target_movement, duration_ms}`
   and hands it to `ipc_client_post()` (non-blocking enqueue — see
   `app/shared/src/qnet_utils.c` line 365). Per `app/shared/README.md`'s
   "Threading pattern", the console thread never itself calls `MsgSend()`;
   the actual send happens later on Central's dedicated **client thread**
   (`ipc_client_thread_main()`), keeping the console responsive.

4. **Target Lx — server thread dispatch.**
   The Lx's single server thread blocks in `ipc_server_run()`
   (`app/shared/src/qnet_utils.c` line 278) on `MsgReceive()`. The inbound
   `MSG_REQUEST_OVERRIDE` is delivered to `on_request()` in
   `app/intersection/src/lx_main.c` (line 37), whose `switch` (case at line
   48) dispatches to `lx_fsm_on_request_override(&ctx->fsm,
   &req->payload.override_request, reply)` in `app/intersection/src/lx_fsm.c`
   (line 734). This is the **authoritative** validator — the wire contract
   never guarantees the sender is a well-behaved operator, so every check
   Central already made at step 2 is independently re-run here, plus checks
   Central cannot make.

5. **Lx FSM — authoritative validation (still on the server thread), under `fsm->lock`.**
   `lx_fsm_on_request_override()` (line 734) takes `fsm->lock`, runs
   `lx_fsm_check_fault_locked()`, then rejects with `NACK` in order for:
   an override already active/pending on this Lx (`supervisory ==
   SUPERVISORY_CENTRAL_OVERRIDE`, line 740 — extending must go through
   `RENEW_OVERRIDE`, not a second `REQUEST_OVERRIDE`); `duration_ms == 0` or
   `> LX_OVERRIDE_DURATION_CAP_MS` (line 755 — PA-11's `(0, 300000]` cap,
   re-checked authoritatively here, independent of Central's step-2 check);
   an out-of-range `target_movement` (line 759); an active railway
   pre-emption (`SUPERVISORY_RAILWAY_PREEMPTION`, line 773 —
   `NACK_REASON_RAILWAY_CONFLICT`, CC-02); or `FAULT_SAFE` (line 778).

6. **Deferral branch — pedestrian clearance in progress (`ped_clearance_active`).**
   If none of the above reject and `fsm->ped_clearance_active` is true (line
   781), the FSM does **not** truncate the running WALK/FLASHING_DONT_WALK
   sequence (UC-08 alt-flow 3.1.1). Instead it sets
   `override_substate = OVR_PENDING_CLEARANCE`, records
   `override_target_movement`/`override_duration_ms`/`override_remaining_ms`,
   sets `supervisory = SUPERVISORY_CENTRAL_OVERRIDE`, and replies
   `RESULT_ACK_PENDING` (lines 791-796) — this is UC-08's `ACK` for "accepted
   either for execution or for safe deferral" (BR-6/PA-12: the reply still
   goes out within the standard deadline; only activation waits).

7. **Immediate-activation branch — no pedestrian conflict.**
   Otherwise (`else`, line 797), the FSM sets `override_substate =
   OVR_ACTIVE` directly, with the same bookkeeping, and replies
   `RESULT_ACK` (lines 798-803). `fsm->lock` is released at line 805 either
   way.

8. **Countdown and re-validation on every phase tick (server thread, under `fsm->lock`).**
   The Lx's timer pulse (`IPC_PULSE_PHASE_TIMER`) is delivered through the
   same `ipc_server_run()` loop to `on_pulse()` (`lx_main.c` line 80), which
   calls `lx_fsm_on_phase_timer()` (`lx_fsm.c` line 931) — so request/renew/
   cancel handling and the tick handler run serialized on one server thread,
   each additionally taking `fsm->lock`. Inside it:
   - Lines 970-977: for either `OVR_ACTIVE` or `OVR_PENDING_CLEARANCE`,
     `override_remaining_ms` is decremented by `LX_PHASE_TICK_MS` each tick;
     when it would reach zero, `lx_fsm_terminate_override_locked()` (line
     205) fires immediately — this is how a still-pending request can expire
     before ever activating (UC-08 alt-flow covers this).
   - Line 985: `lx_fsm_ped_service_tick_locked()` runs next and is what
     actually clears `ped_clearance_active` once WALK+FLASHING_DONT_WALK
     finishes.
   - Lines 1010-1014: **immediately after**, if still
     `OVR_PENDING_CLEARANCE` and `ped_clearance_active` has just dropped,
     the FSM flips `override_substate` straight to `OVR_ACTIVE` — this is
     the re-validation UC-08 alt-flow 3.1.4 describes; because any event
     that would newly invalidate the override (railway conflict, fault)
     already evicts it out of `SUPERVISORY_CENTRAL_OVERRIDE` proactively
     elsewhere, "revalidate" here reduces to just activating it.
   - `lx_fsm_advance_phase_locked()` (line 228), called from the phase
     switch below once a yellow/all-red boundary is reached, is what makes
     `OVR_ACTIVE` concrete: at each `ALL_RED` boundary (line 244 for
     A-to-B) it forces the next green to `override_target_movement` instead
     of letting the ordinary cycle pick it — never skipping yellow/all-red
     clearance, only steering which green comes next.
   - In the `PHASE_ARTERIAL_GREEN` (line 1032) and `PHASE_CONNECTOR_GREEN`
     (line 1079) cases of the same `switch`, if the override is `OVR_ACTIVE`
     and its `override_target_movement` matches the current green, the
     ordinary PEAK_FIXED/OFF_PEAK_SENSOR exit-check is skipped entirely (the
     `break` at lines 1040/1087) — the override holds that green until
     `override_remaining_ms` (decremented above) ends it, not until the
     normal cycle would have.

9. **Renewal (Central console thread, then Lx server thread, under `fsm->lock`).**
   `'r'` invokes `handle_renew_override()` (`c_operator.c` line 296), which
   does *not* pre-validate (per BR-7 that job belongs entirely to the Lx) and
   calls `c_comm_send_renew_override()` (`c_comm.c` line 210,
   `MSG_RENEW_OVERRIDE`). On the Lx, `on_request()`'s case at line 51
   dispatches to `lx_fsm_on_renew_override()` (`lx_fsm.c` line 808), which
   `NACK`s (`NACK_REASON_UNKNOWN_TARGET`) unless currently `OVR_ACTIVE` (line
   814), `NACK`s (`NACK_REASON_INVALID_DURATION`) if the extension exceeds
   `LX_OVERRIDE_DURATION_CAP_MS` (line 817) — leaving the existing expiry
   untouched on either rejection — and otherwise recomputes
   `override_duration_ms`/`override_remaining_ms` (0 means "keep original
   duration") and replies `RESULT_ACK`, restarting the countdown timer
   (lines 822-828).

10. **Termination — cancellation (Central console thread, then Lx server thread).**
    `'c'` invokes `handle_cancel_override()` (`c_operator.c` line 334),
    clears Central's own `override_in_flight` bookkeeping, and calls
    `c_comm_send_cancel_override()` (`c_comm.c` line 226, `MSG_CANCEL_OVERRIDE`,
    no payload — envelope's `target_id` fully identifies it). On the Lx,
    `on_request()`'s case at line 54 dispatches to
    `lx_fsm_on_cancel_override()` (`lx_fsm.c` line 833), which `NACK`s
    unless `override_substate` is `OVR_PENDING_CLEARANCE` or `OVR_ACTIVE`
    (line 839), otherwise calls `lx_fsm_terminate_override_locked()` (line
    843) and replies `RESULT_ACK`.

11. **Termination — expiry or cancellation, shared cleanup.**
    `lx_fsm_terminate_override_locked()` (`lx_fsm.c` line 205), reached
    either from the phase-timer countdown (step 8) or directly from cancel
    (step 10), calls `lx_signal_show_override_clearance()`, resets
    `override_substate = OVR_NONE` and `override_remaining_ms = 0`, and — if
    still `SUPERVISORY_CENTRAL_OVERRIDE` — restores `supervisory =
    SUPERVISORY_NORMAL_OPERATION` (lines 211-216), letting the ordinary
    phase cycle and mode (PEAK_FIXED/OFF_PEAK_SENSOR) resume on the ordinary
    ALL_RED boundary that follows.

## Cross-node view

Three IPC verbs (`app/shared/includes/ipc_msg.h`, around lines 66-68) carry
this entire lifecycle across the Central <-> Lx boundary:
`MSG_REQUEST_OVERRIDE`, `MSG_RENEW_OVERRIDE`, and `MSG_CANCEL_OVERRIDE` — all
Central-to-Lx, all handled synchronously with an `ipc_reply_t`
(`RESULT_ACK` / `RESULT_ACK_PENDING` / `RESULT_NACK`) per the QNX
`MsgSend`/`MsgReceive`/`MsgReply` pattern (see `app/shared/src/qnet_utils.c`'s
`ipc_server_run()`, line 278, and `ipc_client_post()`, line 365). The most
detailed diagram of exactly this exchange in this repo is **SD-07 — Validate
and Apply a Bounded Clear-Route Override** in `SEQUENCE_DIAGRAMS.md` (section
4.2.7, around lines 358-478), which shows the operator/Central/Lx/pedestrian-
sequencer/railway-input/signal-output actors, the pending-clearance branch,
the renewal loop, and the two termination paths (expiry vs. cancel vs.
safety-interrupt) that the function trace above implements.

## System-level summary diagram

```mermaid
sequenceDiagram
    autonumber
    actor Op as Operator (c_operator.c)
    participant C1 as Central (c_comm.c)
    participant Lx as Target Lx (lx_fsm.c)

    Op->>C1: 'o' handle_request_override()
    C1->>C1: c_mode_eng_validate_override_request() (PA-11 surface check)
    C1->>Lx: MSG_REQUEST_OVERRIDE(CLEAR_ROUTE, target_movement, duration_ms)
    Lx->>Lx: lx_fsm_on_request_override() (authoritative check)

    alt pedestrian clearance active, safely deferrable
        Lx-->>C1: ACK_PENDING (OVR_PENDING_CLEARANCE)
        Note over Lx: lx_fsm_on_phase_timer() waits for<br/>ped_clearance_active to drop, then<br/>flips to OVR_ACTIVE
    else no conflict
        Lx-->>C1: ACK (OVR_ACTIVE)
    else unsafe / conflict / fault
        Lx-->>C1: NACK(reason)
    end

    Note over Lx: OVR_ACTIVE holds override_target_movement's<br/>green at each ALL_RED boundary;<br/>override_remaining_ms counts down each tick

    opt operator renews before expiry
        Op->>C1: 'r' handle_renew_override()
        C1->>Lx: MSG_RENEW_OVERRIDE(extend_duration_ms)
        Lx->>Lx: lx_fsm_on_renew_override()
        alt valid
            Lx-->>C1: ACK (timer restarted)
        else invalid
            Lx-->>C1: NACK (expiry unchanged)
        end
    end

    alt operator cancels
        Op->>C1: 'c' handle_cancel_override()
        C1->>Lx: MSG_CANCEL_OVERRIDE
        Lx->>Lx: lx_fsm_on_cancel_override() -> lx_fsm_terminate_override_locked()
        Lx-->>C1: ACK
    else override_remaining_ms reaches 0
        Lx->>Lx: lx_fsm_terminate_override_locked() (phase-timer driven)
    end

    Note over Lx: supervisory -> SUPERVISORY_NORMAL_OPERATION
```
