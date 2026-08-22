# Traffic Light Control System — Project Specification

**Course:** EEET2588 Real-Time Systems — Design Project
**Status:** Living document — single source of truth for the project. Supersedes `README.md` and `idea.md` where they conflict with this file. This file is generated from, and must be kept consistent with, the team's Vietnamese working document (`PROJECT_SPECIFICATION.vi.md` and `spec-vi/01`–`06`) — if the two ever disagree, treat the Vietnamese working document as the source of truth and regenerate this file from it.
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

The single biggest source of confusion in early drafts was conflating a **physical location** with the **controller** that manages it. This document is strict about the distinction from here on:

| Physical thing (infrastructure) | Controller that owns it (logical/software) |
|---|---|
| `I1`–`I6` — the six physical signalised intersections | `L1`–`L6` — the six intersection **local controllers** (`Lx` controls `Ix`) |
| `RC1`–`RC3` — the three physical railway level crossings | `RL1`–`RL3` — the three railway-crossing **local controllers** (`RLx` controls `RCx`) |
| `R1`–`R5` — the five physical roads | *(no controller — roads are not actuated)* |
| Central control room | `C1` — the **Central Controller** |

So: "`I1`" always means *the intersection as a place*; "`L1`" always means *the process/software that controls the lights at that place*. This matches the architecture diagram already present in `README.md`, which correctly used `L1–L6` — this specification restores that distinction.

### Real-world reference and driving convention

The project's real-world grounding is **Vietnam, right-hand traffic** — the team references signalised intersections with dedicated turn arrows in District 2, Ho Chi Minh City, rather than the Melbourne example shown in the official brief (the brief only *encourages* basing the design on a real intersection; it does not mandate which one). With right-hand traffic, the median-side lane of each direction is used for through and permissive-left movements, and the kerb-side lane is used for through and permissive-right movements.

---

## 1. Project Vision

We are building a **distributed, fail-safe traffic and railway-crossing control system**, not a centrally-controlled light board. The system's defining property is that **authority over physical outputs is local**: nine local controllers — six intersection local controllers `L1–L6` (each owning its physical intersection `I1–I6`) and three railway-crossing local controllers `RL1–RL3` (each owning its physical crossing `RC1–RC3`) — each own a distinct set of physical devices and keep operating safely and independently of everything else in the network. A tenth node, the **Central Controller `C1`**, has no actuation power at all — it exists purely to *observe*, *coordinate*, *reconfigure within safe bounds*, and *override within limits that can never violate local safety invariants*.

What makes this a strong EEET2588 project is not the number of intersections modelled, but that it gives a genuine, demonstrable answer to the core real-time systems questions the course is about: what happens when a message is late, when a link drops, when two safety-critical events compete for the same actuator, and when a component fails partway through a critical sequence. The railway crossing is deliberately the centrepiece of the safety argument (an actual life-safety interlock: gate-confirmed-closed before train-proceed), while the six intersections are the vehicle for demonstrating distributed coordination (green-wave-style offsets), local autonomy, and central-command abstraction.

The design deliberately keeps the *traffic simulation* side (vehicle arrival modelling, statistical demand, and even the notion of an individual simulated vehicle) out of scope. This is a **control system** project: sensor *events* (simulated by keyboard input, per [REQ]) are what the system reacts to, and the demonstration only ever shows *signal state* on a terminal — never a simulated vehicle, and therefore never a simulated collision. This has a direct safety-argument consequence explained in Section 5b.

---

## 2. Source of Requirements

| Source | Role |
|---|---|
| `RTS_Final Project.pdf` | Primary authority. Defines: 6 intersections, 5 roads, double-track diagonal railway corridor, boom gates + flashing lights, train headway ranges, local-controls-own-lights principle, Central monitors/does-not-actuate, 3 sequence families (fixed / sensor / advanced), override capability, multi-QNX-node requirement, Pass-minimum scope (I1–I2 + crossing), the instruction to use measured/estimated distance to determine timing, and the encouragement to ground the design in a real intersection. |
| `lecture_clarification.md` | Authoritative supplement. Resolves: 9 local controllers total (6 intersection + 3 railway-crossing, not 6+3 as "same kind"), railway-crossing controllers exclusively own boom gate/flasher/train-signal, intersection controllers may only *sense* railway status, explicit train-signal requirement with fault-reporting, Central→Local command flow (never Central→light directly), keyboard-simulated sensors with an explicit key map, distance-based green-wave coordination (repeated a second time, reinforcing the requirement above), railway-congestion traffic suppression concept, and QNX-node-to-physical-computer flexibility (a single machine may host several logical nodes; the class-wide guidance was roughly 2–3 physical machines for a full demo). |
| `idea.md` | **Not a design.** A brainstorm/question dump generated by another AI assistant, never validated by the team. Used here only to identify which open questions are *high-impact* (answered explicitly below) versus low-value/premature (silently resolved by a reasonable default). Where an earlier draft of this specification uncritically copied a suggestion from `idea.md` (notably the safety priority hierarchy, Section 22), that suggestion has since been independently re-derived from the system's actual behaviour and corrected. |
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

The complete design covers the entire network: 5 roads (`R1–R5`), 6 signalised intersections (`I1–I6`) each with a dedicated local controller (`L1–L6`), 3 railway crossings (`RC1–RC3`) each with a dedicated local controller (`RL1–RL3`), and 1 Central Controller (`C1`) — 10 logical controllers in total [REQ][CLARIF]. All feature families described in this document are part of the full conceptual design and are reflected in the Initial Design Report, state charts, and task architecture regardless of what is actually implemented in code.

### 4.2 Planned Proof of Concept

**Core PoC (guaranteed, satisfies the Pass-level minimum [REQ]):** intersections `I1`/`I2` (controllers `L1`, `L2`), the connecting road `R3`, railway crossing `RC1` (controller `RL1`), and `C1`. This is a fully functional "1/3 slice" — real QNX processes, real IPC, real railway interlock, real degraded-mode behaviour.

**HD-Target PoC extension — committed [TEAM, resolved 2026-08-22]:** the team is targeting HD, not the Pass minimum, so this extension is part of the planned build, not a stretch goal. Duplicate the Core slice once more along one arterial to add `I3`/`I4` (controllers `L3`, `L4`), road `R4`, and railway crossing `RC2` (controller `RL2`) — chosen specifically because a single extra vertical slice cannot demonstrate arterial coordination (green-wave offsets need at least two chained intersections on the same road, e.g. `I1–I3` on `R1`).

**Full 9-controller build (`I5`/`I6` via `L5`/`L6`, `R5`, `RC3` via `RL3`):** Optional/stretch — see 4.3. Because the controller software is generic and config-driven (Goal 5, Section 3), the *design* already covers it; whether it is literally instantiated a third time at demo time is a resourcing decision, not a design gap.

### 4.3 Optional Scope

A dedicated-lane, protected-turn intersection design (Section 5b), a graphical UI beyond the terminal text display [REQ explicitly permits terminal-only], statistical/stochastic vehicle-arrival modelling, persistent database logging beyond the `/fs` test-vector use already permitted [REQ], and the third network slice (`L5/L6` at `I5/I6`, `RL3` at `RC3`) are optional/stretch — see Section 23 for full classification and rationale.

---

## 5. Physical Network

The official diagram [REQ] shows a diagonal double-track railway corridor cutting across a 3×2 grid of intersections. Interpreting that diagram against the Pass-minimum hint ("2 intersections I1–I2 and the railway crossing") and the lecturer's naming clarification [CLARIF] produces the following concrete topology — **[TEAM]**, derived directly from the given diagram, not a topology change, so it does not require lecturer approval:

- **`R1`** — arterial, oriented West–East, running through `I1 → I3 → I5` (the "North row").
- **`R2`** — arterial, oriented West–East, running through `I2 → I4 → I6` (the "South row").
- **`R3`** — connector, oriented North–South, `I1 ↔ I2`, crossing the railway at **`RC1`**.
- **`R4`** — connector, oriented North–South, `I3 ↔ I4`, crossing the railway at **`RC2`**.
- **`R5`** — connector, oriented North–South, `I5 ↔ I6`, crossing the railway at **`RC3`**.
- The double-track railway corridor runs West–East, crossing all three connectors (`R3`/`R4`/`R5`) at `RC1`/`RC2`/`RC3` respectively.

See `SYSTEM_DIAGRAMS.md`, **Diagram 1**, for the full labelled picture of this network (rotated so that `R1`/`R2` are drawn horizontally and `R3`/`R4`/`R5` vertically: travelling from North to South encounters the pair `I1`/`I2`, then `I3`/`I4`, then `I5`/`I6`).

**Arterial vs. connector, in plain terms:** this classification is about *role*, not direction. `R1`/`R2` are the two **long** roads — each runs the full length of the network, linking three intersections in a row. `R3`/`R4`/`R5` are the three **short** roads — each links exactly one intersection pair, and each is the one road that crosses the railway. Arterials carry the higher base priority and are the coordination candidates (green-wave); connectors carry a lower base priority but a higher *coordination sensitivity* to railway conditions, since they are the roads directly exposed to pre-emption and congestion risk.

This gives exactly 5 roads and 6 intersections [REQ], makes the Pass-minimum slice (`I1–I2–RC1`) a literal 1/3 vertical repeat of the full network (satisfying "the complete design should duplicate this part" [REQ]).

**Lane assignment — Core [TEAM]:** every road carries 2 lanes per direction [REQ]. Median-side lane = through + permissive left turn (yields to opposing through traffic); kerb-side lane = through + permissive right turn. No dedicated turn lane and no protected turn-arrow signal exist in the Core design. See `SYSTEM_DIAGRAMS.md`, **Diagram 2**, for the lane layout under right-hand traffic, and **Section 5b** immediately below for the full trade-off analysis and the documented (but deferred) alternative.

Turning-movement proportions and minor/side roads are explicitly **out of scope [ASSUM]**: the brief permits eliminating side roads as not important, and a project about *signal control logic* does not need a traffic-demand model to be credible — sensor *events* (not simulated vehicles) drive all reactive behaviour.

**Distance and speed assumptions [ASSUM, Proposed — Awaiting Confirmation]:** rather than mapping onto a literal named real-world intersection, the network is parameterised with representative illustrative values. Arterial spacing: `I1–I3 = 350 m`, `I3–I5 = 400 m` (on `R1`); `I2–I4 = 320 m`, `I4–I6 = 380 m` (on `R2`) — all within a general 300–500 m range typical of Vietnamese urban arterial spacing. Arterial speed = 60 km/h (16.67 m/s); connector speed = 50 km/h, consistent with typical Vietnamese urban speed limits (QCVN 41:2019/BGTVT). The brief states — in both the official specification and the lecturer's clarification — that distance should be measured or estimated to determine timing requirements; the lecturer's wording explicitly permits *estimating* rather than a literal physical survey, so these illustrative per-segment values (used to actually compute a coordination offset in Section 20, not merely asserted as a range) satisfy this instruction without claiming one specific named site was surveyed.

---

## 5b. Lane Design: Shared Permissive Lanes vs. Dedicated Protected-Turn Lanes

The team raised a direct safety question: if the median-side lane carries both through and permissive-left traffic, and the opposing direction is also green for through movement, is that not a built-in collision? The short answer is **no, not automatically** — but it is a genuine trade-off worth analysing carefully, covering operation, timing, and user experience, not only the narrow technical question.

### Option A — 2 shared lanes, permissive turns, one circular signal head per approach (the Core design)

**How it works:** during one direction's green phase (e.g. the arterial), **both** opposing directions are green simultaneously (through + permissive left + permissive right). A left-turning vehicle must yield to the oncoming through movement, exactly as most small/medium Vietnamese intersections operate without a dedicated arrow (drivers judge the gap themselves). This is **not a design defect that guarantees an accident** — it is a standard, globally used operating pattern; the residual safety risk depends on driver judgement, not on a signal logic error.

**Advantages:** a much shorter cycle (only 2 phases/intersection — arterial vs. connector) → lower average delay for every road user; far fewer actuators/sensors/states → lower implementation risk within the fixed PoC timeframe, leaving more time for the committed HD features (green-wave, congestion, watchdog, multi-node); a simple conflict matrix (Section 22) that is easy to prove correct — important given this project is assessed on safety correctness, not on traffic realism.

**Disadvantage:** the left-turn conflict point carries a higher residual safety risk than Option B, since it depends on driver judgement rather than being physically prevented by the signal.

### Option B — 3 dedicated lanes (left/through/right), 3 arrow heads per approach, protected left turn

**How it works:** each approach gets a dedicated left-turn lane, a dedicated through lane, and a dedicated right-turn lane, each with its own arrow signal. Left turn is permitted **only** during its own protected arrow phase, when the opposing direction is red — eliminating the permissive-left conflict entirely. This is the pattern the team observed at some District 2 intersections.

**Disadvantages (the drawback analysis explicitly requested by the team):**
1. **Worse timing/user experience for everyone.** A protected left turn needs its own phase (it cannot run concurrently with opposing through traffic). Going from 2 phases/intersection to a minimum of 4 (arterial through+right, arterial protected-left, connector through+right, connector protected-left) means each added phase needs its own yellow + all-red clearance. Net effect: a substantially longer cycle, raising average delay for **every** road user, including those never turning left — a direct "safer for a small group" traded against "slower for everyone" cost.
2. **A large increase in actuators and state-machine complexity.** From 1 circular head/approach to 3 arrow heads/approach × 4 approaches × (5–9 intersections depending on scope) — more states, a larger safety conflict matrix to prove correct (Section 22), and more surface area for a bug to survive to demo day.
3. **Requires per-lane sensors** for `OFF_PEAK_SENSOR` to react correctly (e.g. not serving a protected-left phase with nobody waiting to turn left) — this reverses an explicit, well-justified Core decision (Section 23, Rejected: "per-lane signals/sensors").
4. **Schedule risk.** The team has already committed to an ambitious HD scope (green-wave + congestion management + watchdog + multi-node + railway interlock). Adding another large feature area increases the risk of being probed hard on exactly the newest part during Q&A, and reduces the time available to polish the features already committed — a genuine project-management risk, not only a technical one.
5. **Requires adequate left-turn storage length**, a lane-geometry/queueing problem the specification deliberately placed out of scope from the start (Section 5), since this is a control-system project, not a capacity/geometry simulator.
6. **"Yellow trap" hazard.** If protected and permissive phases are mixed carelessly (e.g. a permissive-left phase ending while the opposing direction still has a longer green), a driver can be misled into believing it is safe to turn when it is not. A fully safe design must be **protected-only** (left turn permitted only during the arrow phase, never during the circular green) — reducing flexibility.

### Why permissive-left is not a safety gap — and why "accident" does not apply to this PoC

The key realisation from discussing this with the team: **this proof-of-concept does not simulate individual vehicles or vehicle-to-vehicle collisions at all** — the system only simulates sensor *events* (keyboard input) and displays *signal state*, exactly as the brief allows ("sufficient to display the state... by text messages"). So, literally, there is no "accident" to avoid in the demonstration — there are no vehicle objects that could collide.

At the level of real-world operating theory, two categories of conflict must be distinguished:

| Conflict type | Example | Who resolves right-of-way? | Safe by definition? |
|---|---|---|---|
| Undefined ("prohibited") | Two different roads both green | Nobody — the signal logic itself is contradictory | **No** — this is exactly what Safety Invariant #1 (Section 22) must prevent absolutely |
| Already resolved ("permitted") | One direction through, opposing direction permissive-left | Traffic law: left-turning traffic must yield to oncoming through traffic | **Yes** — the standard operating pattern at the majority of intersections worldwide |

Permissive left falls into the second category: it has a well-defined right-of-way rule, enforced by drivers rather than by the signal — a risk that traffic law resolved long before traffic signals existed, not a gap the signal system created.

### Recommendation and final decision **[TEAM, decided]**

**Core keeps Option A (2 shared lanes, permissive).** A formal assumption is recorded (Section 26) that the system does not simulate, and is not responsible for, vehicle-to-vehicle collisions, since no vehicle is ever simulated — only sensor events and signal states. Option B is recorded as a possible **future** upgrade direction, not a target for this course — no additional diagram is needed for it at this stage. If the team completes the committed HD scope early, the next reasonable stretch step would be upgrading only the two intersections nearest the first railway crossing (`I1`/`I2`, where a protected left turn interacting with railway pre-emption carries the highest safety value) rather than the whole network — see Section 23, Optional/Stretch.

---

## 6. System Components

**Physical infrastructure:** 5 roads, 6 signalised intersections, 3 railway level crossings (double track, one line per direction), 24 vehicle signal heads (4 approaches × 6 intersections), 24 pedestrian crossings with 24 push-buttons + 24 pedestrian signal heads, 6 boom gates (2 per crossing), 6 sets of road-facing flashing lights (2 per crossing), 6 train signals (2 per crossing, one per rail direction).

**Logical controllers:** `L1–L6` (intersection local controllers, one per physical intersection `I1–I6`), `RL1–RL3` (railway-crossing local controllers, one per physical crossing `RC1–RC3`), `C1` (Central Controller). 10 logical controllers total [CLARIF].

**Software/QNX architecture:** each logical controller is one or more QNX **processes**; controllers are distributed across multiple QNX **nodes**; nodes are distributed across a small number of physical/virtual machines at demo time (Section 27). The 1:1:1 mapping (controller = node = computer) is explicitly *not* assumed [CLARIF].

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

     Devices owned by each Lx:                 Devices owned by each RLx:
       - Vehicle signal heads                    - Boom gates
       - Pedestrian signal heads                  - Road-facing flashing lights
       - Vehicle presence sensors                 - Train signals
       - Pedestrian buttons                       - Train approach sensors
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

All three `RLx` instances run *identical* logic — the brief gives no reason for them to differ, and inventing per-crossing behavioural differences would add untestable surface area for no teaching value **[TEAM — reject differentiation]**. `RL1`, `RL2`, and `RL3` also operate **completely independently of one another** — no shared state, no cross-crossing coordination or propagation model (there is, for instance, no assumption that a single train sequentially triggers all three crossings). This is a separate concept from the arterial coordination described in Section 20, which coordinates *road intersections*, not railway crossings.

---

## 9. Sensors

| # | Sensor | Location | Owner | Event | Purpose / Simulation |
|---|---|---|---|---|---|
| 1 | Approach vehicle presence sensor | At stop line, 1 per approach (4 per intersection) | `Lx` | `DEMAND_PRESENT` / `DEMAND_ABSENT` | Standard inductive-loop equivalent: "is a vehicle currently waiting on this approach?" Keyboard key per approach. |
| 2 | Advance/queue sensor **[HD]** | ~60–80 m back from the stop line, only on the 2 approaches that feed directly into a railway crossing | `Lx` | `QUEUE_WARNING` | Picture two trip lines: line A at the stop line (sensor #1), line B about 70 m further back, closer to the tracks (sensor #2). Sensor #1 answers "is anyone waiting at all?" Sensor #2 answers a different question: "has the queue grown all the way back to line B?" — a more urgent signal meaning the queue is long enough to risk backing up onto the tracks. It feeds the congestion-suppression logic in Section 16, not the ordinary phase-extension logic. It is a simple binary presence trip, not a vehicle counter. A dedicated keyboard key. |
| 3 | Pedestrian push-button | 1 per crossing side (4 per intersection) | `Lx` | `PED_REQUEST(side)` | Directly required [REQ]. Keyboard key. |
| 4 | Train approach sensor | Fixed distance before the crossing, 1 per rail direction (2 per crossing) | `RLx` | `TRAIN_APPROACHING(dir)` | Directly required [REQ] — this is the event that starts the entire railway safety sequence (Section 14). Keyboard key. |
| 5 | Train exit/clearance sensor — **reserved, not used in Core logic [TEAM, revised]** | Just past the crossing, 1 per direction (2 per crossing) | `RLx` | *(not wired into the Core reopening decision)* | Originally planned to confirm exactly when a train has cleared the crossing. **Changed:** the Core design instead assumes every train takes the same fixed, known amount of time to clear the crossing once it arrives (Section 14), so reopening is computed from a timer, not from this sensor. The sensor is kept in the physical inventory as a placeholder for a **future enhancement** — e.g. cross-checking that a train actually cleared within the assumed window, to catch an abnormally slow or stalled train — but that check is explicitly out of scope for the Core safety logic today. |
| 6 | Boom-gate position sensor | On each gate arm (2 per crossing) | `RLx` | `GATE_CLOSED_CONFIRMED` / `GATE_OPEN_CONFIRMED` / `GATE_FAULT` | This sensor **does not care how long closing/opening takes** — at any given moment it answers exactly one question: "right now, is the gate closed or open?" It is the second, independent layer in the design, entirely separate from the timing layer (Section 14 assumes ~10 s to close — that number is only used to *schedule in advance*). Hard rule: even once the scheduled 10 s has elapsed, the system **never** infers "it must be closed by now" — it always waits for sensor #6's real confirmation; if the deadline passes without `GATE_CLOSED_CONFIRMED`, that is a fault (Invariant #6), not "a little late is fine." Keyboard fault-injection key. |

Only these six sensor types are included. Vehicle counting, ANPR, weather sensors, and per-lane sensors (Section 5b) were considered and rejected — see Section 23.

**The two layers of the entire railway safety chain (Section 14), summarised:**
1. **Timing layer (the plan):** assumed figures such as "gate takes ~10 s to close" or "total lead time ~45 s" — used only to *schedule in advance*, never to make a safety decision.
2. **Verification layer (the reality):** sensor #6 confirms the actual gate state at the moment a decision is needed — this is what actually gates the `PROCEED` decision.
Keeping these two layers separate is deliberate, so "the scheduled time has elapsed" can never be mistaken for "therefore it is safe" without checking reality.

---

## 10. Actuators and Signals

| # | Actuator | Owner | States | Safe State |
|---|---|---|---|---|
| 1 | Vehicle signal head (per approach) | `Lx` | `RED / YELLOW / GREEN / FLASHING_RED` | `FLASHING_RED` (all-way, real-world power-fail convention — forces every approach to yield) |
| 2 | Pedestrian signal (per crossing side) | `Lx` | `WALK / FLASHING_DONT_WALK / DONT_WALK` | `DONT_WALK` |
| 3 | Boom gate (per direction) | `RLx` | `UP / MOVING / DOWN / FAULT` | `DOWN` (ambiguity always resolved toward blocking the road, never toward allowing the train through unconfirmed) |
| 4 | Railway flashing lights (per road-approach direction) | `RLx` | `ON / OFF` | `ON` |
| 5 | Train signal (per rail direction) | `RLx` | `STOP / PROCEED` | `STOP` |

No dedicated turn-arrow actuators exist in the Core design (Section 5b, Section 23). The 3-arrow-head design (dedicated left/through/right lanes) is fully documented as a considered design option — see Section 5b — but remains a future-upgrade direction, not a Core actuator.

---

## 11. Operating Modes

**In plain terms, before the detailed table:**
- `PEAK_FIXED` = peak hour — lights run on a fixed schedule, not waiting for vehicles.
- `OFF_PEAK_SENSOR` = quiet hours — lights wait for a vehicle/pedestrian before turning green.
- `RAILWAY_PREEMPTION` = a train is coming — the movement heading toward the tracks must turn red earlier than normal, regardless of which of the two modes above is active.
- `CENTRAL_OVERRIDE` = the control-room operator issues a special command (e.g. clearing a route for a priority convoy).
- `DEGRADED_LOCAL` = the link to Central is lost — the lights keep running safely on the last known good configuration.
- `FAULT_SAFE` = a fault has been detected — the lights switch to the safest state (flashing red) and stop reacting normally.

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

Each intersection is modelled as a standard 2-phase 4-leg junction (arterial vs. connector), using the Core 2-lane permissive design (Section 5b, Option A — see 5b for the documented alternative with per-lane phasing):

- **Phase A** — arterial (`R1`/`R2`) through+left green, connector red.
- Yellow (4 s **[ASSUM]**) → All-red clearance (2 s **[ASSUM]**).
- **Phase B** — connector (`R3`/`R4`/`R5`) through+left green, arterial red; compatible pedestrian WALK runs concurrently.
- Yellow → All-red clearance → repeat.

`PEAK_FIXED`: total cycle is **exactly 90 s**: Phase A (arterial) = 48 s green + 4 s yellow + 2 s all-red = 54 s; Phase B (connector) = 30 s green + 4 s yellow + 2 s all-red = 36 s; 54 s + 36 s = 90 s. The green ratio (48:30, ≈3:2) matches the arterial's higher assumed priority (Section 5). Sensors are monitored but do not alter timing in this mode, preserving the deterministic timing that coordination/green-wave depends on.

**Why 90 s, and why it need not divide evenly into the train headway (2 min = 120 s):** unlike some earlier designs that tie cycle length to train headway, this cycle length is deliberately *not* chosen for arithmetic neatness against the headway, because `RAILWAY_PREEMPTION` is event-driven — it interrupts whichever phase is active immediately, through the normal safe clearance sequence, rather than waiting for a cycle boundary. This is more robust than assuming trains always arrive at a clean multiple of the signal cycle, since real trains do not run on that assumption.

`OFF_PEAK_SENSOR`: a phase only receives green when its approach(es) report demand or hold a latched pedestrian request; green extends in 4 s increments **[ASSUM]** up to a 40 s cap **[ASSUM]** while demand persists; with zero demand anywhere, the intersection rests on the arterial phase (default main-road priority) [REQ — "differ between R1-R5, teams must state and justify"]. A minor road never waits longer than one full max-extended opposing phase plus its own minimum green — a structural anti-starvation bound rather than a weighting algorithm.

Mode switches (`PEAK_FIXED ↔ OFF_PEAK_SENSOR`, whether local time-of-day or Central-commanded) are applied only at the next phase boundary, never mid-phase **[TEAM]** — this preserves every clearance/min-green invariant regardless of when the switch request arrives.

---

## 13. Pedestrian Behaviour

- 4 crossings per intersection (N/E/S/W), 1 push-button per crossing side [REQ].
- A button press **latches** a single pending request per side; repeated presses while already latched have no additional effect (10 presses = 1 request) **[TEAM]**.
- Pedestrian WALK runs **concurrently** with the vehicle phase that does not conflict with that crossing (a common parallel-walk pattern at signalised intersections), not as a separate all-red "scramble" phase — a scramble phase was considered and rejected as unjustified added complexity (Section 23).
- Signal sequence: `WALK → FLASHING_DONT_WALK (clearance) → DONT_WALK` **[TEAM]** — a standard 3-state pedestrian signal pattern, not a bare two-state signal.
- Served in both `PEAK_FIXED` (guaranteed once per cycle, since both phases run every cycle) and `OFF_PEAK_SENSOR` (a latched request is treated exactly like vehicle demand for scheduling purposes — the two are never ranked one above the other).
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
- **Warning-to-arrival lead time** — derived from four components: flashing-only warning (5 s) + gate-closing duration (10 s) + closed-confirmation safety margin (5 s) + margin for the adjacent intersection's own safe clearance sequence (25 s) ⇒ target **~45 s from train-detected to train-at-crossing**. Approach-sensor placement distance follows from this lead time and the assumed maximum train speed of **80 km/h (22.2 m/s)** [ASSUM] ⇒ 45 s × 22.2 m/s ≈ **1000 m**.
- **Crossing-occupancy duration** (step 6 above) — an assumed fixed value, **~20 s**, representing train length + crossing width at the assumed train speed, identical for every simulated train.

Both values are explicitly presented in the report as derived, tunable parameters, not fabricated facts.

**Accepted trade-off:** using a fixed timer instead of a confirmed-exit sensor means the design does not, today, detect the specific edge case of a train that becomes abnormally slow or stalled on the crossing. This is a deliberate, documented simplification for the Core design — not an oversight — and the reserved exit sensor (Section 9, item 5) is the identified path for closing that gap later if time permits.

---

## 15. Railway Pre-emption and Traffic Clearing

**Plain description:** "pre-emption" here works like an ambulance taking priority at a signal — except the thing being protected is the level crossing, not a vehicle. The moment `RLx` detects an approaching train, it does not wait for the adjacent intersection's normal cycle to get around to the connector-road phase; it immediately tells the adjacent `Lx` controllers "a train is coming," and each `Lx` reprioritises so that the specific vehicle movement pointed at the crossing loses its green (through the normal safe clearance sequence, not abruptly) well before the gate needs to come down.

Pre-emption is **directional, not intersection-wide [TEAM]**: only the vehicle movement(s) that lead onto the closing/closed crossing are forced red; cross-traffic and pedestrian phases not conflicting with that movement continue to be served normally by the same `Lx`. For example, at `I1` (adjacent to `RC1` via `R3`), only the `R3`-toward-the-crossing movement is affected — the `R1` arterial phase and any pedestrian crossing not conflicting with the `R3` movement keep operating on their ordinary schedule. This is both more realistic and strictly safer to implement than a full-intersection freeze, since it avoids inventing new conflict cases beyond the intersection's normal conflict matrix.

The toward-crossing movement's transition to red always completes through the ordinary yellow + all-red clearance (Safety Invariant) — pre-emption changes *scheduling priority*, never *skips* the clearance sequence, because doing so would itself create a hazard at the intersection.

---

## 16. Railway Congestion Management

Per the lecturer's clarification, the goal is explicitly to stop *feeding* traffic toward a closed crossing, not to invent traffic rerouting the light system cannot actually perform. Concretely, the traffic-light system can do exactly three things, and no more:

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

**Worked example** (the brief requires distance to be used to determine timing — stated twice, in both the official specification and the lecturer's clarification — so the offsets are computed explicitly here, not just left as a formula), using Section 5's assumed distances and 60 km/h (16.67 m/s) arterial speed:

| Segment | Distance | Offset (distance ÷ speed) | Cumulative offset from chain start |
|---|---|---|---|
| `L1` → `L3` (R1) | 350 m | 350 ÷ 16.67 ≈ 21 s | `L3` = 21 s behind `L1` |
| `L3` → `L5` (R1) | 400 m | 400 ÷ 16.67 ≈ 24 s | `L5` = 45 s behind `L1` |
| `L2` → `L4` (R2) | 320 m | 320 ÷ 16.67 ≈ 19 s | `L4` = 19 s behind `L2` |
| `L4` → `L6` (R2) | 380 m | 380 ÷ 16.67 ≈ 23 s | `L6` = 42 s behind `L2` |

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

### Priority order when multiple conditions occur simultaneously **[TEAM, decided]**

**Transparency note:** an earlier draft copied a ranking suggestion almost verbatim from `idea.md`. As the team pointed out, `idea.md` is only a brainstorm question list generated by another AI, never validated against the system's actual behaviour — not solid enough grounding for a core safety section. The ranking below was **re-derived from first principles**, by checking, for every pair of conditions, whether they can genuinely compete for the same output at the same time, based on the concrete behaviour described in Sections 11–21.

**Step 1 — these are genuinely mutually-exclusive MODES (a controller is in exactly one of these at any instant):**

```text
1. FAULT_SAFE                    — does not really "rank" alongside the other modes; it is an
                                    absolute local veto, applying when a controller can no
                                    longer trust its own output
2. RAILWAY_PREEMPTION            — overlays the currently running mode, but ONLY affects the
                                    approach heading toward the crossing (Section 15), never
                                    the whole intersection
3. CENTRAL_OVERRIDE              — may overlay the baseline mode, but is REJECTED if it
                                    conflicts with an active RAILWAY_PREEMPTION (Section 18) —
                                    hence always ranks below item 2
4. PEAK_FIXED / OFF_PEAK_SENSOR  — the baseline mode, active when none of the above apply
```

(`DEGRADED_LOCAL` sits outside this axis — it describes "is Central reachable," not a hazard to be ranked.)

**Step 2 — these are CONSTRAINTS that apply under every mode in Step 1, not separate competing "modes" — ranking them as their own priority tiers, as an earlier draft did, was incorrect:**

- **An in-progress pedestrian clearance must always run to completion** (Invariants #4, #12), regardless of mode. On close inspection of the actual behaviour in Section 15: `RAILWAY_PREEMPTION` only forces **one specific vehicle movement** (the one heading toward the crossing) to red — it never needs to turn green for a movement that conflicts with an active pedestrian WALK. So, in practice, pedestrian clearance **never actually competes** with `RAILWAY_PREEMPTION` — this is a constraint that always holds, not a tier to be ranked above or below item 2.
- **Congestion suppression and the drain phase (Section 16)** are behaviour *inside* `PEAK_FIXED`/`OFF_PEAK_SENSOR`, not a separate mode standing alongside `CENTRAL_OVERRIDE`.
- **Pedestrian requests and vehicle sensor demand are treated EQUALLY** when scheduling phases — this is explicitly stated in Section 13 ("a latched request is treated exactly like vehicle demand for scheduling purposes"). The earlier draft ranked these as two separate tiers (with pedestrians above vehicles) — **this directly contradicted Section 13** and has been corrected here.

**Final, decided conclusion:**

```text
MODE priority axis (mutually exclusive):
  1. FAULT_SAFE                      - absolute local veto
  2. RAILWAY_PREEMPTION              - only affects the approach heading toward the crossing
  3. CENTRAL_OVERRIDE                - rejected if it conflicts with item 2
  4. PEAK_FIXED / OFF_PEAK_SENSOR    - baseline mode

Constraints applying under every mode above (not separate priority tiers):
  - An in-progress pedestrian clearance is never interrupted
  - Congestion suppression + the drain phase are internal logic of the baseline mode
  - Pedestrian requests and vehicle sensor demand are scheduled equally
```

The team confirmed: pedestrians only cross at intersections and never cross the tracks, so pedestrian WALK and `RAILWAY_PREEMPTION` have no scenario in which they genuinely conflict. The team also confirmed the contradiction with Section 13 (the earlier draft ranked "pedestrian requests" above "vehicle sensor demand," while Section 13 states the two are scheduled equally) is real and has been corrected in this final version.

---

## 23. Feature Classification

### Core (required for the assignment to be satisfied)

- Six intersections + railway crossings design [REQ]
- A local controller per intersection, operating continuously [REQ]
- Central controller with monitoring/status/commands [REQ]
- Local controllers control their own lights and survive loss of Central/comms [REQ]
- Three sequence families: fixed / sensor-driven / advanced [REQ]
- Boom gates + flashing lights + train signal, fail-to-red [REQ][CLARIF]
- Keyboard-simulated sensors [REQ]
- Multiple QNX nodes with separate processes [REQ]
- Terminal-based state display [REQ]
- Pass-minimum PoC controller set (`L1`, `L2`, `RL1` at `I1`, `I2`, `RC1`) [REQ]

### HD-Target — committed, will be built (per Section 4.2, not optional)

- The `L3`/`L4`/`RL2` extension itself (Section 4.2)
- Advance/queue sensor + explicit congestion-suppression and drain-phase logic (Section 16) — directly answers the brief's "state and justify... congestion control" requirement
- Arterial green-wave coordination (Section 20) — genuine distributed-systems + timing-analysis content
- Local authority to reject an unsafe command with NACK reporting (Section 18) — a concrete, demonstrable safety argument
- Per-node watchdog/supervisor process (Section 17) — a small extra QNX process per node whose only job is to check that its supervised controller process is still alive and, if not, force that node's outputs to their safe state directly
- Bounded, auto-expiring Central override with explicit subordination rules (Section 22)

### Optional — to be attempted later if time permits (not committed, NOT the same as rejected)

These are ideas the team has **not decided to abandon**, only not yet part of the mandatory build plan. If the committed HD-Target scope above is finished early, this is the priority list to attempt next:

- Extend the PoC to the full 9-controller network (`L5/L6` at `I5/I6`, `RL3` at `RC3`)
- Dedicated-lane, protected-turn design for `I1`/`I2` (Option B, Section 5b)
- A graphical (non-terminal) display
- Real-time exit-sensor cross-checking in place of the fixed timer (see the accepted trade-off in Section 14) — the physical sensor already exists in the design (Section 9, item 5), only the logic is not wired up

### Rejected outright — will not be built, ever (a final decision, distinct from "Optional" above)

| Idea | Why rejected |
|---|---|
| Statistical/stochastic vehicle-arrival simulation | This is a *control system* project; sensor events (keyboard-simulated per [REQ]) are the correct input model — building a traffic simulator adds a whole separate subsystem to validate for no assessment credit |
| Pedestrian "scramble" (all-red) phase | Ordinary signalised intersections default to parallel walk; scramble adds a distinct conflict-matrix mode with no requirement basis |
| Per-lane signal heads / per-lane sensors (in Core) | Brief only requires 2 lanes/direction with turn lanes "may have" — per-lane granularity multiplies state without a corresponding requirement; full trade-off analysis in Section 5b |
| 4-tier time-of-day demand model (morning/day/evening/night) | 2 tiers (Peak/Off-Peak) already exercise every mode-transition and coordination mechanism; a 4th tier is more numbers to justify, not more design |
| Directional demand bias / burst arrival modelling | No sensor or actuator behaviour depends on modelled arrival statistics — this would be simulation realism with zero effect on the control logic being assessed |
| Gradual green-wave resynchronisation algorithm after a train event | The normal reconnection/reprofile path already recovers coordination; a bespoke resync transient is a second state machine solving a problem the first one already solves |
| Local-to-local coordination for connector roads across the railway | Connectors are short and railway-pre-emption-dominated; coordinating their timing has no meaningful travel-time benefit to model |
| `C1` able to override railway equipment in an emergency | Explicitly rejected on safety grounds — no remote authority may ever bypass the gate-confirmed-closed interlock; matches the lecturer's own stated recommendation |

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
| 1 | Right-hand traffic (Vietnam convention); median-side lane = through+left, kerb-side lane = through+right | The project's real-world context is Vietnam; the brief does not mandate a specific country/convention, only encourages a real intersection reference | Determines Diagram 2's orientation and the lane assignment in Section 5 | No |
| 2 | Arterial speed 60 km/h, connector speed 50 km/h; illustrative per-segment distances `I1–I3=350m`, `I3–I5=400m`, `I2–I4=320m`, `I4–I6=380m` (within a general 300–500 m range) | Representative Vietnamese urban values (QCVN 41:2019/BGTVT speed range); the brief states twice (official spec + lecturer clarification) that distance must be measured/estimated to derive timing, so concrete per-segment figures are used to compute a real offset (Section 20) rather than only stating a range | Feeds cycle length, green split, coordination offsets (offsets computed: 21 s, 24 s, 19 s, 23 s — Section 20) | No |
| 3 | Min green 8 s, max green 40 s, yellow 4 s, all-red 2 s | Typical Australian/Vietnamese urban signal-engineering ranges, not tied to one specific national standard | Bounds every phase timer in the design | No |
| 4 | `PEAK_FIXED` cycle exactly 90 s: Phase A (arterial) 48 s green + 4 s yellow + 2 s all-red = 54 s; Phase B (connector) 30 s green + 4 s yellow + 2 s all-red = 36 s; not deliberately synchronised with the 120 s train headway since pre-emption is event-driven | Representative mid-size intersection cycle; green ratio 48:30 matches arterial priority | Basis for fixed-mode timing and green-wave offsets (Section 20) | No |
| 5 | Railway warning-to-arrival lead time ≈ 45 s, derived from flash-lead (5 s) + gate-close (10 s) + margins (5 s + 25 s) | No lead time is given in the brief; a methodology-derived value is safer than an invented one | Sets approach-sensor placement distance and pre-emption reaction budget | No — flagged as tunable |
| 6 | Fixed crossing-occupancy duration ≈ 20 s, identical for every train | Simplifies reopening logic to a timer instead of requiring exit-sensor confirmation, per team decision | Gate reopening (Section 14) is timer-based; exit sensor is reserved, not core | No — flagged as tunable; documented accepted trade-off (no detection of an abnormally slow/stalled train in Core scope) |
| 7 | Assumed maximum train speed = 80 km/h (22.2 m/s) | No train speed is given in the brief; a representative value for a regional/commuter double-track line, stated as its own explicit assumption | Feeds the approach-sensor placement-distance calculation (Section 14) | No |
| 8 | Core uses 2 lanes/direction, permissive turns, no arrow signals (instead of a 3-lane protected design) | Full analysis in Section 5b: the schedule risk and cycle-length increase (affecting user experience) of the 3-lane option outweigh its safety benefit at the left-turn conflict point, given this course's assessment scope | Core actuator/sensor/conflict-matrix stays simple; the 3-lane option is recorded as a future upgrade direction, not Core | No — internal team decision |
| 9 | The system does not simulate individual vehicles or vehicle-to-vehicle collisions — only sensor events (keyboard input) and signal states | The PoC only displays signal state via terminal [REQ]; no vehicle/driver is simulated. The collision risk associated with permissive left turns depends on a real-world right-of-way rule enforced by drivers, which is outside anything this system can simulate or be responsible for (see Section 5b) | Confirms Core keeps the 2-lane permissive design; the demonstration will never show a "vehicle" or an "accident," only valid light/sensor state | No |
| 10 | Only 2 demand tiers, Peak/Off-Peak | Sufficient to exercise every mode transition without unjustified extra numbers | Simplifies `SET_MODE` and time-of-day self-selection | No |
| 11 | No stochastic vehicle-arrival modelling; sensors are event-driven only | Project is a control system, not a traffic simulator | Sensor input model is entirely keyboard-event-based | No |
| 12 | Heartbeat 1 s, link-down after 3 missed | Fast enough for a compressed demo, standard missed-N pattern | Basis for `DEGRADED_LOCAL` entry timing | No |
| 13 | Override max duration 5 minutes, auto-expiring | Prevents a forgotten override from permanently altering operation | Bounds `CENTRAL_OVERRIDE` mode | No |
| 14 | Pedestrian crossings exist only at intersections, not at railway crossings | Brief places pedestrian crossings "at each intersection" only | Simplifies railway pre-emption/pedestrian interaction analysis | No |
| 15 | Demo topology parameterised as a "generic realistic" network (inspired by District 2 intersections) rather than mapped onto one named real intersection | Satisfies the brief's intent (ground timing in real principles) without altering the given diagram's topology | Avoids the topology-change lecturer-approval trigger entirely | No, by design |
| 16 | Demo deployment uses 2 physical machines (Section 27) | Fewer physical/network failure points during a live, assessed demo; QNX nodes are logical and location-transparent, so 2 machines can still host every required separate process/node | Deployment diagram targets 2 machines; can be scaled to 3 later without any code change | No |

---

## 27. Open Design Decisions — resolved 2026-08-22

All items below were open in earlier drafts and have since been decided by the team:

1. **HD-extension PoC commitment — RESOLVED: committed.** The team is targeting HD. The `L3`/`L4`/`RL2` extension (Section 4.2) is part of the planned build, not a stretch goal.
2. **Final physical machine count — RESOLVED: 2 machines, recommended.** A QNX "node" is a logical/software concept, not a physical one — Qnet message passing works identically whether two processes are on the same machine or different machines. Two physical machines can therefore still host every required separate controller process on its own QNX node, fully satisfying "multiple QNX nodes with separate processes" [REQ], without the extra demo-day risk (hardware, network configuration) a third machine would add.
3. **Per-node watchdog/supervisor process — RESOLVED: included, since the team is targeting HD.** A small, cheap QNX process per node whose only job is to check that the real controller process is still responding and, if not, force that node's outputs to their documented safe state (Section 10) immediately.
4. **Driving convention — RESOLVED: Vietnam, right-hand traffic.** See Section 5.
5. **Core lane design — RESOLVED: 2-lane permissive, no arrow signals.** The 3-lane protected-turn option is recorded as a possible future upgrade direction (Section 5b), not a target for this course.
6. **The PoC does not simulate vehicles or vehicle collisions — RESOLVED.** The system only simulates sensor events and signal states; no "vehicle" or "accident" is ever demonstrated (Assumption #9, Section 26).

---

## 28. Questions for Team / Lecturer

**Short answer: nothing in this specification currently requires the lecturer's sign-off.** The only trigger the lecturer described for needing approval was *changing the given map/topology* (e.g. altering lane counts or the intersection layout). This design does not do that — Section 5 only names and orients the roads that are already implied by the official diagram and the Pass-minimum hint, and the choice of driving convention/real-world reference is not a topology change either. Every numeric timing value in Section 26 is presented as a derived, tunable engineering assumption, not a requirement claim, so none of them need approval either.

The only remaining "confirm with someone" item is purely a team preference, not a lecturer question: if the team later wants the report to reference one specific named real-world intersection in Vietnam (rather than the generic realistic parameterisation in Assumption 15), that's a one-line internal decision, not something that needs an email to the lecturer.

---

## 29. Traceability

| Feature | Origin |
|---|---|
| 6 intersections, 5 roads, railway corridor, boom gates/flashers, local-controls-own-lights, Central monitors-only, 3 sequence families, override, multi-node QNX, Pass-minimum slice | [REQ] |
| 9 local controllers total, `RLx` exclusive railway-equipment ownership, train signal + fault-report requirement, keyboard key-map expectation, 2–3 machine deployment guidance | [CLARIF] |
| `Lx`/`Ix` and `RLx`/`RCx` naming split, road/road-name topology mapping (Section 5), Vietnam right-hand-traffic convention, Core lane decision (Section 5b), 2-phase intersection model, pedestrian parallel-walk pattern, re-derived priority hierarchy, command abstraction detail, degraded-mode resync direction, timer-based crossing-occupancy decision, rejected-feature list | [TEAM] |
| All numeric timing values, sensor placement distances, heartbeat/timeout numbers, fixed crossing-occupancy duration, assumed train speed | [ASSUM] |
| Advance/queue sensor + congestion suppression, green-wave coordination, watchdog/supervisor process, bounded auto-expiring override, local-reject-unsafe-command | [HD] |

---

## 30. Final Proposed System Summary

The system is a distributed, safety-first traffic and railway control network: 6 intersection local controllers (`L1–L6`, one per physical intersection `I1–I6`) and 3 railway-crossing local controllers (`RL1–RL3`, one per physical crossing `RC1–RC3`) each hold exclusive, offline-capable authority over their own physical devices, coordinated — never commanded at the light level — by a Central Controller (`C1`) that can only monitor, reconfigure within safe bounds, and issue bounded overrides that local controllers may refuse. The network is grounded in a Vietnamese, right-hand-traffic context, with topology `R1`/`R2` (arterial, West–East) and `R3`/`R4`/`R5` (connector, North–South, crossing the railway corridor at `RC1`/`RC2`/`RC3`). The railway crossing is the safety centrepiece (gate-confirmed-closed before train-proceed, a fixed occupancy-window timer governing safe reopening, fail-to-safe on every ambiguity), while the six-intersection network is the coordination/distribution centrepiece (2-phase local sequencing with permissive-turn lanes, sensor-driven off-peak behaviour, arterial green-wave offsets computed from real assumed distances, and structural anti-starvation). The guaranteed proof-of-concept is the Pass-minimum controller set `L1`/`L2`/`RL1` (at `I1`/`I2`/`RC1`) plus `C1`, built as the same generic, config-driven controller software that — by design, not by promise — duplicates cleanly to the full 6-intersection, 3-crossing network. The team is committing to the HD-target extension (`L3`/`L4`/`RL2` at `I3`/`I4`/`RC2`), deployed across 2 physical machines to keep demo-day risk low while still satisfying the multi-QNX-node requirement. A dedicated-lane, protected-turn intersection design (Section 5b) was researched and fully documented as a justified upgrade direction, held for a future iteration beyond this course rather than built now, alongside an explicit, re-derived safety-priority hierarchy (Section 22) that the team independently verified rather than inheriting from an unvalidated brainstorm document.
