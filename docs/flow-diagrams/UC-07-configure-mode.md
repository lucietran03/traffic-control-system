# UC-07 — Configure Traffic Operating Parameters (SET_MODE)

**Trigger:** keypress `m` (manual, `handle_set_mode()`, targets **1** chosen Lx) · 1Hz auto-check `IPC_PULSE_HEARTBEAT_TICK` (automatic, `c_mode_eng_auto_check()`, targets **all 6** Lx) · keypress `d` (`handle_demo_hour()`, demo-hour shortcut) reuses the exact same broadcast path as the automatic trigger
**Scope:** two-node — Central (C1) client thread → target Lx server thread(s), over `MSG_SET_MODE`
**Spec:** `usecase.md` UC-07 · `SEQUENCE_DIAGRAMS.md` SD-03 · rules DP-01, DP-02, TL-04, PA-09

```mermaid
sequenceDiagram
    actor Kbd as Keyboard
    participant Op as c_operator.c<br/>handle_set_mode() / handle_demo_hour()
    participant Pulse as c_main.c on_pulse()<br/>1Hz IPC_PULSE_HEARTBEAT_TICK
    participant Comm as c_comm.c<br/>send/broadcast_set_mode()
    participant Client as client thread<br/>qnet_utils.c MsgSend()
    participant Lx as lx_main.c on_request()<br/>MSG_SET_MODE case
    participant FSM as lx_fsm.c<br/>(fsm->lock)

    alt (a) manual — 'm', 1 target Lx
        Kbd->>Op: 'm' + Lx(1-6) + mode
        Op->>Comm: c_comm_send_set_mode(target, mode)
    else (b) automatic — 1Hz auto-check, all 6 Lx
        Pulse->>Pulse: c_mode_eng_auto_check()<br/>schedule-implied mode changed?
        Pulse->>Comm: c_comm_broadcast_set_mode(mode)<br/>loops CTRL_L1..CTRL_L6
    else (b') demo shortcut — 'd', all 6 Lx
        Kbd->>Op: 'd' handle_demo_hour()
        Op->>Op: seed last_auto_mode directly<br/>(skips waiting for real clock)
        Op->>Comm: c_comm_broadcast_set_mode(mode)<br/>same fn as path (b)
    end

    Comm->>Client: ipc_client_post() : verb = MSG_SET_MODE
    Client->>Lx: MsgSend() [x1 path a · x6 path b/b']

    Lx->>FSM: lx_fsm_on_set_mode(payload, reply) [lock fsm]
    alt supervisory == FAULT_SAFE
        FSM-->>Client: NACK (FAULT_ACTIVE)
    else mode out of range
        FSM-->>Client: NACK (OUT_OF_RANGE)
    else requested mode == current mode
        FSM-->>Client: ACK (no-op)
    else genuine change while phase active
        FSM->>FSM: pending_mode = mode<br/>mode_change_pending = 1
        FSM-->>Client: ACK_PENDING
    end

    loop next ALL_RED boundary (A→B or B→A)
        FSM->>FSM: lx_fsm_advance_phase_locked()<br/>if mode_change_pending: mode = pending_mode
    end
    FSM->>Client: next heartbeat — status.mode reports confirmed mode
```

NACK reasons (`PA-09`): `NACK_REASON_FAULT_ACTIVE` · `NACK_REASON_OUT_OF_RANGE`.
Path (b)/(b') is **6 independent unicast** `MSG_SET_MODE` sends, not one broadcast wire message.
`console_io_lock` (outer) and `mode_eng_lock` (inner) guard C1-side bookkeeping only — neither is
held across `MsgSend()`/`MsgReceive()`. On the Lx side, `fsm->lock` covers both validation and the
deferred apply, so they're atomic with respect to each other.

## Code map

| Step | File : Function |
|---|---|
| (a) Manual keypress → send | `c_operator.c : c_operator_reader_thread()` `'m'` → `handle_set_mode()` → `c_comm.c : c_comm_send_set_mode()` |
| (b) Auto-check tick → broadcast | `c_main.c : on_pulse()` (`IPC_PULSE_HEARTBEAT_TICK`) → `c_mode_eng.c : c_mode_eng_auto_check()` → `c_comm.c : c_comm_broadcast_set_mode()` |
| (b') Demo shortcut → broadcast | `c_operator.c : handle_demo_hour()` (`'d'`) → same `c_comm_broadcast_set_mode()` as (b) |
| Wire send (both) | `c_comm.c : c_comm_send_set_mode()` → `ipc_client_post()` → `qnet_utils.c` client thread `MsgSend()` |
| Receive + dispatch | `lx_main.c : on_request()` (`MSG_SET_MODE` case) → `lx_fsm.c : lx_fsm_on_set_mode()` |
| Deferred apply | `lx_fsm.c : lx_fsm_advance_phase_locked()` at `PHASE_ALL_RED_A_TO_B` / `PHASE_ALL_RED_B_TO_A` |
| Confirm to Central | `lx_comm.c : lx_comm_send_heartbeat()` → `lx_fsm_fill_status()` → `MSG_HEARTBEAT`/`MSG_STATUS` |

## Cross-node view

Wire message: `MSG_SET_MODE` (`ipc_msg.h`), payload `set_mode_payload_t { uint32_t mode; }`,
`sender_id = CTRL_C1`, `target_id` = the addressed Lx.

- **Path (a):** 1 request → 1 target → 1 `ACK`/`ACK_PENDING`/`NACK`.
- **Paths (b)/(b'):** 6 sequential unicast requests (`CTRL_L1..CTRL_L6`), each its own
  request/reply exchange — no single broadcast frame exists on the wire.

Matches `SEQUENCE_DIAGRAMS.md` SD-03's `opt operator requests a mode change` block
(`Op->>C1: submit SET_MODE` / `C1->>L1: SET_MODE` / `L1->>L1: validate + wait for safe boundary`,
then `alt accepted / else rejected`). Path (b)/(b') replays the identical `opt` block once per
controller across L1-L6, triggered by `c_mode_eng_auto_check()` instead of an operator submission.
