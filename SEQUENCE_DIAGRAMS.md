# Section 4.2 — Behavioural Specifications (Sequence Diagrams)

Companion to `STATE_CHARTS.md` (Section 4.1), `scenarios_and_written_specifications.md`, `system_assumptions_tables.md`, and `PROJECT_SPECIFICATION.md`. Produced for the Initial Design Report requirement: *"Include behavioral specifications such as sequence diagram(s) or collaboration diagrams to map out the chronological flow of actions."*

All diagrams use Mermaid `sequenceDiagram` and were validated by rendering each block with `@mermaid-js/mermaid-cli` (the same Puppeteer/Chromium-based engine behind the Mermaid Live Editor) before being written here — zero parse errors.

---

## 1. Coverage decision

Eight diagrams are sufficient. One diagram per use case is unnecessary in two cases because the *interaction pattern* (who sends what, in what order, with what ACK/NACK shape) is identical even though the triggering scenario differs:

- **UC-03 + UC-07 → SD-03**: both are "Central validates and distributes a parameter change that each `Lx` applies at its own next safe boundary." A coordination offset and a mode switch travel through the exact same `SET_TIMING_PROFILE`/`SET_MODE` → validate → ACK/NACK → apply-at-boundary → report pipeline — drawing a separate diagram for UC-07 would just repeat SD-03 with different payload names.
- **UC-09 + UC-10 → SD-08**: both live on the same heartbeat/connectivity lifeline — "monitor while connected" (UC-09) and "lose the link, run autonomously, resynchronise" (UC-10) are two phases of one continuous interaction, not two separate conversations between the local controller and Central.

The other six use cases (UC-01, UC-02, UC-04, UC-05, UC-06, UC-08) each have a genuinely distinct actor set or message pattern and get their own diagram.

---

## 2. Repository version checked

- Commit: `77f372040ad27dbf4deaa186e3ab0d58651c2a63` (2026-08-24 15:54:02 +0700), working tree clean.
- `scenarios_and_written_specifications.md` contains the approved ten-use-case version, and the IDs/names match the Section 2 baseline exactly (`UC-01`–`UC-10`).
- `PA-11` does **not** exist in `system_assumptions_tables.md` (current pedestrian/sensing/abnormal-operation range is `PA-01`–`PA-10`).

---

## 3. Contradictions or traceability gaps

Only one genuine gap, already flagged by the repository's own use-case file:

- **No assumption ID covers the `CLEAR_ROUTE` override's bounded/auto-expiring rule** (max duration, auto-expiry, early cancellation). `UC-08`'s own Business Rules already cite `PROJECT_SPECIFICATION.md` Sections 8/18/22 directly instead of an ID — this is a pre-existing gap, not something introduced here. Per this task's rule ("if required behaviour has no corresponding assumption ID, identify it as a traceability gap instead of citing the internal specification"), SD-07 marks this in-diagram and in its Constraints bullet rather than citing the Project Specification as authority.

No other unresolved contradiction was found between the official brief, `system_assumptions_tables.md`, the approved use cases, and `PROJECT_SPECIFICATION.md` for the behaviour these eight diagrams need to show.

---

## 4. SD-01 — Serve Off-Peak Vehicle Demand

*What this shows:* a vehicle demand event at a generic intersection controller during `OFF_PEAK_SENSOR` operation — from detector trip through green extension, anti-starvation, and the arterial-rest alternative when no demand exists anywhere.

```mermaid
---
title: SD-01 — Serve Off-Peak Vehicle Demand (representative Lx, applies to L1-L6)
---
sequenceDiagram
    accTitle: SD-01 Serve Off-Peak Vehicle Demand
    accDescr: Chronological flow of a vehicle demand event at a generic intersection controller Lx during OFF_PEAK_SENSOR operation, including green extension and anti-starvation.
    autonumber

    actor V as Vehicle
    participant S as Approach Sensor
    participant Lx as Intersection Controller Lx
    participant Sig as Vehicle Signals

    V->>S: vehicle arrives at approach
    S->>Lx: DEMAND_PRESENT
    Lx->>Lx: latch the demand
    Note over Lx: evaluate active mode OFF_PEAK_SENSOR and phase eligibility

    alt requested movement already has green
        Note over Lx,Sig: no clearance needed, current green continues
    else conflicting phase currently active
        Lx->>Sig: complete minimum green on the conflicting phase
        Lx->>Sig: YELLOW for 4 s
        Lx->>Sig: ALL_RED for 2 s
        Lx->>Sig: activate the requested GREEN
    end

    Note over Lx,Sig: green runs for at least 8 s

    loop every 4 s while demand persists and green below 40 s
        S-->>Lx: DEMAND_PRESENT still asserted
        Lx->>Sig: extend green by 4 s
    end

    alt demand clears before the cap
        Lx->>Sig: begin safe clearance, yellow then all-red
    else 40 s cap reached
        Lx->>Sig: begin safe clearance, yellow then all-red
    end

    Lx->>Lx: clear the served demand
    Note over Lx: normal phase selection resumes

    opt no demand anywhere at the next rest boundary
        Note over Lx,Sig: intersection rests on arterial GREEN indefinitely
    end

    Note over Lx: a latched connector demand cannot be starved past one opposing arterial service opportunity
```

- **Covers:** UC-01
- **Constraints:** `TL-01`, `TL-03`, `DP-04`, `DP-06`, `PA-04`
- **Official requirement:** p.2 ("Traffic demand and priority may differ between R1-R5; teams must state and justify the assumptions"), p.3 ("Sensor driven ... commonly used in off-peak times")

---

## 5. SD-02 — Serve a Pedestrian Crossing Request

*What this shows:* one pedestrian button press through the latched request, waiting for a compatible vehicle phase, `WALK`, and the uninterruptible clearance sequence — including coalesced repeats and a deferred/rejected Central override.

```mermaid
---
title: SD-02 — Serve a Pedestrian Crossing Request (generic Lx, every crossing side I1-I6)
---
sequenceDiagram
    accTitle: SD-02 Serve a Pedestrian Crossing Request
    accDescr: Chronological flow of a pedestrian button press through the latched request, compatible-phase WALK service, and uninterruptible clearance sequence.
    autonumber

    actor P as Pedestrian
    participant Btn as Pedestrian Button
    participant Lx as Intersection Controller Lx
    participant Sig as Vehicle Signals
    participant PedSig as Pedestrian Signal

    P->>Btn: press the crossing button
    Btn->>Lx: PED_REQUEST
    Lx->>Lx: latch one pending request

    opt button pressed again while request already pending
        Btn-->>Lx: PED_REQUEST repeated
        Note over Lx: coalesced, no additional request created
    end

    Lx->>Lx: schedule the next compatible vehicle phase

    opt railway pre-emption currently restricts the compatible phase
        Note over Lx: request remains latched, not discarded
        Note over Lx: served once a compatible phase becomes available
    end

    Lx->>Sig: compatible vehicle phase begins
    Lx->>PedSig: WALK

    opt button pressed again while WALK or clearance is active
        Btn-->>Lx: PED_REQUEST repeated
        Note over Lx: input is coalesced, no additional request is created
    end

    opt button remains stuck active beyond its diagnostic timeout
        Note over Lx: coalesced into the one existing request, a stuck-active fault is reported
    end

    Lx->>PedSig: FLASHING_DONT_WALK

    opt a Central override request arrives during clearance
        Note over Lx: the request is deferred or rejected, clearance is never truncated
    end

    Lx->>PedSig: DONT_WALK
    Lx->>Sig: conflicting vehicle movement may now receive green
    Lx->>Lx: clear the pending request
    Note over Lx: pedestrian service completes successfully
```

- **Covers:** UC-02
- **Constraints:** `TL-05`, `TL-06`, `PA-02`, `PA-03`
- **Official requirement:** p.2 ("Pedestrian buttons and pedestrian signals should also be considered if you want to target for grades from a CR level or above")

---

## 6. SD-03 — Configure and Apply an Arterial Coordination Profile

*What this shows:* Central distributing a validated timing/coordination profile to the `L1-L3-L5` arterial chain — parallel distribution and validation, ACK/NACK, boundary-gated application of offsets, and the missing-profile / railway-disruption / `SET_MODE` alternatives. `L2-L4-L6` on R2 follows the same pattern with its own offsets.

```mermaid
---
title: SD-03 — Configure and Apply an Arterial Coordination Profile (R1 chain L1-L3-L5)
---
sequenceDiagram
    accTitle: SD-03 Configure and Apply an Arterial Coordination Profile
    accDescr: Chronological flow of Central distributing a validated timing and coordination profile to the L1-L3-L5 arterial chain, including ACK-NACK handling, offset application, and fallback paths. R2's L2-L4-L6 chain follows the same pattern with its own offsets.
    autonumber

    actor Op as Central Control Room Operator
    participant C1 as Central Controller
    participant L1 as Controller L1
    participant L3 as Controller L3
    participant L5 as Controller L5
    participant Sig as Local Vehicle Signals

    Op->>C1: submit SET_TIMING_PROFILE request
    C1->>C1: validate requested network-level profile format

    par distribute parameters
        C1->>L1: timing parameters, offset 0 s
        C1->>L3: timing parameters, offset 21 s
        C1->>L5: timing parameters, offset 45 s
    end

    par L1 validates and applies
        L1->>L1: validate own parameters
        alt parameters accepted
            L1-->>C1: ACK
            Note over L1: accepted profile remains pending until the next safe phase boundary
            L1->>Sig: apply own offset, start the reference arterial green
        else parameters rejected
            L1-->>C1: NACK
            Note over L1: retains its previous valid configuration
        end
    and L3 validates and applies
        L3->>L3: validate own parameters
        alt parameters accepted
            L3-->>C1: ACK
            Note over L3: accepted profile remains pending until the next safe phase boundary
            L3->>Sig: start corresponding green after the 21 s cumulative offset
        else parameters rejected
            L3-->>C1: NACK
            Note over L3: retains its previous valid configuration
        end
    and L5 validates and applies
        L5->>L5: validate own parameters
        alt parameters accepted
            L5-->>C1: ACK
            Note over L5: accepted profile remains pending until the next safe phase boundary
            L5->>Sig: start corresponding green after the 45 s cumulative offset
        else parameters rejected
            L5-->>C1: NACK
            Note over L5: retains its previous valid configuration
        end
    end

    L1-->>C1: report active profile and phase state
    L3-->>C1: report active profile and phase state
    L5-->>C1: report active profile and phase state

    C1-->>Op: display successful coordinated operation

    opt coordination profile missing or stale at a controller
        Note over L3: continues standalone fixed or sensor-driven operation without waiting for C1
    end

    opt railway pre-emption disrupts a coordinated intersection
        Note over L1: suspends only the conflicting coordinated movement during the restriction
        Note over L1: recovers through the next valid timing profile, no separate resynchronisation state
    end

    alt operator instead submits SET_MODE
        Op->>C1: submit SET_MODE request
        C1->>L1: SET_MODE
        L1->>L1: validate against local safety constraints
        L1-->>C1: ACK or NACK
        Note over L1: applied only at the next safe phase boundary
    end

    Note over C1: Central distributes parameters and mode requests, it never commands a light colour directly
```

- **Covers:** UC-03, UC-07
- **Constraints:** `TC-01`–`TC-05`, `TL-04`, `PA-09` (plus `DP-01` for the `SET_MODE` branch)
- **Official requirement:** p.2 (distance used to determine timing requirements), p.3 ("Advanced: updateable sequence structure or timing requests from central controller"; "central controller may send a command to change the sequence a few times a day")

---

## 7. SD-04 — Protect a Railway Crossing for an Approaching Train

*What this shows:* the Pass-scope example (`RC1`/`RL1`/`L1`/`L2`) — train-approach detection, immediate warning and status distribution, gate closure, the sensor-confirmed-closure gate before `PROCEED`, occupancy-window tracking across a possible second train, and safe reopening.

```mermaid
---
title: SD-04 — Protect a Railway Crossing for an Approaching Train (RC1-RL1-L1-L2)
---
sequenceDiagram
    accTitle: SD-04 Protect a Railway Crossing for an Approaching Train
    accDescr: Chronological flow from train-approach detection through warning, gate closure confirmation, train proceed, occupancy tracking, and safe reopening at a generic railway crossing.
    autonumber

    actor Train as Train
    participant TA as Train Approach Sensor
    participant RL1 as Railway Controller RL1
    participant FL as Road-Facing Flashers
    participant BG as Boom Gates
    participant BgS as Gate Position Sensors
    participant TS as Train Signal
    participant L1 as Controller L1
    participant L2 as Controller L2
    participant C1 as Central Controller

    Train->>TA: approaches the crossing
    TA->>RL1: TRAIN_APPROACHING
    RL1->>RL1: register expected arrival and 20 s occupancy window

    par immediate warning actions
        RL1->>FL: activate flashers
    and status distribution
        RL1->>L1: WARNING
        RL1->>L2: WARNING
        RL1->>C1: WARNING
    end

    L1->>L1: clear and suppress movement toward the crossing
    L2->>L2: clear and suppress movement toward the crossing

    Note over RL1: after the flash-only warning interval
    RL1->>BG: command both gates to close
    BG->>BgS: gate position becomes reportable

    critical gate confirmation required before PROCEED
        alt both required gates confirmed CLOSED before deadline
            BgS-->>RL1: GATE_CLOSED_CONFIRMED
            RL1->>TS: PROCEED
            Note over RL1: crossing stays protected while any occupancy window remains active

            opt a second train is detected before reopening
                Train->>TA: second train approaches, other direction
                TA->>RL1: TRAIN_APPROACHING
                RL1->>RL1: register an additional occupancy window
            end

            loop while any occupancy window remains active
                Note over RL1: gates stay closed, train signal remains PROCEED for the crossing train
            end

            RL1->>TS: return to STOP
            RL1->>BG: reopen the gates
            Note over RL1: a train detected while the gates are still opening would immediately re-close them and register a new window
            RL1->>FL: stop the flashers
            RL1->>L1: OPEN
            RL1->>L2: OPEN
            RL1->>C1: OPEN
            Note over RL1: normal railway-crossing sequence completes
        else confirmation missing or contradictory by deadline
            BgS-->>RL1: GATE_FAULT or no confirmation
            RL1->>TS: hold STOP
            Note over RL1: crossing sequence terminates in the safe fault state, continued in SD-06
        end
    end

    Note over RL1: the approximately 45 s figure is the total warning-to-arrival budget, not one message delay
    Note over RL1: the reserved exit sensor is not used by Core reopening logic
```

- **Covers:** UC-04
- **Constraints:** `RC-01`–`RC-06`
- **Official requirement:** p.2 ("boom gates and red flashing lights that operate whenever a train crosses the road"; "traffic light system can sense the state of the railway crossing (but has no control over it)"; "the train should receive a red light if there are any issues with the boom gates – thus indicating an error which should be reported to the control room")

---

## 8. SD-05 — Suppress and Drain Road Traffic Around a Railway Closure

*What this shows:* both intersections adjacent to a crossing independently suppressing their toward-crossing movement during a closure, then independently checking their own binary queue detector and running a bounded drain phase after reopening.

```mermaid
---
title: SD-05 — Suppress and Drain Road Traffic Around a Railway Closure (generic RLx)
---
sequenceDiagram
    accTitle: SD-05 Suppress and Drain Road Traffic Around a Railway Closure
    accDescr: Chronological flow of the two adjacent intersection controllers suppressing the toward-crossing movement during a closure and independently running a bounded drain phase after reopening.
    autonumber

    participant RLx as Railway Controller RLx
    participant QN as Queue Detector North
    participant LN as North-side Controller Lx
    participant SigN as North-side Vehicle Signals
    participant QS as Queue Detector South
    participant LS as South-side Controller Lx
    participant SigS as South-side Vehicle Signals

    RLx->>LN: crossing state, not OPEN
    RLx->>LS: crossing state, not OPEN

    par north side reacts
        LN->>LN: check whether its toward-crossing movement is active
        opt movement active
            LN->>SigN: complete minimum green, then yellow, then all-red
        end
        LN->>SigN: hold the toward-crossing movement at RED
        LN->>SigN: continue compatible cross-traffic and away-from-crossing movements
    and south side reacts
        LS->>LS: check whether its toward-crossing movement is active
        opt movement active
            LS->>SigS: complete minimum green, then yellow, then all-red
        end
        LS->>SigS: hold the toward-crossing movement at RED
        LS->>SigS: continue compatible cross-traffic and away-from-crossing movements
    end

    par independent queue reporting
        QN-->>LN: QUEUE_WARNING, fixed threshold occupied
    and
        QS-->>LS: QUEUE_WARNING, fixed threshold occupied
    end

    RLx->>LN: crossing state, OPEN
    RLx->>LS: crossing state, OPEN

    par north side recovery
        LN->>QN: check current queue detector state
        alt QUEUE_WARNING still active
            loop every 4 s
                QN-->>LN: QUEUE_WARNING status
                LN->>SigN: extend the connector drain phase
            end
            alt warning clears before the cap
                LN->>SigN: complete required clearances
            else 60 s cap reached
                LN->>SigN: complete required clearances
            end
            LN->>SigN: resume the previous normal phase family
        else no warning present
            LN->>SigN: resume the previous normal phase family directly
        end
    and south side recovery
        LS->>QS: check current queue detector state
        alt QUEUE_WARNING still active
            loop every 4 s
                QS-->>LS: QUEUE_WARNING status
                LS->>SigS: extend the connector drain phase
            end
            alt warning clears before the cap
                LS->>SigS: complete required clearances
            else 60 s cap reached
                LS->>SigS: complete required clearances
            end
            LS->>SigS: resume the previous normal phase family
        else no warning present
            LS->>SigS: resume the previous normal phase family directly
        end
    end

    Note over QN,QS: each detector is a fixed binary trip point, it never counts vehicles
    Note over LN,LS: the traffic lights suppress feeding toward the crossing, they do not physically reroute vehicles
    Note over LN,LS: this suppression and drain logic runs inside the existing normal mode, it never creates a third demand mode
```

- **Covers:** UC-05
- **Constraints:** `CC-01`–`CC-03`, `TL-01`
- **Official requirement:** p.2 ("By measuring the distance between the traffic light intersections and train crossing you can start to determine the timing requirements ... to clear the intersections and avoid traffic backing up")

---

## 9. SD-06 — Contain and Report a Railway Equipment Fault

*What this shows:* a railway controller detecting an unverifiable gate state, forcing the safe outputs and reporting the fault as parallel (not sequential) actions, and the later verified fault-clear request cycle.

```mermaid
---
title: SD-06 — Contain and Report a Railway Equipment Fault (generic RLx)
---
sequenceDiagram
    accTitle: SD-06 Contain and Report a Railway Equipment Fault
    accDescr: Chronological flow of a railway controller detecting an unverifiable gate state, forcing the safe outputs and reporting the fault in parallel, and later processing a verified fault-clear request.
    autonumber

    participant BgS as Gate Position Sensor
    participant RLx as Railway Controller RLx
    participant TS as Train Signal
    participant BGFL as Boom Gates and Flashers
    participant Lx as Adjacent Controllers Lx
    participant C1 as Central Controller
    actor Op as Central Control Room Operator

    RLx->>BGFL: request and supervise gate closure
    BgS-->>RLx: CLOSED confirmation missing, contradictory, or faulted
    RLx->>RLx: determine the safe condition cannot be verified

    critical immediate local safety response
        par
            RLx->>TS: force or hold STOP
        and
            RLx->>BGFL: apply the documented safe outputs
        and
            RLx->>Lx: FAULT
        and
            RLx->>C1: FAULT
        end
    end

    Lx->>Lx: hold the toward-crossing movements at RED
    C1-->>Op: display the fault
    Note over RLx: the fault remains latched, normal crossing operation stays suspended

    Op->>C1: submit fault-clear request after physical repair
    C1->>RLx: forward the high-level fault-clear request
    RLx->>RLx: independently verify the physical condition

    alt condition verified resolved
        RLx-->>C1: ACK, fault cleared
        Note over RLx: crossing restored to normal readiness
    else condition still unsafe
        RLx-->>C1: NACK, safe state retained
        Note over RLx: crossing remains in the safe fault state
    end
```

The `par` block inside `critical immediate local safety response` shows `RLx->>TS` (forces `STOP`) running concurrently with, not sequenced after, `RLx->>C1` (fault report) — proving the local safety action does not wait for Central.

- **Covers:** UC-06
- **Constraints:** `RC-06`, `RC-09`, `RC-10`, `PA-09`, `PA-10`
- **Official requirement:** p.2 ("The train should receive a red light if there are any issues with the boom gates – thus indicating an error which should be reported to the control room")

---

## 10. SD-07 — Validate and Apply a Bounded Clear-Route Override

*What this shows:* a Central operator requesting a bounded clear-route override, the target controller validating it against pedestrian, railway, conflict, and fault conditions, and the bounded activation/expiry/renewal/cancellation sequence.

```mermaid
---
title: SD-07 — Validate and Apply a Bounded Clear-Route Override (generic target Lx)
---
sequenceDiagram
    accTitle: SD-07 Validate and Apply a Bounded Clear-Route Override
    accDescr: Chronological flow of a Central operator requesting a bounded clear-route override, the target controller validating it against pedestrian, railway, conflict, and fault conditions, and the bounded activation and termination sequence.
    autonumber

    actor Op as Central Control Room Operator
    participant C1 as Central Controller
    participant Lx as Target Controller Lx
    participant PedSeq as Pedestrian Sequencer
    participant RailStat as Railway Status Input
    participant Sig as Vehicle Signals

    Op->>C1: select target movement and finite duration
    C1->>Lx: REQUEST_OVERRIDE CLEAR_ROUTE, target, duration

    Lx->>Lx: validate target movement and conflict matrix
    Lx->>PedSeq: check for an active pedestrian clearance

    opt pedestrian clearance active and can be safely deferred
        Note over Lx: request is retained, unacknowledged, until clearance completes
        PedSeq-->>Lx: clearance completes
        Lx->>Lx: re-validate the retained request
        Note over Lx: if cancelled or expired while still queued, it is discarded here without being applied
    end

    Lx->>RailStat: check adjacent railway status
    Lx->>Lx: check requested duration and local fault state

    alt every check passes
        Lx-->>C1: ACK
        Lx->>Sig: complete the current protected interval and mandatory clearances
        Lx->>Sig: activate the requested through-movement

        loop until the override expires or is cancelled
            Note over Lx: the override timer is running
            opt operator validly renews before expiry
                Op->>C1: renew the override
                C1->>Lx: renew the override
                Note over Lx: the override timer restarts
            end
        end

        alt override expired naturally
            Lx->>Sig: begin mandatory clearance
        else operator cancelled early
            Op->>C1: cancel the override
            C1->>Lx: cancel the override
            Lx->>Sig: begin mandatory clearance
        end

        Lx->>Sig: safely terminate the override through mandatory clearance
        Lx->>Lx: return to the previous normal mode
        Lx-->>C1: report completion
        C1-->>Op: display the result
    else pedestrian clearance active and cannot be safely deferred
        Lx-->>C1: NACK
    else conflicting railway pre-emption is active
        Lx-->>C1: NACK
    else requested duration is invalid or over the limit
        Lx-->>C1: NACK
    else a local fault is active
        Lx-->>C1: NACK
    end

    Note over Lx: only Lx actuates its own physical traffic signals, C1 never sets a light directly

    Note over Lx,C1: traceability gap - no assumption ID such as PA-11 yet documents the bounded duration, maximum 5 minute, auto-expiry, and early-cancellation rule in system_assumptions_tables.md
```

- **Covers:** UC-08
- **Constraints:** `PA-09`. **Traceability gap:** the override's bounded/auto-expiring rule (max 5 min, auto-expiry, early cancellation) has no assumption ID yet — see Section 3.
- **Official requirement:** p.3 ("The central control room may also initiate override commands ... to deal with exceptional situations (e.g. to provide a clear path for a visiting dignitary)")

---

## 11. SD-08 — Monitor Controllers, Lose Central Link, and Resynchronise

*What this shows:* normal heartbeat monitoring, entry into `DEGRADED_LOCAL` after three missed heartbeats with continued local autonomy across sensors/clock/actuators, and the resynchronisation sequence on reconnection. One "Generic Local Controller" lifeline stands in for any `Lx` or `RLx`.

```mermaid
---
title: SD-08 — Monitor Controllers, Lose Central Link, and Resynchronise (generic Lx or RLx)
---
sequenceDiagram
    accTitle: SD-08 Monitor Controllers, Lose Central Link, and Resynchronise
    accDescr: Chronological flow of normal heartbeat monitoring, entry into DEGRADED_LOCAL after three missed heartbeats with continued local autonomy, and the resynchronisation sequence on reconnection.
    autonumber

    actor Op as Central Control Room Operator
    participant C1 as Central Controller
    participant Gx as Generic Local Controller
    participant Clk as Local Clock
    participant Sn as Local Sensors
    participant Ac as Local Actuators

    loop every 1 s while connected
        Gx->>C1: heartbeat
    end

    Gx->>C1: report state transitions and faults
    C1->>C1: update the network display
    Op->>C1: review current controller status

    Note over Gx,C1: three consecutive heartbeats are missed
    Gx->>Gx: declare the Central link unavailable, enter DEGRADED_LOCAL
    Note over Gx: Central coordination and override availability are disabled locally
    Gx->>Gx: retain the last validated timing parameters

    par local operation continues
        Clk->>Gx: continue selecting PEAK_FIXED or OFF_PEAK_SENSOR
    and
        Sn->>Gx: continue driving local behaviour
    and
        Gx->>Ac: continue operating under local authority
    and
        C1->>C1: mark the controller state as stale or unavailable
    end

    opt a railway event occurs while disconnected
        Note over Gx: the full railway-protection sequence runs to completion with zero Central dependency
    end

    alt the link remains unavailable
        Note over Gx: continues safe autonomous operation indefinitely
    else communication returns
        Gx->>C1: send the complete current state
        C1->>C1: replace the stale view with the reported actual state

        critical before accepting new commands
            Note over Gx,C1: no new Central request is accepted until state synchronisation completes
        end

        Gx->>Gx: begin accepting new Central requests
        Note over Gx,C1: normal coordination resumes
    end
```

- **Covers:** UC-09, UC-10
- **Constraints:** `PA-07`, `PA-08`, `TC-04`, `RC-10`
- **Official requirement:** p.2 ("Each local controller also communicates with a central controller (but should continue to operate if either communication link fails or the central controller goes offline)"; "the central controller does not directly control the individual lights ... must be maintained even in the event that the controller cannot communicate")

---

## 12. Use-case coverage summary

- UC-01 → SD-01
- UC-02 → SD-02
- UC-03 → SD-03
- UC-04 → SD-04
- UC-05 → SD-05
- UC-06 → SD-06
- UC-07 → SD-03
- UC-08 → SD-07
- UC-09 → SD-08
- UC-10 → SD-08

All ten approved use cases are covered by at least one diagram.

---

## 13. Final consistency and Mermaid validation check

1. All ten use cases covered — confirmed in Section 12. ✔
2. Every Main Flow reaches a complete observable outcome (SD-01 served/rest, SD-02 request served/latched, SD-03 coordinated/standalone, SD-04 `OPEN`, SD-05 resumed normal phase, SD-06 cleared/retained fault, SD-07 completed/`NACK`, SD-08 resynchronised/autonomous). ✔
3. Every `alt`/`opt` branch either rejoins the flow or ends in a named state (`NACK`, safe fault state, standalone operation). ✔
4. `C1` never appears actuating a light, gate, flasher, or train signal in any diagram — only issuing requests and receiving reports/ACK/NACK. ✔
5. No message from `L1`/`L2`/`Lx` commands `RLx`'s gates, flashers, or train signal in SD-04/05/06 — status flows one way only. ✔
6. SD-04 and SD-06 show the local `STOP`/safe-output action independent of `C1` (SD-06's `par` block explicitly). ✔
7. SD-04's `critical` block gates `PROCEED` strictly on `GATE_CLOSED_CONFIRMED`. ✔
8. SD-04's `loop while any occupancy window remains active` keeps gates down through a second train before reopening. ✔
9. SD-04/SD-05 route the toward-crossing movement through minimum green, yellow, and all-red before holding red. ✔
10. SD-02's `WALK`/`FLASHING_DONT_WALK` sequence runs straight through regardless of a concurrent Central override request, which is only ever deferred or rejected, never allowed to truncate clearance. ✔
11. SD-05's queue detectors are shown as binary triggers (`QUEUE_WARNING`), never a count. ✔
12. SD-03, SD-06, SD-07 all show a `NACK` path for an unsafe/invalid request. ✔
13. SD-08 shows `DEGRADED_LOCAL` retaining parameters while `Clk` keeps selecting `PEAK_FIXED`/`OFF_PEAK_SENSOR`. ✔
14. SD-08 shows the local controller pushing its state to `C1` before `C1` accepts new commands. ✔
15. These diagrams show message order and who-decides-what; none re-draws the state names/transitions already covered in `STATE_CHARTS.md`. ✔
16. All 8 Mermaid blocks were rendered with `@mermaid-js/mermaid-cli` (real Puppeteer/Chromium engine) before being written into this file — zero parse errors. ✔
