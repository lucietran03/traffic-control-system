# 4.2 Behavioural Specifications

The following sequence diagrams show the chronological interactions that realise the principal use cases defined in Section 3.2. Eight diagrams cover all ten use cases because UC-03 and UC-07 share the same configuration-distribution interaction, while UC-09 and UC-10 form one continuous monitoring, disconnection, and reconnection sequence. State transitions internal to individual controllers are defined separately in Section 4.1.

## 4.2.1 SD-01 — Serve Off-Peak Vehicle Demand

This sequence shows how a generic intersection controller serves vehicle demand in `OFF_PEAK_SENSOR`. It includes demand latching, mandatory clearance intervals, bounded green extension, anti-starvation, and the arterial rest condition (`TL-01`, `TL-03`, `DP-04`, `DP-06`, `PA-04`). It realises UC-01.

```mermaid
---
title: SD-01 — Serve Off-Peak Vehicle Demand
---
sequenceDiagram
    accTitle: SD-01 Serve Off-Peak Vehicle Demand
    accDescr: Vehicle demand is detected, safely served, extended while present, and cleared by a generic intersection controller operating in Off-Peak Sensor mode.
    autonumber

    actor V as Vehicle
    participant S as Approach Sensor
    participant Lx as Intersection Controller
    participant Sig as Vehicle Signals

    V->>S: arrive at approach
    S->>Lx: DEMAND_PRESENT(approach)
    Lx->>Lx: latch demand and evaluate phase eligibility

    alt requested movement already has green
        Lx->>Sig: retain current GREEN
    else conflicting phase is active
        Lx->>Sig: complete minimum GREEN
        Lx->>Sig: display YELLOW for 4 s
        Lx->>Sig: display ALL_RED for 2 s
        Lx->>Sig: display requested GREEN
    end

    loop every 4 s while demand remains and green is below 40 s
        S-->>Lx: DEMAND_PRESENT
        Lx->>Sig: extend GREEN by 4 s
    end

    alt demand clears after minimum green
        S-->>Lx: DEMAND_CLEAR
    else maximum green reaches 40 s
        Lx->>Lx: enforce phase termination
    end

    Lx->>Sig: display YELLOW for 4 s
    Lx->>Sig: display ALL_RED for 2 s
    Lx->>Lx: clear served demand and select next phase

    opt no demand remains at the rest boundary
        Lx->>Sig: hold arterial GREEN
    end

    Note over Lx: a pending connector demand cannot be starved past one further arterial session (DP-06)
```

## 4.2.2 SD-02 — Serve a Pedestrian Crossing Request

This sequence shows the complete lifecycle of one pedestrian request. The request is latched until a compatible phase is available, repeated presses are coalesced, and the full pedestrian clearance completes before a conflicting vehicle movement may begin (`TL-05`, `TL-06`, `PA-02`, `PA-03`). It realises UC-02 and the pedestrian-safety constraint used by UC-08.

```mermaid
---
title: SD-02 — Serve a Pedestrian Crossing Request
---
sequenceDiagram
    accTitle: SD-02 Serve a Pedestrian Crossing Request
    accDescr: A pedestrian request is latched, served with a compatible vehicle phase, and completed through the full pedestrian clearance sequence.
    autonumber

    actor P as Pedestrian
    participant B as Pedestrian Button
    participant Lx as Intersection Controller
    participant VS as Vehicle Signals
    participant PS as Pedestrian Signal

    P->>B: press crossing button
    B->>Lx: PED_REQUEST(side)
    Lx->>Lx: latch one pending request

    opt button is pressed again before service completes
        B-->>Lx: PED_REQUEST(side)
        Lx->>Lx: coalesce repeated request
    end

    opt button remains active beyond diagnostic timeout
        Lx->>Lx: retain the one latched request and report a stuck-active fault
    end

    alt compatible vehicle phase is available
        Lx->>VS: begin compatible vehicle phase
    else railway restriction blocks that phase
        Lx->>Lx: retain request until restriction clears
        Lx->>VS: begin next compatible vehicle phase
    end

    Lx->>PS: display WALK

    opt a Central override request arrives during WALK or clearance
        Note over Lx: the request is deferred or rejected, clearance is never truncated
    end

    Lx->>PS: display FLASHING_DONT_WALK
    Lx->>PS: display DONT_WALK
    Lx->>Lx: clear pending request
    Lx->>VS: permit conflicting movement when otherwise eligible
```

## 4.2.3 SD-03 — Configure and Apply an Arterial Coordination Profile

This sequence shows Central distributing a validated timing profile to an arterial controller chain. Each local controller independently validates its assigned offset, returns `ACK` or `NACK`, applies an accepted profile only at a safe phase boundary, and reports its resulting state (`TC-01`–`TC-05`, `TL-04`, `PA-09`). The same interaction carries a `SET_MODE` request for UC-07. It realises UC-03 and UC-07.

```mermaid
---
title: SD-03 — Configure and Apply an Arterial Coordination Profile
---
sequenceDiagram
    accTitle: SD-03 Configure and Apply an Arterial Coordination Profile
    accDescr: Central distributes a timing profile to an arterial chain and each local controller validates, acknowledges, safely applies, and reports its assigned parameters.
    autonumber

    actor Op as Control Room Operator
    participant C1 as Central Controller
    participant Lx as Each Arterial Controller
    participant Sig as Locally Owned Signals

    Op->>C1: submit SET_TIMING_PROFILE(profile)
    C1->>C1: validate profile structure and target chain

    loop for each Lx in the selected arterial chain, independently and concurrently
        C1->>Lx: SET_TIMING_PROFILE(parameters, assigned offset)
        Lx->>Lx: validate local timing and safety bounds
        alt parameters are valid
            Lx-->>C1: ACK(accepted, pending safe boundary)
            Lx->>Lx: wait for next safe phase boundary
            Lx->>Sig: apply accepted timing and offset
            Lx-->>C1: STATUS(profile active, phase state)
        else parameters are invalid or unsafe
            Lx-->>C1: NACK(reason)
            Lx->>Lx: retain previous valid configuration
        end
    end

    Note over C1,Sig: worked example on R1 - L1 offset 0 s, L3 offset 21 s, L5 offset 45 s cumulative (TC-01, TC-02)

    C1-->>Op: display per-controller results

    opt a controller's profile is missing or goes stale
        Lx->>Lx: continue standalone fixed or sensor-driven timing without waiting for C1
    end

    opt railway pre-emption disrupts a coordinated intersection
        Lx->>Lx: suspend only the conflicting coordinated movement for the restriction
        Lx->>Lx: recover coordination through the next valid timing profile, no separate resynchronisation state
    end

    opt operator requests a mode change
        Op->>C1: submit SET_MODE(mode)
        C1->>Lx: SET_MODE(mode)
        Lx->>Lx: validate request and wait for safe boundary
        alt request accepted
            Lx-->>C1: ACK(accepted, pending safe boundary)
            Lx->>Sig: apply selected mode at safe boundary
            Lx-->>C1: STATUS(mode active)
        else request rejected
            Lx-->>C1: NACK(reason)
        end
    end
```

## 4.2.4 SD-04 — Protect a Railway Crossing for an Approaching Train

This sequence shows the complete protection cycle at one railway crossing. The railway controller activates warnings locally, informs the adjacent intersections, requires physical gate-closed confirmation before displaying `PROCEED`, tracks overlapping train-occupancy windows, and confirms that the gates are open before ending the warning (`RC-01`–`RC-06`). It realises UC-04.

```mermaid
---
title: SD-04 — Protect a Railway Crossing for an Approaching Train
---
sequenceDiagram
    accTitle: SD-04 Protect a Railway Crossing for an Approaching Train
    accDescr: A railway controller detects an approaching train, closes and verifies the gates, protects overlapping occupancy windows, and safely reopens the crossing.
    autonumber

    actor T as Train
    participant AS as Approach Sensor
    participant RLx as Railway Controller
    participant FL as Flashing Lights
    participant BG as Boom Gates
    participant GS as Gate Sensors
    participant TS as Train Signals
    participant Lx as Adjacent Intersection Controllers
    participant C1 as Central Controller

    T->>AS: approach crossing
    AS->>RLx: TRAIN_APPROACHING(direction)
    RLx->>RLx: register expected arrival and occupancy window

    par activate local warning
        RLx->>FL: START_FLASHING
    and distribute crossing status
        RLx->>Lx: CROSSING_STATUS(WARNING)
        RLx->>C1: CROSSING_STATUS(WARNING)
    end

    Lx->>Lx: safely suppress toward-crossing movements
    RLx->>BG: CLOSE after 5 s warning interval
    GS-->>RLx: gate-position status

    alt both gates confirm CLOSED before deadline
        RLx->>TS: PROCEED for detected direction
        RLx->>Lx: CROSSING_STATUS(CLOSED)
        RLx->>C1: CROSSING_STATUS(CLOSED)

        opt another train is detected before reopening
            AS->>RLx: TRAIN_APPROACHING(other direction)
            RLx->>RLx: register additional occupancy window
            RLx->>TS: PROCEED for additional direction
        end

        loop while any occupancy window remains active
            RLx->>RLx: retain gates closed and monitor gate status
        end

        RLx->>TS: STOP
        RLx->>BG: OPEN
        GS-->>RLx: gate-position status

        alt both gates confirm OPEN
            RLx->>FL: STOP_FLASHING
            RLx->>Lx: CROSSING_STATUS(OPEN)
            RLx->>C1: CROSSING_STATUS(OPEN)
        else gates fail to confirm OPEN
            RLx->>RLx: hold last confirmed safe outputs
            RLx->>Lx: CROSSING_STATUS(FAULT)
            RLx->>C1: FAULT_REPORT(gate confirmation)
        end
    else confirmation is missing or contradictory
        par immediate local response
            RLx->>TS: hold STOP
        and fault notification
            RLx->>Lx: CROSSING_STATUS(FAULT)
            RLx->>C1: FAULT_REPORT(gate confirmation)
        end
    end

    Note over RLx: the approximately 45 s warning-to-arrival figure is a total budget, not the duration of one message
    Note over RLx: reopening is timer-based on the occupancy window, not on the reserved exit sensor
```

## 4.2.5 SD-05 — Suppress and Drain Road Traffic Around a Railway Closure

This sequence shows the behaviour independently executed by each of the two intersection controllers adjacent to a railway crossing. A controller safely suppresses its toward-crossing movement while the crossing is unavailable, then uses its own binary queue detector to run a drain phase in 4-second increments, capped at 60 seconds (`CC-01`–`CC-03`, `TL-01`). It realises UC-05.

```mermaid
---
title: SD-05 — Suppress and Drain Road Traffic Around a Railway Closure
---
sequenceDiagram
    accTitle: SD-05 Suppress and Drain Road Traffic Around a Railway Closure
    accDescr: Each adjacent intersection controller suppresses its toward-crossing movement and independently performs a bounded queue-drain phase after reopening.
    autonumber

    participant RLx as Railway Controller
    participant Lx as Each Adjacent Intersection Controller
    participant Sig as Locally Owned Signals
    participant Q as Local Binary Queue Detector

    RLx->>Lx: CROSSING_STATUS(not OPEN)
    Lx->>Lx: check toward-crossing movement

    opt movement is currently active
        Lx->>Sig: complete minimum GREEN
        Lx->>Sig: display YELLOW for 4 s
        Lx->>Sig: display ALL_RED for 2 s
    end

    Lx->>Sig: hold toward-crossing movement at RED
    Lx->>Sig: continue non-conflicting movements
    Q-->>Lx: QUEUE_WARNING(active or clear)
    Note over Q,Lx: the same binary detector already feeds pre-emption suppression while the crossing is closed (CC-01)

    RLx->>Lx: CROSSING_STATUS(OPEN)
    Lx->>Q: request current binary status
    Q-->>Lx: QUEUE_WARNING status

    alt queue warning is active
        Lx->>Sig: begin connector drain phase
        loop every 4 s while warning remains and drain is below 60 s
            Q-->>Lx: QUEUE_WARNING(active)
            Lx->>Sig: extend drain by 4 s
        end
        Lx->>Sig: complete required clearances
    else queue warning is clear
        Lx->>Lx: skip extended drain
    end

    Lx->>Sig: resume previous normal phase family
```

## 4.2.6 SD-06 — Contain and Report a Railway Equipment Fault

This sequence shows a railway controller responding to an unverifiable gate condition. The train signal is forced to `STOP` locally while the fault is reported in parallel, ensuring that the safety response never depends on Central communication; normal operation resumes only after verified repair and an accepted fault-clear request (`RC-06`, `RC-09`, `RC-10`, `PA-09`, `PA-10`). It realises UC-06.

```mermaid
---
title: SD-06 — Contain and Report a Railway Equipment Fault
---
sequenceDiagram
    accTitle: SD-06 Contain and Report a Railway Equipment Fault
    accDescr: A railway controller applies local safe outputs and reports a gate fault in parallel before processing a later verified fault-clear request.
    autonumber

    participant GS as Gate Sensor
    participant RLx as Railway Controller
    participant TS as Train Signals
    participant BGFL as Gates and Flashers
    participant Lx as Adjacent Intersection Controllers
    participant C1 as Central Controller
    actor Op as Control Room Operator

    RLx->>BGFL: command and supervise gate closure
    GS-->>RLx: missing or contradictory CLOSED status
    RLx->>RLx: latch local railway fault

    par immediate local safety actions
        RLx->>TS: force or hold STOP
        RLx->>BGFL: hold documented safe outputs
    and independent notifications
        RLx->>Lx: CROSSING_STATUS(FAULT)
        RLx->>C1: FAULT_REPORT(details)
    end

    Lx->>Lx: hold toward-crossing movements at RED
    C1-->>Op: display and log fault

    Op->>C1: request fault clearance after repair
    C1->>RLx: REQUEST_FAULT_CLEAR
    RLx->>RLx: verify physical condition locally

    alt condition is verified safe
        RLx-->>C1: ACK(fault cleared)
        RLx->>RLx: return to normal readiness
    else condition remains unsafe
        RLx-->>C1: NACK(reason)
        RLx->>RLx: retain safe fault state
    end
```

## 4.2.7 SD-07 — Validate and Apply a Bounded Clear-Route Override

This sequence shows a Central clear-route request being validated and executed by the target intersection controller. The local controller retains exclusive actuation authority, rejects unsafe requests, never truncates pedestrian clearance, and terminates an accepted override through the required vehicle-signal clearance sequence (`PA-09`, UC-08). It realises UC-08.

```mermaid
---
title: SD-07 — Validate and Apply a Bounded Clear-Route Override
---
sequenceDiagram
    accTitle: SD-07 Validate and Apply a Bounded Clear-Route Override
    accDescr: Central requests a bounded clear-route override and the target controller validates, safely applies, and automatically or manually terminates it.
    autonumber

    actor Op as Control Room Operator
    participant C1 as Central Controller
    participant Lx as Target Intersection Controller
    participant Ped as Pedestrian Sequencer
    participant Rail as Railway Status Input
    participant Sig as Locally Owned Signals

    Op->>C1: select target and finite duration
    C1->>Lx: REQUEST_OVERRIDE(CLEAR_ROUTE, target, duration)
    Lx->>Lx: validate target, duration, conflicts, and fault state
    Lx->>Rail: read adjacent crossing status
    Rail-->>Lx: current status
    Lx->>Ped: read pedestrian-clearance status
    Ped-->>Lx: current status

    opt pedestrian clearance is active and can be safely deferred
        Note over Lx: request retained, unacknowledged, until clearance completes
        Ped-->>Lx: clearance complete
        Lx->>Lx: revalidate the retained request
    end

    alt request is unsafe, conflicts with railway pre-emption, or pedestrian clearance cannot be safely deferred
        Lx-->>C1: NACK(reason)
        C1-->>Op: display rejection
    else request is accepted
        Lx-->>C1: ACK(accepted, pending safe boundary)
        Lx->>Sig: complete vehicle minimums and clearances
        Lx->>Sig: activate requested through-movement
        Lx-->>C1: STATUS(override active)

        loop until the override expires or is cancelled
            opt operator validly renews before expiry
                Op->>C1: RENEW_OVERRIDE
                C1->>Lx: RENEW_OVERRIDE
                Lx->>Lx: restart the override timer
            end
        end

        alt override reaches its expiry time
            Lx->>Sig: terminate through mandatory clearance
        else operator cancels active override
            Op->>C1: CANCEL_OVERRIDE
            C1->>Lx: CANCEL_OVERRIDE
            Lx->>Sig: terminate through mandatory clearance
        end

        Lx->>Lx: restore previous normal mode
        Lx-->>C1: STATUS(override complete)
        C1-->>Op: display completion
    end

    Note over Lx,C1: traceability gap - no assumption ID such as PA-11 yet documents the bounded 5-minute maximum, auto-expiry, and renewal rule in system_assumptions_tables.md
```

## 4.2.8 SD-08 — Monitor Controllers, Lose Central Link, and Resynchronise

This sequence combines routine Central monitoring with loss and restoration of communication. Each local controller enters `DEGRADED_LOCAL` after three missed heartbeat acknowledgements, continues its complete local control logic using retained parameters, and sends its current state to Central before accepting new commands after reconnection (`PA-07`, `PA-08`, `TC-04`, `RC-10`). It realises UC-09 and UC-10.

```mermaid
---
title: SD-08 — Monitor Controllers, Lose Central Link, and Resynchronise
---
sequenceDiagram
    accTitle: SD-08 Monitor Controllers, Lose Central Link, and Resynchronise
    accDescr: A local controller is monitored while connected, continues autonomously after heartbeat loss, and resynchronises its current state before accepting new Central commands.
    autonumber

    actor Op as Control Room Operator
    participant C1 as Central Controller
    participant Gx as Generic Local Controller
    participant Clk as Local Clock
    participant Sn as Local Sensors
    participant Ac as Local Actuators

    loop every 1 s while connected
        Gx->>C1: HEARTBEAT(local status summary)
        C1-->>Gx: HEARTBEAT_ACK
    end

    Gx->>C1: STATUS(state transition or fault)
    C1->>C1: update network view and log
    C1-->>Op: display current controller status

    loop three consecutive heartbeat intervals
        Gx->>C1: HEARTBEAT
        C1--xGx: no HEARTBEAT_ACK received
    end

    Gx->>Gx: enter DEGRADED_LOCAL and retain parameters
    C1->>C1: mark controller unavailable

    par autonomous local operation
        Clk->>Gx: update Peak or Off-Peak selection
    and local input processing
        Sn->>Gx: local sensor event
    and local actuation
        Gx->>Ac: apply locally authorised output
    end

    opt a railway event occurs while disconnected
        Note over Gx: the full railway-protection sequence runs to completion with zero Central dependency (RC-10)
    end

    alt communication remains unavailable
        Gx->>Gx: continue autonomous operation
    else communication is restored
        Gx->>Gx: enter RESYNCHRONISING
        Gx->>C1: HEARTBEAT and complete current state
        C1-->>Gx: STATE_SYNC_ACCEPTED
        Gx->>Gx: exit DEGRADED_LOCAL
        Gx->>Gx: resume accepting Central requests
        C1-->>Op: display resynchronised state
    end
```
