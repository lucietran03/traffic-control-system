# UC-06 — Respond to a Railway Equipment Fault: Function-Level Flow

## What this feature does

Per `usecase.md` section 3.2.6 (`UC-06 — Respond to a Railway Equipment Fault`), when a railway
controller (RLx) cannot confirm that its gates reached the required safe state, it must force the
train signal to `STOP` and hold gates/flashers in a safe state *locally and immediately*, without
waiting on Central (`RC-06`). That local safety response and the report of the fault to Central are
required to be independent, unsequenced actions (`RC-10`, `PA-09`). Central may later ask the
faulted RLx to re-verify and clear the fault, but Central itself is never allowed to actuate railway
equipment directly (`RC-09`); the RLx alone decides ACK (verified safe) or NACK (still unsafe,
`PA-10` — the fault stays local and does not cascade to unrelated locations).

## Entry point

This use case has two independent entry points in the real source:

- **(a) Fault detection** — the internal `enter_fault()` helper in
  `app/railway/src/rlx_fsm.c` (around line 106). It is not part of the public API; it is invoked
  only from inside `rlx_fsm.c` itself, from three call sites (see chain below).
- **(b) Operator-initiated clearance** — the `'f'` keypress handled in the operator command loop in
  `app/central/src/c_operator.c` (around line 503), which dispatches to
  `handle_request_fault_clear()` (around line 366).

## Function call chain

### (a) Fault containment — local safe state and Central report run in PARALLEL (RC-10)

Both branches below are triggered by the **same** `enter_fault()` call — the code does not
sequence one after the other; `enter_fault()` performs the local safety actions synchronously
under `fsm->lock`, and merely raises a flag (`fault_report_pending`) that a *different* thread
picks up independently, on its own schedule, to send the report. That flag hand-off is exactly
what makes the two effects "never sequentially gated" on each other per RC-10.

1. **Trigger sites** — three distinct call sites in `app/railway/src/rlx_fsm.c` can call
   `enter_fault()`:
   - `check_closing_or_reclosing_complete()` (around line 139-140): if gates have not reported
     confirmed-closed by `RLX_CLOSING_DEADLINE_MS`, calls
     `enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING)`.
   - `check_gate_contradiction_closed()` (around line 144-148): called every tick while
     `RLX_CLOSED`/`RLX_TRAIN_PRESENT`; if gate sensors no longer confirm closed, calls
     `enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING)`.
   - `check_opening_complete()` (around line 157-159): if gates do not confirm open by
     `RLX_OPENING_DEADLINE_MS`, calls `enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING)`.
   - Separately, `rlx_fsm_report_watchdog_trip()` (around line 476-483), called by
     `rlx_watchdog.c` when the main tick loop appears stalled, calls
     `enter_fault(fsm, FAULT_WATCHDOG_TRIP)` directly (PA-10's watchdog-driven fault path).

2. **`enter_fault(rlx_fsm_t *fsm, fault_flags_t fault_bit)`** — `app/railway/src/rlx_fsm.c`
   around line 106-121. Runs with `fsm->lock` already held by the caller. This single function
   is the fork point for the two independent branches:

   **Branch 1 — immediate local safety actions (synchronous, inside this call):**
   - `rlx_gate_command_close()` (line 115) — unconditionally (re)starts a gate-close motion, per
     the inline comment: "a fault must force gates DOWN, not leave them wherever they happened to
     be" (PA-10/RC-10 audit fix).
   - `rlx_signal_show_fault(fault_bit)` (line 116) — forces the train signal to the fault/STOP
     indication.
   - `fsm->state = RLX_FAULT` (line 117) and `fsm->faults |= fault_bit` (line 119) — latches the
     fault; `rlx_fsm_get_crossing_state()`'s mapping (`map_to_crossing_state()`, line 40) reports
     any `RLX_FAULT` internal state as the wire-visible `CROSSING_FAULT`.

   **Branch 2 — independent notification (asynchronous, decoupled by a flag):**
   - `fsm->fault_report_pending = 1` (line 120) — this is the *entire* extent of what
     `enter_fault()` does for reporting. It does not call any send function itself.
   - `rlx_fsm_take_fault_report_pending(rlx_fsm_t *fsm)` (`rlx_fsm.c` around line 429-439) is a
     separate accessor, called from a different thread/context (the server thread's pulse
     handler, not the FSM's own call stack), that atomically reads-and-clears this flag under
     `fsm->lock`. The header comment on this function (`rlx_fsm.h` around line 88-93) states
     explicitly: "so rlx_main.c can independently fire off MSG_FAULT_REPORT ... and apply local
     safe outputs in parallel (RC-10 — neither is sequentially gated on the other)."

3. **`on_pulse()` in `app/railway/src/rlx_main.c`** (around line 46-75), case
   `IPC_PULSE_RAILWAY_WARNING` (the RLx's reused 1 Hz generic tick, around line 51-63):
   - Calls `rlx_fsm_on_tick(&ctx->fsm)` (line 57) — this is what actually re-evaluates gate
     confirmation each second and may internally trigger one of the `enter_fault()` call sites
     from step 1.
   - Immediately after, calls `rlx_fsm_take_fault_report_pending(&ctx->fsm)` (line 59); if it
     returns nonzero, calls `rlx_comm_send_fault_report(ctx->self_id, &ctx->fsm,
     ctx->client_queue)` (line 60).
   - Also unconditionally calls `rlx_comm_broadcast_crossing_status_if_changed(...)` (line 62),
     which separately notifies adjacent Lx controllers of the new `CROSSING_FAULT` state
     (`CROSSING_STATUS`, see Cross-node view below) — this is the RC-07 adjacency notification the
     SD-06 diagram shows as `RLx->>Lx: CROSSING_STATUS(FAULT)`.

4. **`rlx_comm_send_fault_report(controller_id_t self_id, rlx_fsm_t *fsm, ipc_client_queue_t
   *client_queue)`** — `app/railway/src/rlx_comm.c` around line 74-99:
   - Builds an `ipc_request_t` with `verb = MSG_FAULT_REPORT`, `target_id = CTRL_C1`.
   - Calls `rlx_fsm_fill_status(fsm, &tmp_status)` to read `fault_flags_t` back out and packs it
     into `req.payload.fault_report.fault_code` (line 86); `severity` is a fixed placeholder
     (`1`, line 87 — "no severity-classification scheme is designed anywhere").
   - Calls `ipc_client_post(client_queue, CTRL_C1, &req, on_reply_log_failure, NULL)` (line 96) to
     enqueue the send on the **client thread**, not the server thread that is running
     `on_pulse()` — this is the actual OS/IPC-level mechanism that makes the report "independent":
     it goes out over `client_queue` on its own thread, while the FSM state mutation in step 2 has
     already completed synchronously by the time this call is made. A failed enqueue is logged,
     not silently dropped (line 97, per RC-10's "safety-relevant" comment).

### (b) Fault clearance — operator command through to RLx's local verify/ACK/NACK

5. **Operator presses `'f'`** in the command-reader loop in `app/central/src/c_operator.c`
   (around line 503-506), which locks `console_io_lock` and calls `handle_request_fault_clear(args)`.

6. **`handle_request_fault_clear(c_operator_args_t *args)`** — `app/central/src/c_operator.c`
   around line 366-395:
   - Prompts for node type (0 = intersection Lx, 1 = railway RLx) and the specific node number
     (`parse_rlx()` validates RLx 1-3).
   - Logs `"Operator: REQUEST_FAULT_CLEAR(target=%d) submitted"` via `c_logger_log()`.
   - Calls `c_comm_send_request_fault_clear(args->client_queue, target)` (line 394).

7. **`c_comm_send_request_fault_clear(ipc_client_queue_t *q, controller_id_t target)`** —
   `app/central/src/c_comm.c` around line 242-255:
   - Builds an `ipc_request_t` with `verb = MSG_REQUEST_FAULT_CLEAR`, `sender_id = CTRL_C1`,
     `target_id = target`. No payload on the wire.
   - Calls `ipc_client_post(q, target, &req, on_command_reply, NULL)`; `on_command_reply()`
     (`c_comm.c` around line 87-113) is the callback that will later log the ACK/NACK result under
     `g_console_io_lock`.

8. **`on_request()` in `app/railway/src/rlx_main.c`** (around line 31-44), case
   `MSG_REQUEST_FAULT_CLEAR` (line 36-38): dispatches directly to
   `rlx_fsm_on_fault_clear(&ctx->fsm, reply)` — this is the *only* thing a cross-node request from
   Central is allowed to do to the RLx FSM (per the doc comment on `rlx_fsm_on_fault_clear()` in
   `rlx_fsm.h` around line 66-74: "RLx never accepts a raw gate/flasher/signal command from
   Central" — this is the code-level enforcement of RC-09).

9. **`rlx_fsm_on_fault_clear(rlx_fsm_t *fsm, ipc_reply_t *reply)`** —
   `app/railway/src/rlx_fsm.c` around line 297-326:
   - If `fsm->state != RLX_FAULT`, immediately replies `RESULT_NACK` /
     `NACK_REASON_UNKNOWN_TARGET` (line 302-303) — nothing to clear.
   - Otherwise calls `gates_confirmed_open()` (line 308, wraps `rlx_gate_poll_open()`) to
     **re-check the live sensor state at this exact moment**, not a cached value — the inline
     comment states this explicitly: "RC-10: Central's request never bypasses live verification."
     - If open is confirmed: transitions `fsm->state = RLX_OPEN`, clears `fsm->faults =
       FAULT_NONE`, drops stale occupancy-window bookkeeping, and sets `reply->result =
       RESULT_ACK` (line 309-318). This is UC-06 alt-flow 7.1 ("verification succeeds").
     - If not confirmed open: `reply->result = RESULT_NACK`, `reply->reason =
       NACK_REASON_FAULT_ACTIVE` (line 320-321) — the crossing remains latched in `RLX_FAULT`.
       This is alt-flow 7.2 ("premature clearance request rejected").
   - The reply is returned synchronously through the QNX `MsgReply()` path underlying
     `ipc_server_run()`; `on_command_reply()` in `c_comm.c` (client-thread callback registered in
     step 7) logs the eventual ACK/NACK once it arrives back at Central.

## Cross-node view

Two IPC verbs carry this entire use case across node boundaries (see `SEQUENCE_DIAGRAMS.md`,
section 4.2.6, **SD-06 — Contain and Report a Railway Equipment Fault**, which this document
traces down to function level):

- **`MSG_FAULT_REPORT`** (RLx → C1) — sent by `rlx_comm_send_fault_report()`
  (`rlx_comm.c`), received in `on_request()`'s `MSG_FAULT_REPORT` case in
  `app/central/src/c_main.c` (around line 105-122). That handler calls
  `c_server_record_fault_report()` under `mode_eng_lock`, then — after releasing
  `mode_eng_lock` — takes `console_io_lock` to call `c_logger_log("FAULT_REPORT from %d:
  fault_code=0x%08x severity=%u detail=\"%s\"", ...)` (line 114-118). The comment at line 109-113
  notes this ordering is a fix for a terminal-splicing hazard shared with `c_hmi_render()`'s
  status table — `console_io_lock` must guard the log line since the fault report can arrive
  asynchronously at any time on the server thread.
- **`MSG_REQUEST_FAULT_CLEAR`** (C1 → RLx) — sent by `c_comm_send_request_fault_clear()`
  (`c_comm.c`), received in `on_request()`'s `MSG_REQUEST_FAULT_CLEAR` case in
  `app/railway/src/rlx_main.c` (line 36-38), which delegates directly to
  `rlx_fsm_on_fault_clear()`.

A third verb, **`MSG_CROSSING_STATUS`** (RLx → adjacent Lx and RLx → C1), is fired from the same
`on_pulse()` tick via `rlx_comm_broadcast_crossing_status_if_changed()` and corresponds to SD-06's
`RLx->>Lx: CROSSING_STATUS(FAULT)` step — it is how the adjacent intersection controllers (per
RC-07's fixed adjacency table in `rlx_comm.c` around line 109-113) learn to hold their
toward-crossing movements at RED, independent of the `MSG_FAULT_REPORT` path to Central.

## System-level summary diagram

```mermaid
---
title: UC-06 — Respond to a Railway Equipment Fault (function-level trace of SD-06)
---
sequenceDiagram
    accTitle: UC-06 function-level trace
    accDescr: enter_fault() forks into a synchronous local-safety branch and an independently-drained fault-report branch; a later operator fault-clear request is answered by rlx_fsm_on_fault_clear()'s live re-verification.
    autonumber

    participant GS as Gate Sensor (rlx_gate.c)
    participant FSM as rlx_fsm.c (enter_fault)
    participant Main as rlx_main.c (on_pulse)
    participant Comm as rlx_comm.c
    participant Lx as Adjacent Lx
    participant C1 as c_main.c (on_request)
    actor Op as Operator (c_operator.c)

    GS-->>FSM: gate confirmation missing/contradictory
    FSM->>FSM: enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING)

    par immediate local safety actions (synchronous, inside enter_fault)
        FSM->>FSM: rlx_gate_command_close()
        FSM->>FSM: rlx_signal_show_fault(bit)
        FSM->>FSM: state = RLX_FAULT; faults |= bit
    and independent fault-report path (flag only, drained later)
        FSM->>FSM: fault_report_pending = 1
        Main->>FSM: rlx_fsm_take_fault_report_pending()
        Main->>Comm: rlx_comm_send_fault_report()
        Comm->>C1: MSG_FAULT_REPORT(fault_code, severity, detail)
        C1->>C1: c_server_record_fault_report() + c_logger_log() under console_io_lock
    end

    Main->>Comm: rlx_comm_broadcast_crossing_status_if_changed()
    Comm->>Lx: MSG_CROSSING_STATUS(CROSSING_FAULT)
    Lx->>Lx: hold toward-crossing movements at RED

    Note over Op,C1: later, after physical repair
    Op->>C1: presses 'f' -> handle_request_fault_clear()
    C1->>Comm: c_comm_send_request_fault_clear(target=RLx)
    Comm->>FSM: MSG_REQUEST_FAULT_CLEAR
    FSM->>FSM: rlx_fsm_on_fault_clear(): gates_confirmed_open()?

    alt gates re-confirmed open
        FSM-->>C1: RESULT_ACK (faults cleared, state = RLX_OPEN)
    else still not confirmed open
        FSM-->>C1: RESULT_NACK (NACK_REASON_FAULT_ACTIVE)
    end

    C1->>C1: on_command_reply() logs ACK/NACK under console_io_lock
```
