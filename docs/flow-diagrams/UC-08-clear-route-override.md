# UC-08 — Clear-Route Override

**Trigger:** operator keys `o` (request) / `r` (renew) / `c` (cancel)
**Scope:** Central (`c_operator.c`, `c_mode_eng.c`, `c_comm.c`) <-> target Lx (`lx_fsm.c`) over Qnet
**Spec:** `usecase.md` UC-08 · `SEQUENCE_DIAGRAMS.md` SD-07 · rules PA-09, PA-11, PA-12

```mermaid
sequenceDiagram
    actor Op as Operator
    participant Console as Central console thread<br/>c_operator.c
    participant ModeEng as c_mode_eng.c<br/>(mode_eng_lock)
    participant Client as Central client thread<br/>c_comm.c → qnet_utils.c
    participant Lx as Lx server thread<br/>lx_fsm.c (fsm->lock)

    Op->>Console: 'o' handle_request_override()
    Console->>ModeEng: c_mode_eng_validate_override_request()
    alt target/duration/type invalid
        ModeEng-->>Console: reject (NACK_REASON_*)
        Note over Console: never forwarded to Lx
    else accepted (PA-11 surface check)
        ModeEng->>ModeEng: override_in_flight = 1 (bookkeeping only)
        Console->>Client: c_comm_send_request_override()<br/>MSG_REQUEST_OVERRIDE
        Client->>Lx: MsgSend (ipc_client_post → on_request())
        Lx->>Lx: lx_fsm_on_request_override() [lock fsm]<br/>re-checks duration/target/fault/railway — authoritative
        alt already active/pending, bad duration/target,<br/>railway pre-emption, or FAULT_SAFE
            Lx-->>Client: NACK(reason)
        else fsm->ped_clearance_active
            Lx->>Lx: override_substate = OVR_PENDING_CLEARANCE<br/>(WALK/FLASHING_DONT_WALK not truncated)
            Lx-->>Client: ACK_PENDING (BR-6/PA-12, still within deadline)
        else no conflict
            Lx->>Lx: override_substate = OVR_ACTIVE
            Lx-->>Client: ACK
        end
    end

    loop every phase tick — lx_fsm_on_phase_timer() [lock fsm]
        alt OVR_ACTIVE or OVR_PENDING_CLEARANCE
            Lx->>Lx: override_remaining_ms -= LX_PHASE_TICK_MS
        end
        Lx->>Lx: lx_fsm_ped_service_tick_locked()
        alt was PENDING_CLEARANCE, ped_clearance_active just dropped
            Lx->>Lx: override_substate = OVR_ACTIVE (revalidation)
        end
        alt override_remaining_ms hits 0
            Lx->>Lx: lx_fsm_terminate_override_locked()<br/>(can fire while still PENDING, i.e. expires unactivated)
        else OVR_ACTIVE at ALL_RED boundary
            Lx->>Lx: lx_fsm_advance_phase_locked() forces<br/>next green = override_target_movement
        else OVR_ACTIVE, target movement already green
            Lx->>Lx: skip PEAK_FIXED/OFF_PEAK_SENSOR exit-check
        end
    end

    opt operator renews before expiry
        Op->>Console: 'r' handle_renew_override()
        Console->>Client: c_comm_send_renew_override()<br/>MSG_RENEW_OVERRIDE
        Client->>Lx: lx_fsm_on_renew_override() [lock fsm]
        alt not OVR_ACTIVE, or extension > LX_OVERRIDE_DURATION_CAP_MS
            Lx-->>Client: NACK (existing expiry untouched)
        else valid
            Lx->>Lx: recompute override_duration_ms/remaining_ms
            Lx-->>Client: ACK (countdown restarted)
        end
    end

    alt operator cancels
        Op->>Console: 'c' handle_cancel_override()
        Console->>Client: c_comm_send_cancel_override()<br/>MSG_CANCEL_OVERRIDE (no payload)
        Client->>Lx: lx_fsm_on_cancel_override() [lock fsm]
        alt substate is PENDING_CLEARANCE or ACTIVE
            Lx->>Lx: lx_fsm_terminate_override_locked()
            Lx-->>Client: ACK
        else already OVR_NONE
            Lx-->>Client: NACK
        end
    else remaining_ms already reached 0 in tick loop above
        Note over Lx: terminated by phase-timer countdown instead
    end

    Note over Lx: lx_fsm_terminate_override_locked() always resets<br/>override_substate = OVR_NONE, restores<br/>supervisory = SUPERVISORY_NORMAL_OPERATION
```

## Code map

| Step | File : Function |
|---|---|
| Request keypress | `c_operator.c : c_operator_reader_thread()` → `handle_request_override()` |
| Central surface check | `c_mode_eng.c : c_mode_eng_validate_override_request()` |
| Wire send (request) | `c_comm.c : c_comm_send_request_override()` → `qnet_utils.c : ipc_client_post()` |
| Lx dispatch | `lx_main.c : on_request()` → `lx_fsm.c : lx_fsm_on_request_override()` |
| Countdown + re-validate | `lx_main.c : on_pulse()` → `lx_fsm.c : lx_fsm_on_phase_timer()`, `lx_fsm_ped_service_tick_locked()` |
| Phase steering | `lx_fsm.c : lx_fsm_advance_phase_locked()` |
| Renew | `c_operator.c : handle_renew_override()` → `c_comm.c : c_comm_send_renew_override()` → `lx_fsm.c : lx_fsm_on_renew_override()` |
| Cancel | `c_operator.c : handle_cancel_override()` → `c_comm.c : c_comm_send_cancel_override()` → `lx_fsm.c : lx_fsm_on_cancel_override()` |
| Termination (shared) | `lx_fsm.c : lx_fsm_terminate_override_locked()` |

Per `app/shared/README.md`'s threading pattern: the console thread only ever
builds the request and enqueues it; Central's dedicated **client thread**
(`ipc_client_thread_main()`) does the actual `MsgSend()`, so the console
never blocks on the Lx's reply. `override_in_flight` on Central is local
bookkeeping only — never synchronised with the Lx's real state.

## Cross-node view

Three IPC verbs (`ipc_msg.h`) carry the whole lifecycle, all Central-to-Lx,
all replied to synchronously (`RESULT_ACK` / `RESULT_ACK_PENDING` /
`RESULT_NACK`) via `MsgSend`/`MsgReceive`/`MsgReply`:

- `MSG_REQUEST_OVERRIDE` — start (may `ACK`, `ACK_PENDING`, or `NACK`)
- `MSG_RENEW_OVERRIDE` — extend an `OVR_ACTIVE` override before it expires
- `MSG_CANCEL_OVERRIDE` — operator-initiated early termination

Matches **SD-07** (`SEQUENCE_DIAGRAMS.md` §4.2.7) — the most detailed
diagram in that document — including the pedestrian-deferral branch, the
renewal loop, and the expiry-vs-cancel termination paths.
