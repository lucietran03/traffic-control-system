# UC-06 — Respond to a Railway Equipment Fault

**Trigger:** fault detection inside `rlx_fsm.c` (`enter_fault()`), and — for clearance — operator keypress `f`
**Scope:** cross-node — RLx ↔ C1, plus RLx → adjacent Lx broadcast
**Spec:** `usecase.md` UC-06 · `SEQUENCE_DIAGRAMS.md` SD-06 · rules RC-06, RC-09, RC-10, PA-09

```mermaid
sequenceDiagram
    participant GS as Gate sensor<br/>rlx_gate.c
    participant FSM as rlx_fsm.c<br/>(fsm->lock)
    participant Main as rlx_main.c<br/>on_pulse()
    participant Comm as rlx_comm.c
    participant Lx as Adjacent Lx
    participant C1 as c_main.c<br/>on_request()
    actor Op as Operator<br/>c_operator.c

    Note over GS,FSM: (a) Fault containment
    GS-->>FSM: gate confirm missing/contradictory<br/>(or rlx_watchdog.c trip)
    FSM->>FSM: enter_fault(fsm, fault_bit)

    par local safe state — synchronous, inside enter_fault()
        FSM->>FSM: rlx_gate_command_close()
        FSM->>FSM: rlx_signal_show_fault(bit)
        FSM->>FSM: state = RLX_FAULT · faults |= bit
    and independent report — flag drained later, own thread (RC-10)
        FSM->>FSM: fault_report_pending = 1
        Main->>FSM: rlx_fsm_take_fault_report_pending()
        Main->>Comm: rlx_comm_send_fault_report()
        Comm->>C1: MSG_FAULT_REPORT(fault_code, severity)
    end

    Main->>Comm: rlx_comm_broadcast_crossing_status_if_changed()
    Comm->>Lx: MSG_CROSSING_STATUS(FAULT)
    Lx->>Lx: hold toward-crossing movements at RED

    Note over Op,C1: (b) Fault clearance — later, after physical repair
    Op->>C1: 'f' → handle_request_fault_clear()
    C1->>FSM: MSG_REQUEST_FAULT_CLEAR
    FSM->>FSM: rlx_fsm_on_fault_clear():<br/>gates_confirmed_open()? (live re-check, not cached)
    alt confirmed open
        FSM-->>C1: RESULT_ACK (state = RLX_OPEN, faults cleared)
    else still not confirmed open
        FSM-->>C1: RESULT_NACK (NACK_REASON_FAULT_ACTIVE)
    end
    C1->>C1: on_command_reply() logs ACK/NACK under console_io_lock
```

RC-10 in one line: `enter_fault()` performs the local-safety branch synchronously, then only *sets a flag* (`fault_report_pending`) — a different thread (`on_pulse()`) drains that flag on its own schedule to send `MSG_FAULT_REPORT`. Neither branch waits on the other; that's why they're drawn as `par`, not sequential steps.
RC-09 in one line: Central's `MSG_REQUEST_FAULT_CLEAR` never carries a gate/signal command — `rlx_fsm_on_fault_clear()` is the only thing it can trigger, and the RLx alone decides ACK/NACK from a fresh sensor read.

## Code map

| Step | File : Function |
|---|---|
| Trigger sites (3x deadline/contradiction + watchdog) | `rlx_fsm.c : check_closing_or_reclosing_complete/check_gate_contradiction_closed/check_opening_complete()` · `rlx_watchdog.c → rlx_fsm_report_watchdog_trip()` |
| Fork point | `rlx_fsm.c : enter_fault()` |
| Local safety branch | `rlx_gate.c : rlx_gate_command_close()` · `rlx_signal.c : rlx_signal_show_fault()` |
| Report branch (independent thread) | `rlx_fsm.c : rlx_fsm_take_fault_report_pending()` → `rlx_comm.c : rlx_comm_send_fault_report()` |
| Adjacency notify | `rlx_comm.c : rlx_comm_broadcast_crossing_status_if_changed()` |
| Central receives report | `c_main.c : on_request()` (`MSG_FAULT_REPORT` case) → `c_server_record_fault_report()` |
| Operator requests clear | `c_operator.c : handle_request_fault_clear()` → `c_comm.c : c_comm_send_request_fault_clear()` |
| RLx handles clear | `rlx_main.c : on_request()` (`MSG_REQUEST_FAULT_CLEAR` case) → `rlx_fsm.c : rlx_fsm_on_fault_clear()` |

## Cross-node view

- **`MSG_FAULT_REPORT`** (RLx → C1) — carries `fault_code`/`severity`; sent by `rlx_comm_send_fault_report()`, received in `c_main.c`'s `on_request()`.
- **`MSG_REQUEST_FAULT_CLEAR`** (C1 → RLx) — no payload; sent by `c_comm_send_request_fault_clear()`, received in `rlx_main.c`'s `on_request()`, answered synchronously with `RESULT_ACK`/`RESULT_NACK`.
- **`MSG_CROSSING_STATUS`** (RLx → adjacent Lx, RLx → C1) — fired from the same `on_pulse()` tick as the report branch; how adjacent intersections learn to hold RED (RC-07 adjacency), independent of the C1 report path.
