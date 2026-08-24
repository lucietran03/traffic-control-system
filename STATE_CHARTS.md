# 4.1 Intersection State Charts

The following state charts define the traffic-light, pedestrian, railway-crossing, and communication behaviour of the distributed traffic-control system. Generic charts are used because `L1–L6` share one configuration-driven intersection-controller design and `RL1–RL3` share one railway-controller design (`RC-07`). Together they cover normal operation, safety intervention, equipment faults, and loss of communication with Central.

Where a single state chart would otherwise be too dense to stay legible on an A4 page, it is split into lettered sub-diagrams (`SC-01A`/`B`/`C`, `SC-03A`/`B`, `SC-04A`/`B`) that hand off to each other at a named boundary state, in the same way a "master" diagram references a "detail" diagram for one of its composite states. Each sub-diagram says explicitly where control comes from and where it goes next.

## 4.1.1 SC-01A — Vehicle-Signal Mode Selection Overview

This overview shows how a generic intersection controller chooses between `PEAK_FIXED` and `OFF_PEAK_SENSOR`, without repeating either mode's phase-by-phase detail. A pending mode change — whether from the local clock or a Central `SET_MODE` — is only ever applied once the current phase and its clearances have completed (`TL-04`). See `SC-01B` for the `PEAK_FIXED` cycle and `SC-01C` for the `OFF_PEAK_SENSOR` cycle. It supports UC-01 and UC-07.

```mermaid
---
title: SC-01A — Vehicle-Signal Mode Selection Overview
---
stateDiagram-v2
    accTitle: SC-01A Vehicle-Signal Mode Selection Overview
    accDescr: Top-level choice between PEAK_FIXED and OFF_PEAK_SENSOR for a generic intersection controller, applied only at a safe phase boundary.
    direction LR

    state mode_selection <<choice>>
    [*] --> mode_selection : initialise
    mode_selection --> PEAK_FIXED : [Peak selected]
    mode_selection --> OFF_PEAK_SENSOR : [Off-Peak selected]

    state "PEAK_FIXED (detail in SC-01B)" as PEAK_FIXED
    state "OFF_PEAK_SENSOR (detail in SC-01C)" as OFF_PEAK_SENSOR

    PEAK_FIXED --> mode_selection : pending mode change [safe phase boundary]
    OFF_PEAK_SENSOR --> mode_selection : pending mode change [safe phase boundary]

```

## 4.1.2 SC-01B — PEAK_FIXED Phase Detail

This state chart details the fixed 90-second two-phase cycle used while `PEAK_FIXED` is active. Ordinary approach sensors are monitored for status and fault reporting but do not alter the configured phase durations (`TL-01`, `TL-02`).

```mermaid
---
title: SC-01B — PEAK_FIXED Phase Detail
---
stateDiagram-v2
    accTitle: SC-01B PEAK_FIXED Phase Detail
    accDescr: Fixed 90 second two-phase vehicle-signal cycle used while PEAK_FIXED is the active mode.
    direction LR

    state boundary_after_arterial <<choice>>
    state boundary_after_connector <<choice>>

    [*] --> ARTERIAL_GREEN : startup or entry from SC-01A
    ARTERIAL_GREEN --> ARTERIAL_YELLOW : after 48 s
    ARTERIAL_YELLOW --> ALL_RED_A_TO_B : after 4 s
    ALL_RED_A_TO_B --> boundary_after_arterial : after 2 s
    boundary_after_arterial --> CONNECTOR_GREEN : [no mode change pending]
    boundary_after_arterial --> [*] : [mode change pending] / return to SC-01A, next phase connector
    CONNECTOR_GREEN --> CONNECTOR_YELLOW : after 30 s
    CONNECTOR_YELLOW --> ALL_RED_B_TO_A : after 4 s
    ALL_RED_B_TO_A --> boundary_after_connector : after 2 s
    boundary_after_connector --> ARTERIAL_GREEN : [no mode change pending]
    boundary_after_connector --> [*] : [mode change pending] / return to SC-01A, next phase arterial

    note right of ARTERIAL_GREEN
        Ordinary approach-presence sensors are monitored for
        status and fault reporting only; they never extend or
        shorten this fixed cycle (TL-01, TL-02). Full cycle:
        54 s arterial + 36 s connector = 90 s.
    end note

```

## 4.1.3 SC-01C — OFF_PEAK_SENSOR Phase Detail

This state chart details the demand-responsive cycle used while `OFF_PEAK_SENSOR` is active. Each green interval is bounded between 8 and 40 seconds and extends only while demand persists; the arterial cap ensures that a pending connector request is served after no more than one further arterial service opportunity (`TL-03`, `DP-04`, `DP-06`).

```mermaid
---
title: SC-01C — OFF_PEAK_SENSOR Phase Detail
---
stateDiagram-v2
    accTitle: SC-01C OFF_PEAK_SENSOR Phase Detail
    accDescr: Demand-responsive two-phase vehicle-signal cycle used while OFF_PEAK_SENSOR is the active mode, including minimum and maximum green and the anti-starvation rule.
    direction LR

    state boundary_after_arterial <<choice>>
    state boundary_after_connector <<choice>>

    [*] --> ARTERIAL_GREEN : startup or entry from SC-01A
    ARTERIAL_GREEN --> ARTERIAL_GREEN : every 4 s [arterial or latched pedestrian demand persists and green < 40 s] / extend 4 s
    ARTERIAL_GREEN --> ARTERIAL_YELLOW : [green >= 8 s and connector demand pending and (no arterial demand or green = 40 s)]
    ARTERIAL_YELLOW --> ALL_RED_A_TO_B : after 4 s
    ALL_RED_A_TO_B --> boundary_after_arterial : after 2 s
    boundary_after_arterial --> CONNECTOR_GREEN : [no mode change pending]
    boundary_after_arterial --> [*] : [mode change pending] / return to SC-01A, next phase connector
    CONNECTOR_GREEN --> CONNECTOR_GREEN : every 4 s [connector or latched pedestrian demand persists and green < 40 s] / extend 4 s
    CONNECTOR_GREEN --> CONNECTOR_YELLOW : [green >= 8 s and (no connector demand or green = 40 s)]
    CONNECTOR_YELLOW --> ALL_RED_B_TO_A : after 4 s
    ALL_RED_B_TO_A --> boundary_after_connector : after 2 s
    boundary_after_connector --> ARTERIAL_GREEN : [no mode change pending]
    boundary_after_connector --> [*] : [mode change pending] / return to SC-01A, next phase arterial

    note right of ARTERIAL_GREEN
        With no demand anywhere, the intersection simply has no
        enabled exit transition and rests on arterial green
        indefinitely (DP-04). Vehicle and latched pedestrian
        demand are scheduled equally (TL-06).
    end note

    note right of CONNECTOR_GREEN
        Anti-starvation (DP-06): because arterial green is capped
        at 40 s, a latched connector request can be delayed by at
        most one further arterial session before it is served.
    end note

```

## 4.1.4 SC-02 — Generic Pedestrian-Signal State Chart

This state chart defines the lifecycle of a pedestrian request at one crossing side. A button press is latched until a compatible vehicle phase becomes available, after which the complete `WALK → FLASHING_DONT_WALK → DONT_WALK` sequence runs without unsafe interruption (`TL-05`, `TL-06`, `PA-02`, `PA-03`). It supports UC-02 and the pedestrian-safety constraint in UC-08.

```mermaid
---
title: SC-02 — Generic Pedestrian-Signal State Chart
---
stateDiagram-v2
    accTitle: SC-02 Generic Pedestrian-Signal State Chart
    accDescr: Request latching and signal sequence for one pedestrian crossing side at an intersection controlled by L1 to L6.
    direction LR
    [*] --> DONT_WALK
    DONT_WALK --> REQUEST_LATCHED : PED_REQUEST(side) [no request pending] / latch request
    REQUEST_LATCHED --> REQUEST_LATCHED : PED_REQUEST(side) [request already pending] / coalesce request
    REQUEST_LATCHED --> REQUEST_LATCHED : button remains active beyond diagnostic timeout / retain one request and report stuck-active fault
    REQUEST_LATCHED --> WALK : compatible vehicle phase begins / display WALK
    WALK --> WALK : PED_REQUEST(side) / coalesce repeated request
    WALK --> FLASHING_DONT_WALK : WALK interval expires / begin pedestrian clearance
    FLASHING_DONT_WALK --> FLASHING_DONT_WALK : PED_REQUEST(side) / coalesce repeated request
    FLASHING_DONT_WALK --> DONT_WALK : clearance interval expires / clear request and display DONT_WALK

    note right of REQUEST_LATCHED
        A request arriving while railway pre-emption (SC-03A)
        suppresses the compatible movement simply has no enabled
        WALK transition yet and remains latched, not discarded
        (PA-02) — it is served once a compatible phase returns.
    end note

    note left of DONT_WALK
        A permanently silent push-button is not detectable by the
        Core design (PA-03) — indistinguishable from no request.
        Shown here only as a note, not as a transition.
    end note
```

## 4.1.5 SC-03A — Intersection Supervisory Authority Overview

This state chart defines which supervisory condition controls an intersection above the normal phase sequencer in `SC-01`. Local fault protection has the highest authority, followed by railway pre-emption, a validated Central override, and normal Peak or Off-Peak operation. Every request is checked locally, and no Central command may bypass railway or pedestrian safety constraints (`PA-09`, `PA-10`). The override's own validation, queueing, and active/renewal behaviour is detailed separately in `SC-03B`. It supports UC-05–UC-08.

```mermaid
---
title: SC-03A — Intersection Supervisory Authority Overview
---
stateDiagram-v2
    accTitle: SC-03A Intersection Supervisory Authority Overview
    accDescr: Supervisory authority at an intersection, including normal operation, Central override, railway pre-emption, and local fault-safe operation.
    direction TB
    [*] --> NORMAL_OPERATION
    state "NORMAL_OPERATION (phase sequencing in SC-01)" as NORMAL_OPERATION
    state "CENTRAL_OVERRIDE (validation, pending, and renewal detail in SC-03B)" as CENTRAL_OVERRIDE
    state "RAILWAY_PREEMPTION" as RAILWAY_PREEMPTION
    state "FAULT_SAFE" as FAULT_SAFE

    NORMAL_OPERATION --> CENTRAL_OVERRIDE : REQUEST_OVERRIDE(CLEAR_ROUTE, target, duration)
    CENTRAL_OVERRIDE --> NORMAL_OPERATION : rejected, discarded, expired, or cancelled / resume or restore normal mode

    NORMAL_OPERATION --> RAILWAY_PREEMPTION : adjacent crossing becomes active / safely suppress toward-crossing movement
    CENTRAL_OVERRIDE --> RAILWAY_PREEMPTION : adjacent crossing becomes active / cancel or terminate override, suppress toward-crossing movement
    RAILWAY_PREEMPTION --> RAILWAY_PREEMPTION : conflicting REQUEST_OVERRIDE(CLEAR_ROUTE) / NACK
    RAILWAY_PREEMPTION --> NORMAL_OPERATION : crossing reports OPEN and connector drain completes / resume normal mode

    NORMAL_OPERATION --> FAULT_SAFE : local hardware conflict or watchdog trip / set FLASHING_RED and DONT_WALK
    CENTRAL_OVERRIDE --> FAULT_SAFE : local hardware conflict or watchdog trip / cancel or terminate override, apply safe outputs
    RAILWAY_PREEMPTION --> FAULT_SAFE : local hardware conflict or watchdog trip / apply safe outputs
    FAULT_SAFE --> NORMAL_OPERATION : verified repair and accepted local fault-clear request / resume normal operation

    note right of CENTRAL_OVERRIDE
        A railway or fault interruption applies regardless of
        whether the override is still pending or already active
        inside SC-03B — both cases exit this box the same way.
    end note

    note right of RAILWAY_PREEMPTION
        Once the crossing reopens, control always returns to
        NORMAL_OPERATION — an override interrupted by a train
        is never automatically resumed afterward.
    end note
```

## 4.1.6 SC-03B — Clear-Route Override Validation, Pending, and Renewal Detail

This state chart details the `CENTRAL_OVERRIDE` state introduced in SC-03A. It shows local validation, safe deferral behind an active pedestrian clearance, bounded activation, renewal, cancellation, and automatic expiry. A safely deferred request is acknowledged promptly, but it is activated only after pedestrian clearance completes and the request remains valid (`PA-09`, `PA-11`).

```mermaid
---
title: SC-03B — Clear-Route Override Validation, Pending, and Renewal Detail
---
stateDiagram-v2
    accTitle: SC-03B Clear-Route Override Validation, Pending, and Renewal Detail
    accDescr: Validation, pedestrian-clearance queueing, active operation, and validated renewal for a bounded Central clear-route override, entered from and exited to SC-03A.
    direction LR

    state override_validation <<choice>>
    [*] --> override_validation : REQUEST_OVERRIDE(CLEAR_ROUTE, target, duration) from SC-03A
    override_validation --> ACTIVE : [bounded, safe, and no pedestrian clearance active] / ACK, apply at safe boundary
    override_validation --> OVERRIDE_PENDING : [otherwise safe but pedestrian clearance active] / ACK(accepted, pending clearance), queue request
    override_validation --> [*] : [unsafe, unbounded, or conflicts with railway pre-emption] / NACK, return to SC-03A NORMAL_OPERATION

    OVERRIDE_PENDING --> ACTIVE : pedestrian clearance completes [request remains safe and valid] / apply at safe boundary
    OVERRIDE_PENDING --> [*] : request no longer valid, cancelled, or expires before application / discard request, return to SC-03A NORMAL_OPERATION

    state renewal_validation <<choice>>
    ACTIVE --> renewal_validation : RENEW_OVERRIDE(duration)
    renewal_validation --> ACTIVE : [bounded and safe] / ACK, restart override timer
    renewal_validation --> ACTIVE : [invalid, unsafe, or over limit] / NACK, retain current expiry
    ACTIVE --> [*] : duration expires or operator cancels / return to SC-03A NORMAL_OPERATION

    note right of OVERRIDE_PENDING
        The pedestrian sequence itself is never truncated
        (SC-02) — the override simply waits. The initial ACK
        already covers this state; it is not repeated when the
        queued request later activates.
    end note

    note right of ACTIVE
        Bounded and auto-expiring: default maximum 5 minutes,
        renewed only after an explicit request passes local
        duration and safety validation (PA-11).
    end note
```

## 4.1.7 SC-04A — Railway Crossing Approach and Closure

This state chart defines the first part of the railway-crossing lifecycle shared by `RL1–RL3`. It covers train-approach detection, the flash-only warning interval, commanded gate closure, and sensor-confirmed closure before any train signal may display `PROCEED`; the lifecycle continues in SC-04B after the gates are confirmed `CLOSED` (`RC-03`, `RC-06`, `RC-11`).

```mermaid
---
title: SC-04A — Railway Crossing Approach and Closure
---
stateDiagram-v2
    accTitle: SC-04A Railway Crossing Approach and Closure
    accDescr: Railway-crossing operation from train approach detection through the warning interval and commanded gate closure to sensor-confirmed CLOSED, or a safe-side FAULT.
    direction LR
    [*] --> OPEN
    OPEN --> WARNING : TRAIN_APPROACHING(direction) / activate flashers, notify Lx and C1, register occupancy window
    WARNING --> WARNING : TRAIN_APPROACHING(other direction) / register additional occupancy window
    WARNING --> CLOSING : after 5 s / command both gates down
    WARNING --> FAULT : approach input remains active beyond diagnostic timeout / hold safe outputs and report fault

    state gate_confirmation <<choice>>
    CLOSING --> CLOSING : TRAIN_APPROACHING(other direction) / register additional occupancy window
    CLOSING --> gate_confirmation : gate reports received or closing deadline reached
    gate_confirmation --> CLOSED : [both gates confirmed CLOSED before deadline] / set train signal(s) for registered approaches to PROCEED
    gate_confirmation --> FAULT : [confirmation missing or contradictory at deadline] / hold STOP and report fault

    note right of WARNING
        The approximately 45 s figure (RC-03) is the total
        warning-to-arrival budget across flash-lead, gate
        closing, confirmation margin, and adjacent-intersection
        clearance — not the duration of any single state here.
    end note

    note left of OPEN
        A permanently silent train-approach sensor is an accepted
        Core limitation (RC-11) — indistinguishable from "no
        train approaching." Shown only as this note, never as a
        detectable transition.
    end note

    note right of CLOSED
        Continued in SC-04B: train occupancy tracking and safe
        reopening once every active occupancy window has elapsed.
    end note

    note right of FAULT
        Continued in SC-04B: fault clearance requires verified
        repair and an accepted local fault-clear request, and
        returns the crossing directly to OPEN.
    end note
```

## 4.1.8 SC-04B — Railway Crossing Occupancy and Reopening

This state chart defines the second part of the railway-crossing lifecycle, entered from SC-04A after gate closure is confirmed. The gates remain closed while any registered occupancy window is active, including windows created by additional trains from either direction, and all train signals return to `STOP` before the gates begin opening (`RC-04`, `RC-05`).

```mermaid
---
title: SC-04B — Railway Crossing Occupancy and Reopening
---
stateDiagram-v2
    accTitle: SC-04B Railway Crossing Occupancy and Reopening
    accDescr: Railway-crossing operation from confirmed gate closure through overlapping train-occupancy windows to safe reopening, or a safe-side FAULT, entered from SC-04A.
    direction LR

    [*] --> CLOSED : entering from SC-04A once both gates are confirmed CLOSED

    CLOSED --> CLOSED : TRAIN_APPROACHING(other direction) / register occupancy window and set relevant train signal to PROCEED
    CLOSED --> TRAIN_PRESENT : expected arrival time reached [gates remain confirmed CLOSED] / mark occupancy window active
    CLOSED --> FAULT : gate state contradicts CLOSED / hold STOP and report fault

    TRAIN_PRESENT --> TRAIN_PRESENT : TRAIN_APPROACHING(other direction) / register occupancy window and set relevant train signal to PROCEED
    TRAIN_PRESENT --> TRAIN_PRESENT : next expected arrival reached [another occupancy window remains active] / keep gates closed
    TRAIN_PRESENT --> FAULT : gate state contradicts CLOSED / set STOP and report fault
    TRAIN_PRESENT --> OPENING : all occupancy windows expire / set STOP and command gates up

    state "RECLOSING" as RECLOSING
    state reclose_confirmation <<choice>>
    OPENING --> RECLOSING : TRAIN_APPROACHING(direction) / stop opening, keep flashers active, register occupancy window, command gates down
    RECLOSING --> reclose_confirmation : gate reports received or closing deadline reached
    reclose_confirmation --> CLOSED : [both gates confirmed CLOSED before deadline] / set relevant train signal to PROCEED
    reclose_confirmation --> FAULT : [confirmation missing or contradictory at deadline] / hold STOP and report fault
    OPENING --> OPEN : both gates confirmed OPEN / stop flashers and broadcast OPEN
    OPENING --> FAULT : gates fail to confirm OPEN / hold last confirmed safe outputs and report fault

    FAULT --> OPEN : verified repair and accepted local fault-clear request [crossing safe] / resume normal operation

    note right of OPENING
        Reopening is timer-based on the occupancy window, not on
        the reserved exit sensor, which does not participate in
        this decision (RC-05).
    end note
```

## 4.1.9 SC-05 — Central Connectivity and Local Autonomy

This state chart defines how a local intersection or railway controller responds to changes in its connection with Central. After three missed heartbeats, the controller continues autonomously with its last validated parameters; after reconnection, it reports its complete current state before accepting new Central commands (`PA-07`, `PA-08`). It supports UC-09 and UC-10.

```mermaid
---
title: SC-05 — Central Connectivity and Local Autonomy
---
stateDiagram-v2
    accTitle: SC-05 Central Connectivity and Local Autonomy
    accDescr: Connected, autonomous degraded, and resynchronising communication states for a generic local controller.
    direction LR
    [*] --> CENTRAL_CONNECTED
    CENTRAL_CONNECTED --> DEGRADED_LOCAL : three consecutive 1 s heartbeats missed / retain parameters, disable Central coordination and overrides
    DEGRADED_LOCAL --> DEGRADED_LOCAL : local schedule changes / select Peak or Off-Peak mode locally
    DEGRADED_LOCAL --> RESYNCHRONISING : heartbeat link restored / send complete current state to C1
    RESYNCHRONISING --> CENTRAL_CONNECTED : C1 accepts complete state exchange / resume Central command handling
    RESYNCHRONISING --> DEGRADED_LOCAL : heartbeat lost before exchange completes / continue autonomous operation

    note right of DEGRADED_LOCAL
        Connectivity loss alone never forces all-red,
        FLASHING_RED, or frozen timing — traffic, pedestrian,
        railway, and fault logic (SC-01 through SC-04) all
        continue locally, unaffected by this axis. Only
        FAULT_SAFE (SC-03A) produces those safe-state outputs.
    end note
```
