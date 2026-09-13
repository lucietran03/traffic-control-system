# UC-02 — Serve Pedestrian Crossing Request

**Trigger:** keypress `1`/`2`/`3`/`4` (pedestrian button, side 0-3)
**Scope:** single-node — `lx_main` only; railway pre-emption gates it indirectly via incoming `MSG_CROSSING_STATUS` from `RLx`
**Spec:** `usecase.md` UC-02 · `SEQUENCE_DIAGRAMS.md` SD-02 · rules TL-05, TL-06, PA-02, PA-03

```mermaid
sequenceDiagram
    actor Ped as Pedestrian
    participant Sensor as Sensor thread<br/>lx_sensor.c
    participant FSM as fsm struct<br/>(fsm->lock)
    participant Server as Server thread<br/>lx_main.c on_pulse()<br/>100ms IPC_PULSE_PHASE_TIMER
    participant Signal as lx_signal.c<br/>printf → stdout

    Ped->>Sensor: key '1'..'4' (side 0-3)
    Sensor->>FSM: lx_fsm_latch_pedestrian_request(fsm, side) [lock fsm]
    alt side not in ped_serving_mask
        FSM->>FSM: ped_latched[side] = 1 (TL-06, repeat press idempotent)
    else side already mid-service
        FSM->>FSM: ped_recall[side] = 1 (race fix: request not lost)
    end

    Note over Sensor,Server: gap of 0..N x 100ms ticks until a compatible vehicle phase is active

    loop every 100ms
        Server->>FSM: lx_fsm_on_phase_timer() [lock fsm]
        FSM->>FSM: lx_fsm_ped_service_tick_locked()
        alt ped_phase==NONE and phase==ARTERIAL_GREEN with latched[0|1], or CONNECTOR_GREEN with latched[2|3]
            FSM->>FSM: ped_serving_mask=compatible_mask · ped_phase=WALK · ped_clearance_active=1
            FSM->>Signal: lx_signal_show_walk(side) per served side
        else ped_phase==WALK and elapsed>=LX_WALK_MS
            FSM->>Signal: lx_signal_show_flashing_dont_walk(side)
        else ped_phase==FLASHING_DONT_WALK and elapsed>=LX_FLASHING_DONT_WALK_MS
            FSM->>Signal: lx_signal_show_dont_walk(side)
            FSM->>FSM: ped_recall[side] ? keep latched[side]=1 : latched[side]=0 (TL-06 cleared)
            FSM->>FSM: clear ped_serving_mask, ped_phase=NONE, ped_clearance_active=0
        else phase incompatible, or RAILWAY_PREEMPTION blocks CONNECTOR_GREEN
            FSM->>FSM: leave ped_latched[side]=1 (PA-02: stays latched, never dropped)
        end
    end
```

Constants (`lx_timer.h`): `LX_PHASE_TICK_MS=100` · `LX_WALK_MS=6000` · `LX_FLASHING_DONT_WALK_MS=4000`.

## Code map

| Step | File : Function |
|---|---|
| Keypress → latch | `lx_sensor.c : lx_sensor_reader_thread()` → `lx_fsm.c : lx_fsm_latch_pedestrian_request()` |
| 100ms tick | `lx_main.c : on_pulse()` (`IPC_PULSE_PHASE_TIMER`) → `lx_fsm.c : lx_fsm_on_phase_timer()` |
| Compatibility + WALK/FDW/DONT_WALK sequencing | `lx_fsm.c : lx_fsm_ped_service_tick_locked()`, using `lx_fsm_arterial_ped_compatible_locked()` / `lx_fsm_connector_ped_compatible_locked()` |
| Pedestrian output | `lx_signal.c : lx_signal_show_walk() / show_flashing_dont_walk() / show_dont_walk()` |
| Feeds back into vehicle timing | `lx_fsm.c : lx_fsm_on_phase_timer()` folds ped-compatible flags into `own_demand`/`other_demand` for `lx_timer_should_exit_green()` |
| Railway gate on CONNECTOR side | `lx_fsm.c : lx_fsm_on_crossing_status()` (from `MSG_CROSSING_STATUS`) → `SUPERVISORY_RAILWAY_PREEMPTION` → `lx_fsm_advance_phase_locked()` skips `PHASE_CONNECTOR_GREEN` |

Two threads, one lock: the **sensor thread** only ever latches `ped_latched[]`/`ped_recall[]` via `lx_fsm_latch_pedestrian_request()`; the **server thread** (driven by the 100ms pulse) does the compatibility check, the WALK → FLASHING_DONT_WALK → DONT_WALK sequencing, and all `lx_signal_show_*()` output. They share nothing but `fsm->lock`, so a press can land anywhere between two ticks with no missed or double-counted state.

## Cross-node view

Single-node by construction — matches SD-02, whose own participants are `Pedestrian`, `Push-Button`, `Intersection Controller`, `Pedestrian Signals`. No `MsgSendv`/`ipc_client_post()` call appears in this trace, and neither `C1` nor any `RLx` is a target or dependency for latching or serving a request (`PA-01`). The one cross-node input that matters *indirectly* is an incoming `MSG_CROSSING_STATUS` from `RLx`, which can put the FSM into `SUPERVISORY_RAILWAY_PREEMPTION` and make `PHASE_CONNECTOR_GREEN` stop cycling — so sides 2/3 stay latched (`PA-02`) while sides 0/1 keep being served normally.
