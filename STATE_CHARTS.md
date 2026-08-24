# Section 4.1 — Intersection State Charts

Companion to `PROJECT_SPECIFICATION.md`, `system_assumptions_tables.md`, and `scenarios_and_written_specifications.md`. Produced for the Initial Design Report requirement: *"Provide State Charts describing the lights' behavior at each intersection. You should include relevant State Chart(s) alongside your use cases."*

All charts use Mermaid `stateDiagram-v2` and render independently in the Mermaid Live Editor or any Markdown viewer with Mermaid support (e.g. GitHub, VS Code preview).

---

## 1. Coverage decision

Five diagrams are sufficient, and one generic `Lx` chart validly represents `L1–L6`, for two concrete reasons drawn from the spec rather than assumed:

- **Section 3 (Goal 5)** and **Section 8.2** require every `Lx` to run the *same* generic, config-driven controller binary — no `Lx`-specific behavioural branch exists anywhere in the spec.
- **Section 5's topology** puts every one of `I1–I6` on exactly one connector road that crosses the railway exactly once (`I1/I2↔RC1`, `I3/I4↔RC2`, `I5/I6↔RC3`). So *all six* intersections receive railway status and are subject to `RAILWAY_PREEMPTION` — there is no "railway-free" intersection that would need a different SC-03 shape.

Likewise, **RC-07** states `RL1–RL3` run *identical* logic, so one SC-04 chart covers all three, and **PA-07/PA-08** describe the connectivity axis generically for "the local controller," so one SC-05 chart covers both controller types. SC-01+SC-02+SC-03 together fully characterise any `Lx`; SC-04 fully characterises any `RLx`; SC-05 covers the connectivity axis shared by both — nothing required by the brief falls outside this set.

---

## 2. Assumptions or contradictions found

No hard contradictions. Three genuine gaps in use-case *narrative* coverage, none requiring an invented resolution because each is already dictated by an existing, decided rule — the transition is drawn and cited to what dictates it:

- **UC-08 has no dedicated ID in `system_assumptions_tables.md`.** Its own Business Rules (BR-4, BR-5) already cite `PROJECT_SPECIFICATION.md` Sections 8/18/22 directly instead of an assumption ID — this is a pre-existing, already-accepted pattern, not something new. SC-03 follows the same citation style for override-specific bullets.
- **`CENTRAL_OVERRIDE → RAILWAY_PREEMPTION`** (a train approach interrupting an active override) has no use-case alternative flow walking through it — UC-08's only interruption case (Alt 6.1) is a *fault*, not a railway event. The transition is nonetheless required by **Section 22's priority axis** (`RAILWAY_PREEMPTION` ranks above `CENTRAL_OVERRIDE`), and UC-08's own **Post-condition #2** ("returned to its prior normal mode unless a higher-priority state remains active") confirms the override does not resume afterward — that exact wording is used as the transition's action.
- **`RESYNCHRONISING → DEGRADED_LOCAL`** (link drops again before the state exchange finishes) isn't spelled out step-by-step in UC-10, but follows necessarily from **Invariant #10** ("every local controller must be able to run its full safety-relevant logic with `C1` unreachable" — applies at every instant, including mid-resync).

---

## 3. SC-01 — Generic Intersection Vehicle-Signal Phase Sequencer

*What this shows:* the six-state vehicle-light cycle every intersection runs (arterial green → yellow → all-red → connector green → yellow → all-red → repeat), drawn twice — once with `PEAK_FIXED`'s fixed 90 s timings, once with `OFF_PEAK_SENSOR`'s demand-driven extension logic — with the mode-switch boundary between the two boxes.

```mermaid
---
title: SC-01 — Generic Intersection Vehicle-Signal Phase Sequencer (applies to L1-L6)
---
stateDiagram-v2
    accTitle: SC-01 Generic Intersection Vehicle-Signal Phase Sequencer
    accDescr: Two-phase vehicle-signal cycle for a generic intersection controller Lx, shown separately for PEAK_FIXED fixed timing and OFF_PEAK_SENSOR demand-responsive timing, with the mode-switch boundary between them.
    direction LR

    [*] --> PEAK_FIXED : local clock default or SET_MODE at startup

    state "PEAK_FIXED (fixed 90 s cycle, TL-02)" as PEAK_FIXED {
        state "ARTERIAL_GREEN" as P_ARTERIAL_GREEN
        state "ARTERIAL_YELLOW" as P_ARTERIAL_YELLOW
        state "ALL_RED_A_TO_B" as P_ALL_RED_A_TO_B
        state "CONNECTOR_GREEN" as P_CONNECTOR_GREEN
        state "CONNECTOR_YELLOW" as P_CONNECTOR_YELLOW
        state "ALL_RED_B_TO_A" as P_ALL_RED_B_TO_A

        [*] --> P_ARTERIAL_GREEN
        P_ARTERIAL_GREEN --> P_ARTERIAL_YELLOW : after 48 s / begin arterial clearance
        P_ARTERIAL_YELLOW --> P_ALL_RED_A_TO_B : after 4 s / hold all approaches red
        P_ALL_RED_A_TO_B --> P_CONNECTOR_GREEN : after 2 s / release connector movement
        P_CONNECTOR_GREEN --> P_CONNECTOR_YELLOW : after 30 s / begin connector clearance
        P_CONNECTOR_YELLOW --> P_ALL_RED_B_TO_A : after 4 s / hold all approaches red
        P_ALL_RED_B_TO_A --> P_ARTERIAL_GREEN : after 2 s / release arterial movement

        note right of P_ARTERIAL_GREEN
            Ordinary approach-presence sensors are monitored
            for status/fault only, and never alter these
            durations (TL-01, TL-02). Full cycle = 54 s
            arterial + 36 s connector = 90 s.
        end note
    }

    state "OFF_PEAK_SENSOR (demand-responsive, TL-03)" as OFF_PEAK_SENSOR {
        state "ARTERIAL_GREEN" as O_ARTERIAL_GREEN
        state "ARTERIAL_YELLOW" as O_ARTERIAL_YELLOW
        state "ALL_RED_A_TO_B" as O_ALL_RED_A_TO_B
        state "CONNECTOR_GREEN" as O_CONNECTOR_GREEN
        state "CONNECTOR_YELLOW" as O_CONNECTOR_YELLOW
        state "ALL_RED_B_TO_A" as O_ALL_RED_B_TO_A

        [*] --> O_ARTERIAL_GREEN
        O_ARTERIAL_GREEN --> O_ARTERIAL_GREEN : every 4 s recheck [vehicle or latched pedestrian demand persists and green < 40 s] / extend green +4 s (TL-03)
        O_ARTERIAL_GREEN --> O_ARTERIAL_YELLOW : [green >= 8 s] and (no arterial demand or green = 40 s) and connector demand latched / begin arterial clearance
        O_ARTERIAL_YELLOW --> O_ALL_RED_A_TO_B : after 4 s / hold all approaches red
        O_ALL_RED_A_TO_B --> O_CONNECTOR_GREEN : after 2 s / release connector movement
        O_CONNECTOR_GREEN --> O_CONNECTOR_GREEN : every 4 s recheck [vehicle or latched pedestrian demand persists and green < 40 s] / extend green +4 s (TL-03)
        O_CONNECTOR_GREEN --> O_CONNECTOR_YELLOW : [green >= 8 s] and (no connector demand or green = 40 s) / begin connector clearance
        O_CONNECTOR_YELLOW --> O_ALL_RED_B_TO_A : after 4 s / hold all approaches red
        O_ALL_RED_B_TO_A --> O_ARTERIAL_GREEN : after 2 s / release arterial movement, rest here if no demand anywhere

        note right of O_ARTERIAL_GREEN
            No demand anywhere: the intersection rests on
            arterial green indefinitely (TL-03, DP-04).
            Vehicle and latched pedestrian demand are
            scheduled equally (TL-06).
        end note
        note right of O_CONNECTOR_GREEN
            Anti-starvation (DP-06): a latched connector
            request must be served after at most one
            opposing arterial service opportunity.
        end note
    }

    PEAK_FIXED --> OFF_PEAK_SENSOR : SET_MODE(OFF_PEAK_SENSOR) or time-of-day [at next completed clearance] / apply pending mode change (TL-04)
    OFF_PEAK_SENSOR --> PEAK_FIXED : SET_MODE(PEAK_FIXED) or time-of-day [at next completed clearance] / apply pending mode change (TL-04)

    note left of PEAK_FIXED
        For diagram clarity, both mode boxes are entered
        at their own ARTERIAL_GREEN. The underlying rule
        is that a mode change applies at the next completed
        clearance sequence, never mid-phase (TL-04).
        Railway pre-emption, Central override, and fault
        overlays on this sequencer are shown in SC-03,
        not repeated here.
    end note
```

---

## 4. SC-02 — Generic Pedestrian-Signal State Chart

*What this shows:* the lifecycle of one pedestrian crossing side, from an idle `DONT_WALK`, through a latched request that waits for a compatible vehicle phase, to `WALK` and its uninterruptible clearance sequence back to `DONT_WALK` — including how repeated or stuck button presses are handled.

```mermaid
---
title: SC-02 — Generic Pedestrian-Signal State Chart (every crossing side, I1-I6)
---
stateDiagram-v2
    accTitle: SC-02 Generic Pedestrian-Signal State Chart
    accDescr: Pedestrian request latching and WALK sequence for one crossing side of a generic intersection controller Lx.
    direction LR

    [*] --> DONT_WALK

    DONT_WALK --> REQUEST_LATCHED : PED_REQUEST(side) [no request pending] / latch one pending request (TL-06)
    REQUEST_LATCHED --> REQUEST_LATCHED : PED_REQUEST(side) [request already pending] / coalesce, no new request created (TL-06)
    REQUEST_LATCHED --> REQUEST_LATCHED : PED_REQUEST(side) asserted beyond diagnostic timeout [stuck-active] / coalesce to one request, raise fault (PA-03)
    REQUEST_LATCHED --> WALK : compatible vehicle phase begins [movement not conflicting with this crossing] / display WALK (TL-05)
    WALK --> FLASHING_DONT_WALK : after WALK interval / begin pedestrian clearance
    FLASHING_DONT_WALK --> DONT_WALK : after clearance interval / clear latched request, resume DONT_WALK

    note right of REQUEST_LATCHED
        A request arriving while RAILWAY_PREEMPTION
        suppresses the compatible vehicle movement
        remains latched, not discarded (PA-02) - served
        once a compatible phase is available.
    end note

    note right of WALK
        Once WALK has started, WALK and the following
        FLASHING_DONT_WALK clearance cannot be
        truncated by any mode, including
        CENTRAL_OVERRIDE (Invariants #4 and #12, see SC-03).
    end note

    note left of DONT_WALK
        A permanently silent push-button is not detectable
        by the Core design (PA-03) - indistinguishable
        from "no request." Not shown as a transition.
    end note
```

---

## 5. SC-03 — Intersection Supervisory and Safety Overlay State Chart

*What this shows:* the authority layer that sits above SC-01's phase cycle — which of `NORMAL_OPERATION`, `CENTRAL_OVERRIDE`, `RAILWAY_PREEMPTION`, or `FAULT_SAFE` is in charge at any moment, and the fixed priority order between them (fault beats railway, railway beats override, override beats normal).

```mermaid
---
title: SC-03 — Intersection Supervisory and Safety Overlay State Chart (generic Lx)
---
stateDiagram-v2
    accTitle: SC-03 Intersection Supervisory and Safety Overlay
    accDescr: High-level mode authority for a generic intersection controller Lx, showing FAULT_SAFE, RAILWAY_PREEMPTION, CENTRAL_OVERRIDE and NORMAL_OPERATION without repeating the detailed phase sequence from SC-01.
    direction TB

    [*] --> NORMAL_OPERATION

    state "NORMAL_OPERATION (baseline mode, priority 4)" as NORMAL_OPERATION {
        [*] --> PEAK_FIXED
        PEAK_FIXED --> OFF_PEAK_SENSOR : SET_MODE or time-of-day [at safe phase boundary] / apply pending change (TL-04)
        OFF_PEAK_SENSOR --> PEAK_FIXED : SET_MODE or time-of-day [at safe phase boundary] / apply pending change (TL-04)
        note right of PEAK_FIXED
            Detailed phase-by-phase sequencing for both
            sub-modes is shown in SC-01, not repeated here.
        end note
    }

    state override_choice <<choice>>
    NORMAL_OPERATION --> override_choice : REQUEST_OVERRIDE(CLEAR_ROUTE, target, duration)
    override_choice --> CENTRAL_OVERRIDE : [bounded valid duration, no conflicting RAILWAY_PREEMPTION, pedestrian clearance not in progress or safely deferrable] / ACK
    override_choice --> NORMAL_OPERATION : [unsafe, unbounded duration, or clearance cannot be safely deferred] / NACK

    state "CENTRAL_OVERRIDE (bounded, auto-expiring, priority 3)" as CENTRAL_OVERRIDE
    CENTRAL_OVERRIDE --> NORMAL_OPERATION : duration expiry or operator cancel / restore prior normal mode (Section 18)

    NORMAL_OPERATION --> RAILWAY_PREEMPTION : RLx reports WARNING/CLOSING/CLOSED for the adjacent crossing / suppress only the toward-crossing movement through yellow + all-red to red
    CENTRAL_OVERRIDE --> RAILWAY_PREEMPTION : RLx reports WARNING/CLOSING/CLOSED for the adjacent crossing / override terminated, toward-crossing movement suppressed through yellow + all-red to red (Section 22 priority axis)
    RAILWAY_PREEMPTION --> RAILWAY_PREEMPTION : REQUEST_OVERRIDE(CLEAR_ROUTE) [conflicts with active pre-emption] / NACK (UC-08 Alt 3.2)
    RAILWAY_PREEMPTION --> NORMAL_OPERATION : crossing reports OPEN and drain phase completes / resume previous phase family, override not resumed (UC-08 Post-condition 2)

    state "FAULT_SAFE (absolute local veto, priority 1)" as FAULT_SAFE
    NORMAL_OPERATION --> FAULT_SAFE : local hardware conflict or watchdog trip / all-way FLASHING_RED, all crossings DONT_WALK
    CENTRAL_OVERRIDE --> FAULT_SAFE : local hardware conflict or watchdog trip / all-way FLASHING_RED, all crossings DONT_WALK
    RAILWAY_PREEMPTION --> FAULT_SAFE : local hardware conflict or watchdog trip / all-way FLASHING_RED, all crossings DONT_WALK
    FAULT_SAFE --> NORMAL_OPERATION : explicit local fault-clear request [verified repair confirmed] / resume normal operation

    note right of RAILWAY_PREEMPTION
        Suppression is directional, not intersection-wide:
        only the movement toward the affected crossing is
        forced red, while compatible cross-traffic, away-from-
        crossing movements, and pedestrian phases (SC-02)
        continue unaffected.
    end note

    note right of override_choice
        If a pedestrian clearance is in progress but can be
        safely deferred, ACK/entry is delayed until clearance
        completes rather than issued immediately
        (UC-08 Alt 3.1) - clearance is never truncated.
    end note

    note right of FAULT_SAFE
        Fault clearance is explicit and never automatic.
        DEGRADED_LOCAL (Central connectivity loss) is not
        part of this hierarchy - see SC-05.
    end note
```

---

## 6. SC-04 — Generic Railway-Crossing Lifecycle

*What this shows:* the full life of one railway crossing — from `OPEN`, through the train-approach warning, gate closing and sensor-confirmed closure, the train's occupancy window(s), reopening, and back to `OPEN` — with every unconfirmed or contradictory condition routing to a safe `FAULT` state.

```mermaid
---
title: SC-04 — Generic Railway-Crossing Lifecycle (applies to RL1-RL3)
---
stateDiagram-v2
    accTitle: SC-04 Generic Railway-Crossing Lifecycle
    accDescr: Full crossing lifecycle for a generic railway-crossing controller RLx, from OPEN through WARNING, CLOSING, CLOSED, TRAIN_PRESENT, OPENING, and back to OPEN, with FAULT reachable from any unconfirmed or contradictory condition.
    direction LR

    [*] --> OPEN

    OPEN --> WARNING : TRAIN_APPROACHING(dir) / activate flashers immediately, notify adjacent Lx and C1, register warning-to-arrival budget (~45 s, RC-03)

    WARNING --> CLOSING : after flash-only warning interval (~5 s) / command both required gate arms down
    WARNING --> FAULT : TRAIN_APPROACHING(dir) asserted beyond diagnostic timeout with no train ever confirmed clear [stuck-active] / hold gates down, train signal STOP, raise fault (RC-11)

    state gate_confirm <<choice>>
    CLOSING --> gate_confirm : gate-position sensors report, checked against deadline
    gate_confirm --> CLOSED : [both required gates GATE_CLOSED_CONFIRMED before deadline]
    gate_confirm --> FAULT : [missing or contradictory confirmation by deadline] / hold train signal STOP, report fault in parallel to C1 (RC-06, RC-10)

    CLOSED --> TRAIN_PRESENT : [gate confirmed CLOSED] / train signal shows PROCEED (RC-06)
    CLOSED --> FAULT : gate position report contradicts CLOSED before train arrival / hold train signal STOP, report fault in parallel to C1 (RC-06, RC-10)

    TRAIN_PRESENT --> TRAIN_PRESENT : TRAIN_APPROACHING(other direction) [second train detected before reopening] / register additional occupancy window (RC-04)
    TRAIN_PRESENT --> FAULT : gate position report contradicts CLOSED while an occupancy window is active / train signal returns to STOP, report fault in parallel to C1 (RC-06, RC-10)
    TRAIN_PRESENT --> OPENING : every active occupancy window elapsed (RC-04) / train signal returns to STOP, command gates up

    OPENING --> OPEN : both gates GATE_OPEN_CONFIRMED / stop flashers, broadcast OPEN
    OPENING --> FAULT : gate fails to confirm OPEN / hold last-confirmed safe outputs, report fault

    FAULT --> OPEN : verified physical repair and accepted local fault-clear request [crossing confirmed safe] / resume normal operation (never automatic)

    note right of WARNING
        The ~45 s figure (RC-03) is a total warning-to-
        arrival budget (flash-lead + gate-closing +
        confirmation margin + adjacent-intersection
        clearance), not the duration of any single state.
    end note

    note right of TRAIN_PRESENT
        A gate must not reopen while any occupancy window
        remains active, including one from a second train
        (Invariant #8). Reopening is timer-based, and the
        reserved exit sensor [Ex] does not participate in
        this decision (RC-04, RC-05).
    end note

    note left of OPEN
        A permanently silent train-approach sensor is an
        accepted Core limitation (RC-11) - indistinguishable
        from "no train approaching." Not shown as a
        detectable transition.
    end note
```

---

## 7. SC-05 — Central Connectivity and Local Autonomy

*What this shows:* the heartbeat-driven link state between a local controller and Central — normal `CENTRAL_CONNECTED` operation, dropping into autonomous `DEGRADED_LOCAL` after three missed heartbeats, and `RESYNCHRONISING` on reconnection — kept deliberately separate from the hazard hierarchy in SC-03.

```mermaid
---
title: SC-05 — Central Connectivity and Local Autonomy (generic Lx or RLx)
---
stateDiagram-v2
    accTitle: SC-05 Central Connectivity and Local Autonomy
    accDescr: Connectivity states for a generic local controller, showing heartbeat-based detection of Central link loss, continued local autonomous operation, and resynchronisation on reconnection. Orthogonal to the mode/hazard hierarchy in SC-03 and the crossing lifecycle in SC-04.
    direction LR

    [*] --> CENTRAL_CONNECTED

    CENTRAL_CONNECTED --> DEGRADED_LOCAL : 3 consecutive missed 1 s heartbeats (~3 s) / retain last validated timing parameters, disable Central coordination and override availability (PA-07)

    DEGRADED_LOCAL --> DEGRADED_LOCAL : local clock mode re-evaluation [PEAK_FIXED or OFF_PEAK_SENSOR selection criteria met] / continue local mode selection using retained parameters (PA-07)

    DEGRADED_LOCAL --> RESYNCHRONISING : heartbeat link restored / begin sending complete current state to C1 (PA-08)

    RESYNCHRONISING --> CENTRAL_CONNECTED : state exchange accepted by C1 [complete] / resume accepting new Central requests and coordination/override availability (PA-08)

    RESYNCHRONISING --> DEGRADED_LOCAL : heartbeat link lost again before state exchange completes / remain in autonomous local operation (Invariant #10)

    note right of DEGRADED_LOCAL
        Traffic (SC-01), pedestrian (SC-02), railway
        (SC-04), and FAULT_SAFE logic (SC-03) continue
        locally, unaffected by this axis. Connectivity loss
        alone never forces all-red, FLASHING_RED, or
        frozen operation - only FAULT_SAFE does that.
    end note

    note left of CENTRAL_CONNECTED
        Heartbeat exchanged every 1 s while connected
        (PA-07). This axis is orthogonal to the mode/
        hazard hierarchy in SC-03: DEGRADED_LOCAL is a
        connectivity condition, not a hazard-priority mode.
    end note

    note right of RESYNCHRONISING
        New Central requests (SET_MODE, SET_TIMING_
        PROFILE, REQUEST_OVERRIDE) are not accepted
        until the state exchange completes (PA-08).
    end note
```

---

## 8. Use-case-to-state-chart mapping

- SC-01 → UC-01 (Serve Vehicle Demand), UC-07 (Configure Traffic Operating Parameters)
- SC-02 → UC-02 (Serve Pedestrian Crossing Request), UC-08 (Apply a Bounded Clear-Route Override)
- SC-03 → UC-05 (Manage Road Traffic During and After a Railway Closure), UC-06 (Respond to a Railway Equipment Fault), UC-07 (Configure Traffic Operating Parameters), UC-08 (Apply a Bounded Clear-Route Override)
- SC-04 → UC-04 (Protect a Railway Crossing for an Approaching Train), UC-06 (Respond to a Railway Equipment Fault)
- SC-05 → UC-09 (Monitor Network Status and Faults), UC-10 (Continue Local Operation During Central Link Loss)

The repository's current numbering (`UC-01`–`UC-10` in `scenarios_and_written_specifications.md`) already matches this mapping exactly — no renumbering discrepancy to report.

---

## 9. Final consistency check

1. Every state/transition traced above to a specific spec section or assumption ID — no invented states, timings, signals, sensors, or commands. ✔
2. Yellow + all-red clearance preserved in SC-01's core cycle; SC-03 explicitly routes `RAILWAY_PREEMPTION` and override termination "through yellow + all-red to red," never a direct green→red jump. ✔
3. `C1` never appears as an actuator anywhere — only as the source of `SET_MODE`/`SET_TIMING_PROFILE`/`REQUEST_OVERRIDE` events consumed by `Lx`'s own state machine, and as a passive report recipient in SC-04/SC-05. ✔
4. SC-04 has zero transition guarded on `C1` reachability; SC-05 shows railway/traffic/pedestrian logic (SC-01/02/04) continuing unaffected through `DEGRADED_LOCAL`. ✔
5. SC-04's `TRAIN_PRESENT` self-loop accumulates occupancy windows; `OPENING` is only reached when "every active occupancy window elapsed." ✔
6. SC-04's `CLOSED → TRAIN_PRESENT` (where `PROCEED` is issued) is guarded strictly on `gate confirmed CLOSED`; the `gate_confirm` choice routes anything else to `FAULT`. ✔
7. SC-02's `WALK → FLASHING_DONT_WALK → DONT_WALK` has no incoming interrupt edges; SC-03's `override_choice` explicitly defers/NACKs rather than truncating. ✔
8. SC-03 contains no reference to `DEGRADED_LOCAL`; it lives only in SC-05, with an explicit note marking the two hierarchies as orthogonal. ✔
9. No diagram models the advance/queue sensor `[Q]` as anything other than a binary trigger — SC-01/SC-05 don't reference it at all, keeping it out of scope for these five charts. ✔
10. Each diagram is a single flat or lightly-nested chart (one composite level, ≤2 composites per diagram) with concise event/guard/action labels, sized for A4 legibility. ✔
