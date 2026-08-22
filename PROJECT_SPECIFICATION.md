# Traffic Light Control System — Project Specification

**Course:** EEET2588 Real-Time Systems — Design Project
**Status:** Living document — single source of truth for the project. Supersedes `README.md` and `idea.md` where they conflict with this file.
**Purpose:** Define *what system we are building* before any Initial Design Report, UML diagram, QNX architecture, or code is produced.

Every design item below is tagged with one of the following provenance labels:

| Tag | Meaning |
|---|---|
| **[REQ]** | Official Requirement — stated directly in `RTS_Final Project.pdf` |
| **[CLARIF]** | Lecturer Clarification — stated in `lecture_clarification.md` |
| **[TEAM]** | Team Design Decision — a firm choice made in this document to resolve an open point the brief leaves free |
| **[ASSUM]** | Engineering Assumption — a value or behaviour invented to make the design concrete, justified by reasoning, and adjustable |
| **[HD]** | Proposed HD Enhancement — not required, included to strengthen the real-time/distributed-systems story |
| **[CONFIRM]** | Needs Lecturer Confirmation — genuinely ambiguous against the official material |

Where a decision is a judgement call rather than a hard requirement, it is additionally marked **Proposed — Awaiting Confirmation** so the team can override it without archaeology.

### Naming convention — read this first

The single biggest source of confusion in earlier drafts was conflating a **physical location** with the **controller** that manages it. This document is strict about the distinction from here on:

| Physical thing (infrastructure) | Controller that owns it (logical/software) |
|---|---|
| `I1`–`I6` — the six physical signalised intersections | `L1`–`L6` — the six intersection **local controllers** (`Lx` controls `Ix`) |
| `RC1`–`RC3` — the three physical railway level crossings | `RL1`–`RL3` — the three railway-crossing **local controllers** (`RLx` controls `RCx`) |
| `R1`–`R5` — the five physical roads | *(no controller — roads are not actuated)* |
| Central control room | `C1` — the **Central Controller** |

So: "`I1`" always means *the intersection as a place*; "`L1`" always means *the process/software that controls the lights at that place*. This matches the architecture diagram already present in `README.md`, which correctly used `L1–L6` — this specification restores that distinction.

---

## 1. Project Vision

We are building a **distributed, fail-safe traffic and railway-crossing control system**, not a centrally-controlled light board. The system's defining property is that **authority over physical outputs is local**: nine local controllers — six intersection local controllers `L1–L6` (each owning its physical intersection `I1–I6`) and three railway-crossing local controllers `RL1–RL3` (each owning its physical crossing `RC1–RC3`) — each own a distinct set of physical devices and keep operating safely and independently of everything else in the network. A tenth node, the **Central Controller `C1`**, has no actuation power at all — it exists purely to *observe*, *coordinate*, *reconfigure within safe bounds*, and *override within limits that can never violate local safety invariants*.

What makes this a strong EEET2588 project is not the number of intersections modelled, but that it gives a genuine, demonstrable answer to the core real-time systems questions the course is about: what happens when a message is late, when a link drops, when two safety-critical events compete for the same actuator, and when a component fails partway through a critical sequence. The railway crossing is deliberately the centrepiece of the safety argument (an actual life-safety interlock: gate-confirmed-closed before train-proceed), while the six intersections are the vehicle for demonstrating distributed coordination (green-wave-style offsets), local autonomy, and central-command abstraction.

The design deliberately keeps the *traffic simulation* side (vehicle arrival modelling, statistical demand) out of scope. This is a **control system** project: sensor *events* (simulated by keyboard input, per [REQ]) are what the system reacts to, not a traffic-flow model we would need to separately validate.

---

## 2. Source of Requirements

| Source | Role |
|---|---|
| `RTS_Final Project.pdf` | Primary authority. Defines: 6 intersections, 5 roads, double-track diagonal railway corridor, boom gates + flashing lights, train headway ranges, local-controls-own-lights principle, Central monitors/does-not-actuate, 3 sequence families (fixed / sensor / advanced), override capability, multi-QNX-node requirement, Pass-minimum scope (I1–I2 + crossing), and the encouragement to ground the design in a real intersection. |
| `lecture_clarification.md` | Authoritative supplement. Resolves: 9 local controllers total (6 intersection + 3 railway-crossing, not 6+3 as "same kind"), railway-crossing controllers exclusively own boom gate/flasher/train-signal, intersection controllers may only *sense* railway status, explicit train-signal requirement with fault-reporting, Central→Local command flow (never Central→light directly), keyboard-simulated sensors with an explicit key map, green-wave-style distance-based coordination, railway-congestion traffic suppression concept, and QNX-node-to-physical-computer flexibility (a single machine may host several logical nodes; the class-wide guidance was roughly 2–3 physical machines for a full demo). |
| `idea.md` | **Not a design.** A brainstorm/question dump. Used here only to identify which open questions are *high-impact* (answered explicitly below) versus low-value/premature (silently resolved by a reasonable default and not individually re-litigated). |
| `README.md` | Early draft. Its architecture diagram (`L1–L6` / `I1–I6`, `RL1–RL3` / `RC1–RC3`, `C1`) and repository layout are consistent with the decisions below and are retained; its scope claims are superseded by this document. |

---

## 3. Design Goals

1. **Safety is structural, not incidental.** Every hazardous condition (conflicting greens, train vs. open gate, pedestrian mid-crossing) is prevented by an invariant enforced *locally*, never by trusting a remote message to arrive correctly and on time.
2. **Local autonomy is real, not cosmetic.** Every local controller must be demonstrably capable of full safe operation with `C1` powered off.
3. **Central authority is monitoring + coordination + bounded override**, never direct actuation. This distinction must be visible in the IPC design, not just asserted in prose.
4. **Complexity is earned.** Every feature above the Pass-minimum must map to a specific real-time-systems teaching point (fault handling, coordination, timing analysis, degraded-mode operation) — features that only add surface area are explicitly rejected (Section 23).
5. **The design must scale by duplication, not by rewrite.** The Pass-minimum controller set (`L1`, `L2` at `I1`, `I2`, and `RL1` at `RC1`) must run the *same* generic controller binary as `L3–L6` / `RL2–RL3`, differing only by configuration — this is what "the complete design should duplicate this part" [REQ] actually demands.
6. **Everything claimed must be demonstrable.** No feature is included if it cannot be triggered live and observed in a 15-minute demo window [REQ].

---

## 4. Scope

### 4.1 Full Conceptual System

The complete design covers the entire network: 5 roads (`R1–R5`), 6 signalised intersections (`I1–I6`) each with a dedicated local controller (`L1–L6`), 3 railway crossings (`RC1–RC3`) each with a dedicated local controller (`RL1–RL3`), and 1 Central Controller (`C1`) — 10 logical controllers in total [REQ][CLARIF]. All feature families described in this document (fixed/sensor/advanced sequencing, pedestrian handling, full railway lifecycle including faults, congestion suppression, coordination/green-wave, override, degraded-mode operation) are part of the full conceptual design and are reflected in the Initial Design Report, state charts, and task architecture regardless of what is actually implemented in code.

### 4.2 Planned Proof of Concept

**Core PoC (guaranteed, satisfies the Pass-level minimum [REQ]):**
Intersections `I1`/`I2` (controllers `L1`, `L2`), the connecting road `R3`, railway crossing `RC1` (controller `RL1`), and `C1`. This is a fully functional "1/3 slice" — real QNX processes, real IPC, real railway interlock, real degraded-mode behaviour.

**HD-Target PoC extension — committed [TEAM, resolved 2026-08-22]:**
The team is targeting HD, not the Pass minimum, so this extension is now part of the planned build rather than a stretch goal. Duplicate the Core slice once more along one arterial to add `I3`/`I4` (controllers `L3`, `L4`), road `R4`, and railway crossing `RC2` (controller `RL2`) — chosen specifically because a *single* extra vertical slice cannot demonstrate arterial coordination (green-wave offsets need at least two chained intersections on the same road, e.g. `I1–I3` on `R1`). This extension is what upgrades the demonstration from "one working intersection pair" to "a distributed *network* of coordinating controllers," which is the actual HD differentiator.

**Full 9-controller build (`I5`/`I6` via `L5`/`L6`, `R5`, `RC3` via `RL3`):** Optional/stretch — see 4.3. Because the controller software is generic and config-driven (Goal 5, Section 3), the *design* already covers it; whether it is literally instantiated a third time at demo time is a resourcing decision, not a design gap.

### 4.3 Optional Scope

Protected left-turn arrows, a graphical UI beyond the terminal text display [REQ explicitly permits terminal-only], statistical/stochastic vehicle-arrival modelling, persistent database logging beyond the `/fs` test-vector use already permitted [REQ], and the third network slice (`L5/L6` at `I5/I6`, `RL3` at `RC3`) are optional/stretch — see Section 23 for full classification and rationale.

---

## 5. Physical Network

The official diagram [REQ] shows a diagonal double-track railway corridor cutting across a 3×2 grid of intersections. Interpreting that diagram against the Pass-minimum hint ("2 intersections I1–I2 and the railway crossing") and the lecturer's naming clarification [CLARIF] produces the following concrete topology — **[TEAM]**, derived directly from the given diagram, not a topology change, so it does not require lecturer approval:

```text
                R1 (arterial)                    R2 (arterial)
                    |                                 |
        I5 ---------+---------- R5 (connector) -------+-------- I6
                    |                                 |
                    |         (RC3 crosses R5)         |
                    |                                 |
        I3 ---------+---------- R4 (connector) -------+-------- I4
                    |                                 |
                    |         (RC2 crosses R4)         |
                    |                                 |
        I1 ---------+---------- R3 (connector) -------+-------- I2
                                (RC1 crosses R3)
```

- **`R1`** — north–south arterial through `I1 → I3 → I5` (west side).
- **`R2`** — north–south arterial through `I2 → I4 → I6` (east side).
- **`R3`** — east–west connector `I1 ↔ I2`, crosses the railway at **`RC1`**.
- **`R4`** — east–west connector `I3 ↔ I4`, crosses the railway at **`RC2`**.
- **`R5`** — east–west connector `I5 ↔ I6`, crosses the railway at **`RC3`**.

See `SYSTEM_DIAGRAMS.md`, Diagram 1, for the labelled visual version of this network.

This gives exactly 5 roads and 6 intersections [REQ], makes the Pass-minimum slice (`I1–I2–RC1`) a literal 1/3 vertical repeat of the full network (satisfying "the complete design should duplicate this part" [REQ]), and cleanly separates the **arterials** (through-traffic priority, candidates for green-wave coordination) from the **connectors** (lower base volume, but the ones directly exposed to railway pre-emption and congestion risk) — which directly answers `idea.md` F5's distinction between traffic priority and coordination sensitivity.

All roads carry two-way traffic with 2 lanes per direction [REQ]. Lane assignment **[TEAM]**: inner lane = through + left (permissive), outer lane = through + right (permissive); no dedicated turn arrows in the Core design (see Section 23 — Rejected). See `SYSTEM_DIAGRAMS.md`, Diagram 2, for the lane/direction layout. Turning-movement proportions and minor/side roads are explicitly **out of scope [ASSUM]**: the brief permits eliminating side roads as not important, and a project about *signal control logic* does not need a traffic-demand model to be credible — sensor *events* (not simulated vehicles) drive all reactive behaviour.

**Distance and speed assumptions [ASSUM, Proposed — Awaiting Confirmation]:** rather than mapping onto a literal named real-world intersection, the network is parameterised with representative urban values (arterial speed 60 km/h, connector speed 50 km/h, inter-intersection spacing 300–500 m, consistent with typical Australian arterial/collector spacing per Austroads guidance). This satisfies the brief's instruction to "at least attempt to base your design on a real intersection" for the purpose of *deriving timing methodology* (Section 20), without altering the given topology. If the team later wants to anchor to one specific real intersection for the report narrative, that is a documentation change only, not an architecture change.

---

## 6. System Components

**Physical infrastructure:** 5 roads, 6 signalised intersections, 3 railway level crossings (double track, one line per direction), 24 vehicle signal heads (4 approaches × 6 intersections), 24 pedestrian crossings with 24 push-buttons + 24 pedestrian signal heads, 6 boom gates (2 per crossing), 6 sets of road-facing flashing lights (2 per crossing), 6 train signals (2 per crossing, one per rail direction).

**Logical controllers:** `L1–L6` (intersection local controllers, one per physical intersection `I1–I6`), `RL1–RL3` (railway-crossing local controllers, one per physical crossing `RC1–RC3`), `C1` (Central Controller). 10 logical controllers total [CLARIF].

**Software/QNX architecture:** each logical controller is one or more QNX **processes**; controllers are distributed across multiple QNX **nodes**; nodes are distributed across a small number of physical/virtual machines at demo time (Section 18). The 1:1:1 mapping (controller = node = computer) is explicitly *not* assumed [CLARIF].

**Demonstration environment:** terminal-based state display per controller/console [REQ — GUI not required], keyboard-driven simulated sensor/operator input [REQ], optional `/fs`-based logging of test vectors [REQ].

---

## 7. Controller Architecture

```text
                                CENTRAL CONTROLLER (C1)
                          monitor · reconfigure · override
                         (never actuates a physical device)
                    ________________|________________
                   |                                  |
        INTERSECTION CONTROLLERS               RAILWAY CROSSING CONTROLLERS
             L1 L2 L3 L4 L5 L6                       RL1  RL2  RL3
              |  |  |  |  |  |                         |    |    |
             I1 I2 I3 I4 I5 I6                       RC1  RC2  RC3
      (physical intersections)                (physical level crossings)

     Devices owned by each Lx:                Devices owned by each RLx:
       - Vehicle signal heads                   - Boom gates
       - Pedestrian signal heads                 - Road-facing flashing lights
       - Vehicle presence sensors                - Train signals
       - Pedestrian buttons                       - Train approach/exit sensors
                   ^                                       |
                   |________ railway status (sense-only) ___|
```

Key architectural rule [CLARIF]: the arrow from `RLx` to `Lx` carries **status only** (crossing state), never a command, and it flows in one direction only. `Lx` cannot query or influence railway equipment; it can only *know* the crossing's current state and react on its own side of the interlock.

---

## 8. Controller Responsibilities

### 8.1 Central Controller (`C1`)

- **Inputs:** status reports from all 9 local controllers (light state, pedestrian queue, sensor state, mode, faults), heartbeats.
- **Outputs/commands:** `SET_MODE(PEAK_FIXED | OFF_PEAK_SENSOR)`, `SET_TIMING_PROFILE(params within validated bounds)`, `REQUEST_OVERRIDE(type, target, duration)`.
- **Monitoring responsibilities:** aggregate and display current light/pedestrian/railway state network-wide, maintain fault log, detect stale/missing controllers via heartbeat timeout.
- **Explicit limitations [REQ][CLARIF]:** cannot set an individual light to a colour; cannot actuate any railway equipment; cannot force a command through a safety invariant a local controller has rejected; cannot open a currently-latched pedestrian clearance early.

### 8.2 Intersection Local Controller (`Lx`, controlling intersection `Ix`)

- **Controlled devices:** 4 vehicle signal heads, 4 pedestrian signal heads (at `Ix`).
- **Local sensors:** 4 approach presence sensors, 4 pedestrian buttons, and (HD, only on the two crossing-facing intersections per railway crossing) 1 advance/queue sensor.
- **Traffic logic:** runs one of `PEAK_FIXED` / `OFF_PEAK_SENSOR`, enforces the 2-phase conflict matrix, clearance intervals, min/max green.
- **Pedestrian logic:** latches button presses, serves WALK inside the compatible vehicle phase, runs full WALK → FLASHING-DON'T-WALK → DON'T WALK sequence.
- **Railway information consumed:** crossing state (`OPEN/WARNING/CLOSING/CLOSED/TRAIN_PRESENT/OPENING/FAULT`) for the one crossing geometrically relevant to it (only the two controllers directly on the connector road that crosses that railway line receive it — e.g. only `L1`/`L2` receive `RC1`'s status).
- **Fallback behaviour:** continues full local sequencing (including sensor-driven logic) with no Central link at all; falls back to time-of-day self-selected mode.
- **Central interaction:** accepts `SET_MODE`/`SET_TIMING_PROFILE`/override, applies only at the next safe phase boundary, rejects and NACKs anything that would violate a safety invariant.

### 8.3 Railway-Crossing Local Controller (`RLx`, controlling crossing `RCx`)

- **Train detection:** 2 approach sensors (one per rail direction). See Section 9 regarding the exit sensor's status.
- **Boom-gate control:** exclusive owner of both gate arms for its crossing.
- **Flashing-light control:** exclusive owner of both road-facing flasher sets.
- **Train signal:** exclusive owner of both train signals (one per direction); enforces gate-confirmed-closed-before-proceed invariant.
- **Crossing state machine:** `OPEN → WARNING → CLOSING → CLOSED → (TRAIN_PRESENT) → OPENING → OPEN`, with a `FAULT` state reachable from any point.
- **Failure handling:** fail-to-safe on any ambiguous sensor/gate state (Section 17).
- **Information shared with road controllers:** current crossing state, broadcast to the two `Lx` controllers adjacent to its connector road only.
- **Central reporting:** every state transition and every fault, immediately.

All three `RLx` instances run *identical* logic — the brief gives no reason for them to differ, and inventing per-crossing behavioural differences would add untestable surface area for no teaching value **[TEAM — reject differentiation]**.

---

## 9. Sensors

| # | Sensor | Location | Owner | Event | Purpose / Simulation |
|---|---|---|---|---|---|
| 1 | Approach vehicle presence sensor | At stop line, 1 per approach (4 per intersection) | `Lx` | `DEMAND_PRESENT` / `DEMAND_ABSENT` | Standard inductive-loop equivalent: "is a vehicle currently waiting on this approach?" Keyboard key per approach. |
| 2 | Advance/queue sensor **[HD]** | ~60–80 m back from the stop line, only on the 2 approaches that feed directly into a railway crossing | `Lx` | `QUEUE_WARNING` | A *second, further-back* trip sensor with a different job to sensor #1. Sensor #1 answers "is anyone waiting at all?" Sensor #2 answers "has the queue grown all the way back to *here*?" — i.e. it is a distinct, more urgent signal meaning the queue is now long enough that it risks backing up onto the railway tracks. It feeds the congestion-suppression logic in Section 16, not the ordinary phase-extension logic. It is a simple presence trip, not a vehicle counter. Keyboard key. |
| 3 | Pedestrian push-button | 1 per crossing side (4 per intersection) | `Lx` | `PED_REQUEST(side)` | Directly required [REQ]. Keyboard key. |
| 4 | Train approach sensor | Fixed distance before the crossing, 1 per rail direction (2 per crossing) | `RLx` | `TRAIN_APPROACHING(dir)` | Directly required [REQ] — this is the event that starts the entire railway safety sequence (Section 14). Keyboard key. |
| 5 | Train exit/clearance sensor — **reserved, not used in Core logic [TEAM, revised]** | Just past the crossing, 1 per direction (2 per crossing) | `RLx` | *(not wired into the Core reopening decision)* | Originally planned to confirm exactly when a train has cleared the crossing. **Changed:** the Core design instead assumes every train takes the same fixed, known amount of time to clear the crossing once it arrives (Section 14), so reopening is computed from a timer, not from this sensor. The sensor is kept in the physical inventory as a placeholder for a **future enhancement** — e.g. cross-checking that a train actually cleared within the assumed window, to catch an abnormally slow or stalled train — but that check is explicitly out of scope for the Core safety logic today. |
| 6 | Boom-gate position sensor | On each gate arm (2 per crossing) | `RLx` | `GATE_CLOSED_CONFIRMED` / `GATE_OPEN_CONFIRMED` / `GATE_FAULT` | Real gates report confirmed position; the system never assumes a gate is closed just because a timer elapsed. Keyboard fault-injection key. |

Only these six sensor types are included. Vehicle counting, ANPR, weather sensors, and similar were considered and rejected — see Section 23.

---

## 10. Actuators and Signals

| # | Actuator | Owner | States | Safe State |
|---|---|---|---|---|
| 1 | Vehicle signal head (per approach) | `Lx` | `RED / YELLOW / GREEN / FLASHING_RED` | `FLASHING_RED` (all-way, real-world power-fail convention — forces every approach to yield) |
| 2 | Pedestrian signal (per crossing side) | `Lx` | `WALK / FLASHING_DONT_WALK / DONT_WALK` | `DONT_WALK` |
| 3 | Boom gate (per direction) | `RLx` | `UP / MOVING / DOWN / FAULT` | `DOWN` (ambiguity always resolved toward blocking the road, never toward allowing the train through unconfirmed) |
| 4 | Railway flashing lights (per road-approach direction) | `RLx` | `ON / OFF` | `ON` |
| 5 | Train signal (per rail direction) | `RLx` | `STOP / PROCEED` | `STOP` |

No dedicated turn-arrow actuators in the Core design (Section 23).

---

## 11. Operating Modes

| Mode | Purpose | Entry | Behaviour | Permitted Central Commands | Exit | Priority |
|---|---|---|---|---|---|---|
| `PEAK_FIXED` | Predictable, coordinable timing at high demand | Time-of-day or `SET_MODE` | Fixed 2-phase cycle, arterial-priority split; sensors monitored for congestion/fault only, not for extension | `SET_TIMING_PROFILE`, `SET_MODE`, override | `SET_MODE` or time-of-day change | Normal (7) |
| `OFF_PEAK_SENSOR` | Demand-responsive at low volume | Time-of-day or `SET_MODE` | Phase selection/extension driven by presence sensors + latched pedestrian requests, capped min/max green | `SET_TIMING_PROFILE`, `SET_MODE`, override | `SET_MODE` or time-of-day change | Normal (7) |
| `RAILWAY_PREEMPTION` | Protect the level crossing | `RLx` reports `WARNING`/`CLOSING`/`CLOSED` for the adjacent crossing | Overlays on top of `Lx`'s current mode: forces the toward-crossing movement through safe clearance to red, suspends normal phase selection for that approach only, prioritises drain movements after reopening | None accepted that would re-green the toward-crossing movement | Crossing reports `OPEN` and drain phase completes | 2 |
| `CENTRAL_OVERRIDE` | Exceptional situations (e.g. dignitary route) [REQ] | `REQUEST_OVERRIDE` accepted | Forces a specified through-movement to green for bounded duration | (is itself the command) | Duration expiry or explicit cancel | 4 |
| `DEGRADED_LOCAL` | Continue safely with no Central | Heartbeat timeout from `C1` | Continue last-known mode/profile autonomously; self-select Peak/Off-Peak by local clock | None (link is down) | Comms restored + local pushes full state to `C1` | Orthogonal (not in the hazard hierarchy — an availability state, not a hazard) |
| `FAULT_SAFE` | Contain a detected hazard | Local hardware conflict, watchdog trip, or (for `RLx`) unconfirmed gate state under train risk | Force safe-state outputs (Section 10), stop reacting to non-safety commands | None accepted except fault-clear (may require operator action) | Explicit clear, never automatic | 1 (highest) |

---

## 12. Normal Traffic Behaviour

Each intersection is modelled as a standard 2-phase 4-leg junction (arterial vs. connector), not per-lane phasing:

- **Phase A** — arterial (`R1`/`R2`) through+left green, connector red.
- Yellow (4 s **[ASSUM]**) → All-red clearance (2 s **[ASSUM]**).
- **Phase B** — connector (`R3`/`R4`/`R5`) through+left green, arterial red; compatible pedestrian WALK runs concurrently.
- Yellow → All-red clearance → repeat.

`PEAK_FIXED`: total cycle ≈ 90 s **[ASSUM]**, split favouring the arterial (e.g. ~55 s Phase A / ~35 s Phase B before yellows/clearances), matching the arterial's designated higher priority (Section 5). Sensors are monitored but do not alter timing in this mode, preserving the deterministic timing that coordination/green-wave depends on.

`OFF_PEAK_SENSOR`: a phase only receives green when its approach(es) report demand or hold a latched pedestrian request; green extends in 4 s increments **[ASSUM]** up to a 40 s cap **[ASSUM]** while demand persists; with zero demand anywhere, the intersection rests on the arterial phase (default main-road priority) [REQ — "differ between R1-R5, teams must state and justify"]. A minor road never waits longer than one full max-extended opposing phase plus its own minimum green — a structural anti-starvation bound rather than a weighting algorithm.

Mode switches (`PEAK_FIXED ↔ OFF_PEAK_SENSOR`, whether local time-of-day or Central-commanded) are applied only at the next phase boundary, never mid-phase **[TEAM]** — this preserves every clearance/min-green invariant regardless of when the switch request arrives.

---

## 13. Pedestrian Behaviour

- 4 crossings per intersection (N/E/S/W), 1 push-button per crossing side [REQ].
- A button press **latches** a single pending request per side; repeated presses while already latched have no additional effect (10 presses = 1 request) **[TEAM]**.
- Pedestrian WALK runs **concurrently** with the vehicle phase that does not conflict with that crossing (the Australian-standard parallel-walk pattern), not as a separate all-red "scramble" phase — a scramble phase was considered and rejected as unjustified added complexity (Section 23).
- Signal sequence: `WALK → FLASHING_DONT_WALK (clearance) → DONT_WALK` **[TEAM]** — matches real Australian pedestrian signals, not a bare two-state signal.
- Served in both `PEAK_FIXED` (guaranteed once per cycle, since both phases run every cycle) and `OFF_PEAK_SENSOR` (a latched request is treated exactly like vehicle demand for scheduling purposes).
- Once WALK has started, it must run to completion through its clearance interval before that crossing can be interrupted by anything, including a Central override (Safety Invariant, Section 22).
- A pedestrian request arriving during an active railway pre-emption is **latched, not discarded**. Because pre-emption only forces the *toward-crossing vehicle movement* red (Section 15) and pedestrian crossings exist only at intersections, not at the railway crossing itself **[TEAM — scope note: pedestrian crossings are out of scope at `RCx`, since the brief only places them "at each intersection"]**, the request is normally still served in the ordinary non-conflicting phase.
- A button that never releases (stuck) or is unresponsive is treated fail-safe as a **standing pending request** (assume real demand exists) rather than ignored, and a fault is reported to `C1` **[ASSUM]**.

---

## 14. Railway Behaviour

Full normal lifecycle, owned entirely by `RLx` [CLARIF]:

1. **Train detected** — approach sensor trips → `RLx` immediately starts flashing lights and immediately notifies the two adjacent `Lx` controllers and `C1` (state → `WARNING`).
2. **Traffic clearing begins** — the adjacent `Lx` starts its safe transition to red for the toward-crossing movement (finishes any clearance already in progress; does not cut a green instantly — Section 22).
3. **Gate closing** — after the initial flash-only warning interval, `RLx` commands both gate arms down (state → `CLOSING`).
4. **Gate confirmed closed** — position sensors confirm both arms down (state → `CLOSED`).
5. **Train signal set** — `PROCEED` issued **only if** gate confirmed `CLOSED`; otherwise remains `STOP` and a fault is raised (Safety Invariant).
6. **Occupancy window starts** — the instant the approach sensor trips, `RLx` also computes a fixed **crossing-occupancy window** for that train: `expected_arrival_time + fixed_crossing_clear_duration`. This is the timer-based replacement for exit-sensor detection (Section 9, item 5) — the assumption **[ASSUM]** is that every train takes the same, known amount of time to fully clear the crossing once it arrives, so a timer is sufficient and the exit sensor is not required for this decision.
7. **Train passes** — state is `TRAIN_PRESENT` for as long as *any* direction's occupancy window (step 6) has not yet elapsed.
8. **Gate reopens** — only once **every** active occupancy window (both directions, including a second/closely-following train) has elapsed does `RLx` raise the gates (state → `OPENING → OPEN`), stop the flashers, and broadcast `OPEN`. If a second train is detected approaching before the first train's window has elapsed, the gate simply stays down — its own occupancy window is added to the set of windows that must all clear before reopening (this naturally handles two trains arriving close together, from the same or opposite directions, without needing any special-case logic).
9. **Traffic resumes** — the adjacent `Lx` resumes its previous phase family (not a full reset) and runs an extended drain phase on the connector road to clear the queue that built up during closure (Section 16) before returning to normal cycling.

**Timing methodology [ASSUM, Proposed — Awaiting Confirmation]:** the brief supplies no lead-time numbers, so values are *derived*, not invented:
- **Warning-to-arrival lead time** ≈ flashing-only warning (5 s) + gate closing duration (10 s) + closed-confirmation safety margin (5 s) + additional margin for the adjacent intersection's own safe clearance sequence (~25 s) ⇒ target **~45 s from train-detected to train-at-crossing**. Approach-sensor placement distance follows from `assumed max train speed × 45 s` (e.g. 80 km/h ≈ 22 m/s ⇒ ≈1000 m).
- **Crossing-occupancy duration** (step 6 above) ≈ an assumed fixed value, e.g. **20 s**, representing train length + crossing width at the assumed train speed, identical for every simulated train.

Both values are explicitly presented in the report as derived, tunable parameters, not fabricated facts.

**Accepted trade-off:** using a fixed timer instead of a confirmed-exit sensor means the design does not, today, detect the specific edge case of a train that becomes abnormally slow or stalled on the crossing. This is a deliberate, documented simplification for the Core design — not an oversight — and the reserved exit sensor (Section 9, item 5) is the identified path for closing that gap later if time permits.

---

## 15. Railway Pre-emption and Traffic Clearing

**Plain description:** "pre-emption" here works like an ambulance taking priority at a signal — except the thing being protected is the level crossing, not a vehicle. The moment `RLx` detects an approaching train, it does not wait for the adjacent intersection's normal cycle to get around to the connector-road phase; it immediately tells the adjacent `Lx` controllers "a train is coming," and each `Lx` reprioritises so that the specific vehicle movement pointed at the crossing loses its green (through the normal safe clearance sequence, not abruptly) well before the gate needs to come down.

Pre-emption is **directional, not intersection-wide [TEAM]**: only the vehicle movement(s) that lead onto the closing/closed crossing are forced red; cross-traffic and pedestrian phases not conflicting with that movement continue to be served normally by the same `Lx`. For example, at `I1` (adjacent to `RC1` via `R3`), only the `R3`-toward-the-crossing movement is affected — the `R1` arterial phase and any pedestrian crossing not conflicting with the `R3` movement keep operating on their ordinary schedule. This is both more realistic and strictly safer to implement than a full-intersection freeze, since it avoids inventing new conflict cases beyond the intersection's normal conflict matrix.

The toward-crossing movement's transition to red always completes through the ordinary yellow + all-red clearance (Safety Invariant) — pre-emption changes *scheduling priority*, never *skips* the clearance sequence, because doing so would itself create a hazard at the intersection.

---

## 16. Railway Congestion Management

Per the lecturer's clarification, the goal is explicitly to stop *feeding* traffic toward a closed crossing, not to invent traffic rerouting the light system cannot actually perform. Concretely, the traffic-light system can do exactly three things, and no more — the specification states this plainly rather than implying the system "solves" congestion:

1. **Suppress demand toward the crossing.** Once pre-emption begins, the toward-crossing movement stops receiving green until the crossing reopens (Section 15) — this is the primary lever.
2. **Prioritise away-from-crossing and cross-traffic movements** at the adjacent intersection during the closure window, so vehicles already queued between the intersection and the crossing have a chance to turn away instead of accumulating — only where the existing lane/turn geometry allows it; the spec does not claim the system can conjure a new physical route.
3. **Run a bounded drain phase after reopening** — a green extension beyond the normal max-green cap on the connector road, specifically to clear the backlog that accumulated during closure, before returning to standard cycling.

The advance/queue sensor (Section 9, item 2) exists specifically to detect when a queue on the crossing-facing approach is growing toward the crossing, feeding both the pre-emption suppression logic and the recovery drain-phase sizing.

---

## 17. Failure and Safety Behaviour

| Failure | Response |
|---|---|
| Gate fails to confirm `CLOSED` before the deadline while a train is approaching | `FAULT_SAFE` (crossing): train signal forced/held `STOP`, immediate `C1` fault, toward-crossing vehicle movement held red until manually cleared |
| Train sensor failure (no signal / stuck) | Fail-safe to "train may be present" — gate stays down / does not reopen until operator-cleared. Mirrors the real-world principle that level-crossing equipment fails toward stopping road traffic, never toward allowing a train through unconfirmed |
| Gate stuck open with a train approaching | Train signal forced `STOP`; `C1` raises a **critical** alarm; adjacent intersections receive the fault for situational logging only — the spec is explicit that **no software action here can physically stop a train**; this is a genuine limitation, not something the system claims to fix |
| Gate stuck closed, no train | Reported to `C1` as a nuisance fault; train signal stays at its normal safe default; road traffic simply waits (safe, only inefficient); requires manual operator clearance after physical repair — `C1` cannot force the physical gate (Safety Invariant: no remote override of railway equipment) |
| Single vehicle/pedestrian sensor failure | Declared after an unrealistically long silence window relative to the expected cycle; that one approach fails safe to "assume demand present" so it is never starved; does not affect the rest of the intersection |
| Local controller software failure | Fails to the actuator safe state (Section 10) — realised in the PoC via an **[HD]** lightweight QNX watchdog/supervisor process per node (Section 23) that detects the controller process's death and forces safe outputs |
| Communication loss, `Lx ↔ C1` | `DEGRADED_LOCAL` (Section 11) |
| Communication loss, `Lx ↔ Lx'` (peer coordination) | Any coordination feature (green-wave offset) degrades to standalone timing; core safety behaviour of either controller is entirely unaffected — no controller ever blocks on another for safety |

---

## 18. Central Controller Behaviour

`C1` monitors all 10 controllers' state, timing profile, faults, and railway status, and issues three command classes: `SET_MODE`, `SET_TIMING_PROFILE`, `REQUEST_OVERRIDE` [REQ/CLARIF — "command abstraction," never a raw light colour]. Every command receives an explicit ACK/NACK. A NACK means the local controller judged the command unsafe (e.g. an override request during active railway pre-emption at that intersection) and it is surfaced to the operator as a rejected/faulted command, never silently dropped.

`C1` decides the network-wide time-of-day mode as the default source of truth (for demo consistency), but every local controller retains and uses its own fallback clock the moment `C1` is unreachable (Section 11) — this is what makes "Central decides the mode" and "locals must stay safe without Central" simultaneously true.

`REQUEST_OVERRIDE(CLEAR_ROUTE, target, duration)` **[TEAM]**: sent to the local controller `Lx` of the target intersection; forces a specified through-movement to green for a bounded duration (default max 5 minutes **[ASSUM]**, auto-expiring back to the prior mode if not renewed — preventing a forgotten override from permanently breaking normal operation). It is rejected outright if the target intersection currently has an active/imminent railway pre-emption on that movement, and it never truncates an in-progress pedestrian clearance (Section 22).

---

## 19. Local Autonomy

Every safety-relevant decision an `Lx` or `RLx` makes — conflict-free phasing, clearance timing, railway pre-emption reaction, gate-confirmed-before-proceed — is computed **entirely locally**, with zero runtime dependency on `C1` being reachable. Loss of the Central link:

- Does **not** stop sensor-driven operation (`OFF_PEAK_SENSOR` continues using purely local sensor input).
- Does **not** stop railway protection (`RLx` logic has no Central dependency at any point in its state machine).
- Causes the controller to enter `DEGRADED_LOCAL`: continue on the last known valid mode/timing profile rather than reverting to an arbitrary hard-coded default, and self-select Peak/Off-Peak by local clock.
- On reconnection, the **local controller pushes its full current actual state to `C1`** (not the reverse) — this avoids any window where `C1` could impose a stale command over state that has already moved on.

Loss of a peer-to-peer link between two local controllers (used only for optional coordination, Section 20) never affects either controller's core safety behaviour — coordination gracefully degrades to standalone; it is never a dependency for staying safe.

---

## 20. Timing and Coordination Strategy

**Methodology, not fabricated numbers [ASSUM]:** cycle length, green splits, and offsets are derived from the distance/speed assumptions in Section 5, following the standard approach the brief itself points to (measure inter-intersection distance, assume a travel speed, derive the time a platoon takes to reach the next signal, offset that signal's green start by that travel time — a green wave). Exact final values are **tunable parameters**, explicitly not asserted as validated real-world numbers.

**Coordination scope [TEAM]:** offsets are computed for the arterial chains only (`I1–I3–I5` on `R1`, `I2–I4–I6` on `R2`) — coordinating the connectors (`R3/R4/R5`) across the railway corridor was considered and rejected, since those roads are short, low-continuity connectors whose dominant timing driver is railway pre-emption, not platoon progression (Section 23).

**Who computes it [TEAM]:** `C1` computes and distributes offset profiles (it has the network-wide view needed for the calculation), but each `Lx` applies the offset to its own local sequencer — coordination is therefore a *timing parameter* delivered via the same `SET_TIMING_PROFILE` command class as everything else, not a new command type or a new authority for `C1`.

**Coordination failure [TEAM]:** any `Lx` that loses its offset input (Central unreachable, or a peer report goes stale) reverts to standalone fixed/sensor timing — never blocks waiting for a peer.

**Railway disruption to green-wave [TEAM]:** after a pre-emption event, the affected `Lx`'s phase clock resumes and its next-received `SET_TIMING_PROFILE` (or reconnection push) re-establishes the offset — the design does **not** attempt a special resynchronisation transient, since bolting that on adds a state machine with no independent safety or demonstration value beyond "coordination eventually recovers," which the normal path already guarantees (Section 23 — Rejected: gradual resync algorithm).

---

## 21. Real-Time Requirements

| Event | Trigger | Nature | Deadline (target) | Miss behaviour |
|---|---|---|---|---|
| Railway safety chain start | Train approach sensor trip | Hard, aperiodic | React locally within 200 ms; adjacent-intersection notification within 500 ms end-to-end (large margin against the ~45 s physical lead-time budget, Section 14) | N/A — this is the one deadline the whole design is built around never missing; local reaction is a simple sensor-to-actuator path with no remote dependency |
| Gate-closed confirmation before train-proceed | Gate position sensors | Hard | Must be confirmed before the derived arrival deadline (Section 14) | Train signal defaults to `STOP`, fault raised (fail-to-safe, never fail-to-proceed) |
| Phase sequencer tick | Internal timer | Soft, periodic | 100 ms tick, ±50 ms jitter tolerance | Purely cosmetic timing smoothness; no safety impact |
| Pedestrian button → latch registered | Button press | Soft | < 500 ms (human-perceptible threshold) | Delayed acknowledgement only, request is never lost |
| Light-state change → `C1` status report | Any light/pedestrian/mode state change | Firm — explicitly mandated by the brief [REQ] | < 200 ms | Logged as a comms-degradation indicator; does not affect local safety |
| Heartbeat | Periodic liveness | Firm, periodic | 1 s period; 3 missed (~3 s) ⇒ declare link down | Triggers `DEGRADED_LOCAL` |
| Central command → ACK/NACK | `SET_MODE`/`SET_TIMING_PROFILE`/override | Soft/firm | < 1 s | 2 retries, then `C1` marks the controller unreachable/raises a comms alarm |
| Fault detection → annunciation | Any sensor/gate/hardware fault | Hard (it is itself a safety report) | Faster than the normal status path — sent immediately, not batched | N/A — fault reporting is never rate-limited |
| Override applied | `REQUEST_OVERRIDE` accepted | Soft, deliberately bounded by safety, not instant | Applied at next safe phase boundary (no fixed deadline by design) | N/A — this is intentional: an override is *never* allowed to apply instantly if that would skip a clearance interval |

**Demo-time scaling [TEAM]:** a single configurable global speed-up factor is applied uniformly to every timer in the system (phase durations, pedestrian intervals, railway lead times, heartbeat/timeout thresholds) so that relative real-time margins are preserved at demo speed — scaling only some timers (e.g. traffic but not railway) was considered and rejected because it would falsify the very safety margins the demonstration is meant to prove.

---

## 22. Safety Invariants

These are the rules the state charts, task architecture, and test scenarios must all be traceable back to:

1. No two conflicting vehicle movements at the same intersection shall receive `GREEN` concurrently (defined by the intersection's static conflict matrix).
2. No pedestrian `WALK` shall be active concurrently with a `GREEN`/`YELLOW` vehicle movement that conflicts with that crossing.
3. Every transition between conflicting movements passes through `YELLOW` then `ALL_RED` of at least the configured minimum duration before the next conflicting movement gets `GREEN`.
4. Once a pedestrian `WALK` phase has started, it must complete its full clearance before that crossing serves a conflicting vehicle `GREEN` again.
5. A train signal shall never show `PROCEED` unless its crossing's boom gate is sensor-confirmed `CLOSED`.
6. If gate-closed confirmation cannot be obtained by the required deadline, the train signal defaults to `STOP` and a fault is raised — ambiguity always resolves to the safer state, never to "assume it's fine."
7. Only the owning `RLx` may actuate its boom gates, flashers, or train signals — no other controller, including `C1`, may issue an actuation command to railway equipment.
8. A gate must not reopen while any train's computed crossing-occupancy window (Section 14, step 6) has not yet elapsed, for either direction — including a second train detected while the first is still occupying the crossing.
9. An intersection movement leading directly onto a crossing must not show `GREEN` while that crossing is `CLOSING`, `CLOSED`, `TRAIN_PRESENT`, or `FAULT`.
10. Every local controller must be able to run its full safety-relevant logic (phasing, clearance, railway reaction) with `C1` unreachable.
11. A `C1` command that would violate any of these invariants must be rejected and reported by the receiving controller, never silently applied.
12. An override must never truncate an in-progress pedestrian clearance and must never command a movement toward a crossing that is not `OPEN`.
13. Loss of a single sensor or a single communication link may degrade functionality (e.g. to fixed timing, or to standalone coordination) but must never by itself produce an unsafe output state.

**Priority hierarchy [TEAM]**, refined from `idea.md`'s draft:

```text
1. FAULT_SAFE            — local hardware/software or railway-equipment fault (always wins locally; cannot be remotely overridden)
2. RAILWAY_PREEMPTION    — active/imminent train movement protection
3. Committed pedestrian clearance in progress
4. CENTRAL_OVERRIDE      — exceptional operator command
5. Congestion-prevention response (drain phase, suppression)
6. Normal pedestrian request service
7. Normal sensor/demand-based phase selection
8. Fixed-timing baseline (lowest — the default backdrop)
```

Note: `FAULT_SAFE` is not really "competing" with the others in the same sense — it represents a controller that can no longer trust its own outputs at all, so it is listed first as an absolute local veto, not as a schedulable priority level.

---

## 23. Feature Classification

### Core (required for the assignment to be satisfied)

Six intersections + railway crossings design [REQ]; local controller per intersection operating continuously [REQ]; central controller with monitoring/status/commands [REQ]; local-controls-own-lights, survives central/comm loss [REQ]; fixed / sensor-driven / advanced sequence families [REQ]; boom gates + flashing lights + train signal with fault-to-red [REQ][CLARIF]; keyboard-simulated sensors [REQ]; multiple QNX nodes with separate processes [REQ]; terminal state display [REQ]; Pass-minimum PoC controller set (`L1`, `L2`, `RL1` at `I1`, `I2`, `RC1`) [REQ].

### HD-Target (committed build, per Section 4.2)

The `L3`/`L4`/`RL2` extension itself (Section 4.2); advance/queue sensor + explicit congestion-suppression + drain-phase logic (Section 16) — directly answers the brief's explicit "state and justify... congestion control" requirement; arterial green-wave coordination (Section 20) — genuine distributed-systems + timing-analysis content; local-authority-to-reject-unsafe-command with NACK reporting (Section 18) — a concrete, demonstrable safety argument; per-node watchdog/supervisor process (Section 17) — a small extra QNX process per node whose only job is to check that its supervised controller process is still alive and, if not, force that node's outputs to their safe state directly — a direct, low-cost RTOS supervision demonstration; bounded, auto-expiring Central override with explicit subordination rules (Section 22) — shows a real distributed-authority argument rather than "Central can do anything."

### Optional / Stretch

Extending the PoC to the full 9-controller network (`L5/L6` at `I5/I6`, `RL3` at `RC3`) once the committed HD-target 5-controller build is proven; protected left-turn arrows; a graphical (non-terminal) display; the future exit-sensor cross-check described in Section 14's accepted trade-off.

### Rejected / Deferred

| Idea | Why rejected |
|---|---|
| Statistical/stochastic vehicle-arrival simulation | This is a *control system* project; sensor events (keyboard-simulated per [REQ]) are the correct input model — building a traffic simulator adds a whole separate subsystem to validate for no assessment credit |
| Pedestrian "scramble" (all-red) phase | Real Australian intersections default to parallel walk; scramble adds a distinct conflict-matrix mode with no requirement basis |
| Per-lane signal heads / per-lane sensors | Brief only requires 2 lanes/direction with turn lanes "may have" — per-lane granularity multiplies state without a corresponding requirement |
| Dedicated turn-arrow signals | Explicitly called out in the brief as an optional variation, not a requirement; adds phases/conflict cases for no core teaching value |
| 4-tier time-of-day demand model (morning/day/evening/night) | 2 tiers (Peak/Off-Peak) already exercises every mode-transition and coordination mechanism; a 4th tier is more numbers to justify, not more design |
| Directional demand bias / burst arrival modelling | No sensor or actuator behaviour depends on modelled arrival statistics — this would be simulation realism with zero effect on the control logic being assessed |
| Gradual green-wave resynchronisation algorithm after a train event | The normal reconnection/reprofile path already recovers coordination; a bespoke resync transient is a second state machine solving a problem the first one already solves |
| Local-to-local coordination for connector roads across the railway | Connectors are short and railway-pre-emption-dominated; coordinating their timing has no meaningful travel-time benefit to model |
| `C1` able to override railway equipment in an emergency | Explicitly rejected on safety grounds — no remote authority may ever bypass the gate-confirmed-closed interlock; matches the lecturer's own stated recommendation |
| Real-time exit-sensor-based crossing clearance (as opposed to fixed timer) | Superseded by the fixed crossing-occupancy-duration timer (Section 14) per team decision; kept as a documented future enhancement, not built now |

---

## 24. Demonstration Scenarios

| # | Scenario | Verifies |
|---|---|---|
| 1 | Normal `PEAK_FIXED` cycling at `I1`/`I2` (`L1`/`L2`) | Core 2-phase sequencing, clearance intervals |
| 2 | `OFF_PEAK_SENSOR` responding to a simulated vehicle event | Sensor-driven extension, main-road default rest state |
| 3 | Pedestrian button press → WALK → clearance → resume | Full pedestrian lifecycle, latching |
| 4 | Central `SET_MODE`/`SET_TIMING_PROFILE` applied only at a safe boundary | Command abstraction, safe-boundary application |
| 5 | Full train-approach sequence at `RC1` (`RL1`) | Warning → gate closing → confirmed closed → train proceed |
| 6 | Train currently occupying the crossing, second train detected before the first clears | Adjacent intersection holding toward-crossing red, gate-not-reopen invariant (occupancy-window stacking) |
| 7 | Crossing reopening + drain phase | Recovery, queue clearance, resumed phase family |
| 8 | Boom-gate fault injected while a train approaches | Train signal forced `STOP`, fault reported, no unsafe proceed |
| 9 | Kill `C1` mid-demo | `DEGRADED_LOCAL`, continued safe autonomous operation at all locals |
| 10 | Restart `C1` | Local-initiated resync (state pushed up, not commands pushed down) |
| 11 | `REQUEST_OVERRIDE(CLEAR_ROUTE)` during an in-progress pedestrian WALK | Override correctly queued/subordinated, no clearance truncation |
| 12 | `REQUEST_OVERRIDE` attempted during active railway pre-emption at the same intersection | Local rejection (NACK), railway safety wins |
| 13 | Advance-sensor queue warning near a closing crossing | Congestion-suppression logic (toward-crossing green withheld) |
| 14 | Green-wave offset across `I1–I3` (`L1`–`L3`, HD extension) | Arterial coordination, offset computed from distance/speed |
| 15 | Kill an `Lx` controller process | Watchdog/supervisor forces safe-state outputs at that node |

---

## 25. Simulation Inputs

A single mnemonic keyboard scheme, structured as `<type><location><detail>`. The numeric/letter suffix identifies the **physical location** (intersection or crossing); the keystroke is routed internally to that location's controller (`Lx`/`RLx`):

| Prefix | Meaning | Example |
|---|---|---|
| `V<intersection><approach>` | Vehicle presence sensor trip | `V1N` = vehicle detected, `I1` North approach (handled by `L1`) |
| `Q<intersection><approach>` | Advance/queue sensor trip | `Q2W` = queue warning, `I2` West approach (handled by `L2`) |
| `P<intersection><side>` | Pedestrian button press | `P1E` = pedestrian request, `I1` East crossing (handled by `L1`) |
| `T<crossing><dir>` | Train approaching | `T1A` = train approaching `RC1` from direction A; `T1B` = direction B (handled by `RL1`) |
| `FG<crossing>` | Inject gate/sensor fault | `FG1` = fault at `RC1`'s gate (handled by `RL1`) |
| `FL<intersection>` | Inject local light-hardware fault | `FL2` = fault at `I2` (handled by `L2`) |
| `M` | Toggle `PEAK_FIXED`/`OFF_PEAK_SENSOR` (operator console) | — |
| `O<intersection><movement>` | Request `CLEAR_ROUTE` override | `O1NS` = clear route north–south at `I1` (handled by `L1`) |
| `D` | Dump current network status to the display/console | — |

Note: the `X<crossing><dir>` (train exit) key from earlier drafts is removed from the Core input set, since reopening is now timer-driven (Section 14) rather than exit-sensor-driven; it can be reintroduced if the reserved exit sensor is ever wired into a future enhancement.

The console explicitly echoes the resulting state transition after each key press, since the demo assessment expects visible, provable behaviour, not just an accepted keystroke.

---

## 26. Assumptions

| # | Assumption | Reason | Design Consequence | Lecturer Confirmation Needed? |
|---|---|---|---|---|
| 1 | Arterial speed 60 km/h, connector 50 km/h, spacing 300–500 m | Representative Australian urban arterial/collector values, used only to *derive* timing methodology | Feeds cycle length, green split, coordination offset calculations | No |
| 2 | Min green 8 s, max green 40 s, yellow 4 s, all-red 2 s | Typical Australian signal-engineering ranges (Austroads-consistent) | Bounds every phase timer in the design | No |
| 3 | `PEAK_FIXED` cycle ≈ 90 s, arterial-favoured split | Representative mid-size intersection cycle | Basis for fixed-mode timing and green-wave offsets | No |
| 4 | Railway warning-to-arrival lead time ≈ 45 s, derived from flash-lead + gate-close + margins | No lead time is given in the brief; a methodology-derived value is safer than an invented one | Sets approach-sensor placement distance and pre-emption reaction budget | No — flagged as tunable |
| 5 | Fixed crossing-occupancy duration ≈ 20 s, identical for every train | Simplifies reopening logic to a timer instead of requiring exit-sensor confirmation, per team decision | Gate reopening (Section 14) is timer-based; exit sensor is reserved, not core | No — flagged as tunable; documented accepted trade-off (no detection of an abnormally slow/stalled train in Core scope) |
| 6 | Only 2 demand tiers, Peak/Off-Peak | Sufficient to exercise every mode transition without unjustified extra numbers | Simplifies `SET_MODE` and time-of-day self-selection | No |
| 7 | No stochastic vehicle-arrival modelling; sensors are event-driven only | Project is a control system, not a traffic simulator | Sensor input model is entirely keyboard-event-based | No |
| 8 | Heartbeat 1 s, link-down after 3 missed | Fast enough for a compressed demo, standard missed-N pattern | Basis for `DEGRADED_LOCAL` entry timing | No |
| 9 | Override max duration 5 minutes, auto-expiring | Prevents a forgotten override from permanently altering operation | Bounds `CENTRAL_OVERRIDE` mode | No |
| 10 | Pedestrian crossings exist only at intersections, not at railway crossings | Brief places pedestrian crossings "at each intersection" only | Simplifies railway pre-emption/pedestrian interaction analysis | No |
| 11 | Demo topology parameterised as a "generic realistic" network rather than mapped onto one named real intersection | Satisfies the brief's intent (ground timing in real principles) without altering the given diagram's topology | Avoids the topology-change lecturer-approval trigger entirely | No, by design |
| 12 | Demo deployment uses 2 physical machines (Section 27) | Fewer physical/network failure points during a live, assessed demo; QNX nodes are logical and location-transparent, so 2 machines can still host every required separate process/node | Deployment diagram targets 2 machines; can be scaled to 3 later without any code change | No |

---

## 27. Open Design Decisions — resolved 2026-08-22

All three items below were open in the previous draft and have now been decided by the team:

1. **HD-extension PoC commitment — RESOLVED: committed.** The team is targeting HD. The `L3`/`L4`/`RL2` extension (Section 4.2) is part of the planned build, not a stretch goal. Green-wave coordination (Section 20) is therefore a *demonstrated* feature.
2. **Final physical machine count — RESOLVED: 2 machines, recommended.** Reasoning: a QNX "node" is a logical/software concept, not a physical one — Qnet message passing works identically whether two processes are on the same machine or different machines. Two physical machines can therefore still host every required separate controller process on its own QNX node, fully satisfying "multiple QNX nodes with separate processes" [REQ]. A third machine adds real demo-day risk (extra hardware, extra network configuration, one more thing that can fail to boot or connect on the day) without adding any capability the requirement actually needs. If the team later wants a more visually convincing "these are obviously separate boxes" demo, moving to 3 machines is a deployment-config change only, not a design change.
3. **Per-node watchdog/supervisor process — RESOLVED: included, since the team is targeting HD.** Plain description: it is one extra, very small QNX process per node whose only job is "check that the real controller process on this node is still responding; if it stops responding, force that node's outputs to their documented safe state (Section 10) immediately." It is cheap to build, has no interaction with the rest of the traffic logic, and directly demonstrates a genuine real-time-systems supervision pattern (Section 17, Section 23). Final scheduling (which sprint it gets built in) is left to the team.

---

## 28. Questions for Team / Lecturer

**Short answer: nothing in this specification currently requires the lecturer's sign-off.** The only trigger the lecturer described for needing approval was *changing the given map/topology* (e.g. altering lane counts or the intersection layout). This design does not do that — Section 5 only names and orients the roads that are already implied by the official diagram and the Pass-minimum hint. Every numeric timing value in Section 26 is presented as a derived, tunable engineering assumption, not a requirement claim, so none of them need approval either.

The only remaining "confirm with someone" item is purely a team preference, not a lecturer question: if the team later wants the report to reference one specific named real-world intersection (rather than the generic realistic parameterisation in Assumption 11), that's a one-line internal decision, not something that needs an email to the lecturer.

---

## 29. Traceability

| Feature | Origin |
|---|---|
| 6 intersections, 5 roads, railway corridor, boom gates/flashers, local-controls-own-lights, Central monitors-only, 3 sequence families, override, multi-node QNX, Pass-minimum slice | [REQ] |
| 9 local controllers total, `RLx` exclusive railway-equipment ownership, train signal + fault-report requirement, keyboard key-map expectation, 2–3 machine deployment guidance | [CLARIF] |
| `Lx`/`Ix` and `RLx`/`RCx` naming split, road/road-name topology mapping (Section 5), 2-phase intersection model, pedestrian parallel-walk pattern, priority hierarchy, command abstraction detail, degraded-mode resync direction, timer-based crossing-occupancy decision, rejected-feature list | [TEAM] |
| All numeric timing values, sensor placement distances, heartbeat/timeout numbers, fixed crossing-occupancy duration | [ASSUM] |
| Advance/queue sensor + congestion suppression, green-wave coordination, watchdog/supervisor process, bounded auto-expiring override, local-reject-unsafe-command | [HD] |

---

## 30. Final Proposed System Summary

The system is a distributed, safety-first traffic and railway control network: 6 intersection local controllers (`L1–L6`, one per physical intersection `I1–I6`) and 3 railway-crossing local controllers (`RL1–RL3`, one per physical crossing `RC1–RC3`) each hold exclusive, offline-capable authority over their own physical devices, coordinated — never commanded at the light level — by a Central Controller (`C1`) that can only monitor, reconfigure within safe bounds, and issue bounded overrides that local controllers may refuse. The railway crossing is the safety centrepiece (gate-confirmed-closed before train-proceed, a fixed occupancy-window timer governing safe reopening, fail-to-safe on every ambiguity), while the six-intersection network is the coordination/distribution centrepiece (2-phase local sequencing, sensor-driven off-peak behaviour, arterial green-wave offsets, and structural anti-starvation). The guaranteed proof-of-concept is the Pass-minimum controller set `L1`/`L2`/`RL1` (at `I1`/`I2`/`RC1`) plus `C1`, built as the same generic, config-driven controller software that — by design, not by promise — duplicates cleanly to the full 6-intersection, 3-crossing network. The team is committing to the HD-target extension (`L3`/`L4`/`RL2` at `I3`/`I4`/`RC2`), which is specifically what makes multi-intersection coordination and congestion suppression demonstrable rather than theoretical, deployed across 2 physical machines to keep demo-day risk low while still satisfying the multi-QNX-node requirement.
