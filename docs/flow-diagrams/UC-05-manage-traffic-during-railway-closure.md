# UC-05 — Manage Traffic During and After a Railway Closure

**Trigger:** inbound `MSG_CROSSING_STATUS` from `RLx` (adjacent railway controller)
**Scope:** cross-node — this is `Lx`'s reaction only; the `RLx` send side is documented in UC-04
**Spec:** `usecase.md` UC-05 · `SEQUENCE_DIAGRAMS.md` SD-05 · rules CC-01, CC-02, CC-03

```mermaid
sequenceDiagram
    participant RLx as Railway ctrl<br/>rlx_comm.c (UC-04)
    participant Server as Lx server thread<br/>lx_main.c on_request()/on_pulse()
    participant State as fsm struct<br/>(fsm->lock)
    participant Signal as lx_signal.c

    Note over RLx,Signal: Moment 1 — crossing closes
    RLx->>Server: MSG_CROSSING_STATUS(state != OPEN)
    Server->>State: lx_fsm_on_crossing_status()<br/>supervisory = RAILWAY_PREEMPTION

    alt connector green already running (the fixed bug)
        loop every 100ms IPC_PULSE_PHASE_TIMER
            Server->>State: lx_fsm_on_phase_timer(), PHASE_CONNECTOR_GREEN case
            alt supervisory==RAILWAY_PREEMPTION && green_elapsed_ms >= LX_MIN_GREEN_MS
                State->>State: drain_active=0, drain_extending=0
                Server->>State: lx_fsm_advance_phase_locked()<br/>cut green short → YELLOW → ALL_RED
                Server->>Signal: lx_signal_show_phase()
            end
        end
    else no connector green running yet
        Server->>State: lx_fsm_advance_phase_locked(), PHASE_ALL_RED_A_TO_B case
        State->>State: supervisory==RAILWAY_PREEMPTION →<br/>skip CONNECTOR_GREEN, phase = PHASE_ARTERIAL_GREEN
        Server->>Signal: lx_signal_show_phase() (never prints CONNECTOR GREEN)
    end

    Note over RLx,Signal: Moment 2 — crossing reopens
    RLx->>Server: MSG_CROSSING_STATUS(state == OPEN)
    Server->>State: lx_fsm_on_crossing_status()<br/>supervisory = NORMAL_OPERATION
    opt queue_warning_active
        State->>State: drain_pending = 1
    end

    Server->>State: lx_fsm_advance_phase_locked(), next PHASE_ALL_RED_A_TO_B
    alt drain_pending was set
        State->>State: drain_pending=0, drain_active=1<br/>phase = PHASE_CONNECTOR_GREEN
        Server->>State: lx_fsm_on_phase_timer() — ordinary duration/demand check runs first
        State->>State: drain_active → drain_extending = 1 (held open instead of advancing)
        loop every LX_EXTENSION_MS (4s)
            alt queue_warning_active && drain_extension_total_ms < LX_DRAIN_MAX_EXTENSION_MS
                State->>State: drain_extension_total_ms += 4000 (extend)
            else cleared, or cap reached
                Server->>State: lx_fsm_advance_phase_locked()<br/>YELLOW → ALL_RED_B_TO_A → ARTERIAL_GREEN
                Server->>Signal: lx_signal_show_phase()
            end
        end
    else drain_pending was not set
        State->>State: phase = PHASE_CONNECTOR_GREEN (ordinary cycle, no drain)
    end
```

Constants (`lx_timer.h`): `LX_MIN_GREEN_MS=8000` · `LX_EXTENSION_MS=4000` · `LX_DRAIN_MAX_EXTENSION_MS=60000`.
The Branch-A early-cutoff check runs on **every** 100ms tick (not gated to a 4s modulo like ordinary demand checks) — before this fix, an already-running connector green was never truncated, only prevented from starting fresh (Branch B).

## Code map

| Step | File : Function |
|---|---|
| Inbound message | `lx_main.c : on_request()` (`MSG_CROSSING_STATUS`) → `lx_fsm.c : lx_fsm_on_crossing_status()` |
| Suppress — mid-green cutoff (Branch A) | `lx_fsm.c : lx_fsm_on_phase_timer()`, `PHASE_CONNECTOR_GREEN` case, early-cutoff check |
| Suppress — boundary guard (Branch B) | `lx_fsm.c : lx_fsm_advance_phase_locked()`, `PHASE_ALL_RED_A_TO_B` case |
| Drain arm → consume | `lx_fsm_on_crossing_status()` sets `drain_pending` → `lx_fsm_advance_phase_locked()` consumes it into `drain_active` |
| Drain extend/exit loop | `lx_fsm_on_phase_timer()`, `drain_active && drain_extending` block |
| Phase output | `lx_signal.c : lx_signal_show_phase()` |

One server thread (QNX single-channel receive loop) handles `MSG_CROSSING_STATUS`, every other request verb, and the 100ms `IPC_PULSE_PHASE_TIMER` pulse, so `lx_fsm_on_crossing_status()` and `lx_fsm_on_phase_timer()` never run concurrently with each other — `fsm->lock` is still needed because `lx_sensor.c`, `lx_watchdog.c`, and `lx_comm.c` touch `fsm` from other threads.

## Cross-node view

This trace is `Lx`'s reaction to a message it did not originate. `RLx` sends via `rlx_comm.c : send_crossing_status()` / `rlx_comm_broadcast_crossing_status_if_changed()`, fanned out per the fixed `ADJACENCY` table; `Lx` always `ACK`s and never commands railway equipment back (`RC-02`). See UC-04's flow diagram for how `RLx` decides when to send `WARNING`/`CLOSED`/`OPEN`/`FAULT` (SD-04). This document is the `Lx`-side function-level expansion of SD-05.
