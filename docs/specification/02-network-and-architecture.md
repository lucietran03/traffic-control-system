[← Table of Contents](README.md) · Chapter 2/6 · [← Vision and Scope](01-vision-and-scope.md) · Next: [Operations and Behavior →](03-operation-and-behavior.md)

---

## 5. Physical Network

The official diagram [REQ] shows a double-track railway corridor running diagonally across a 3×2 grid of intersections. Interpreting that diagram together with the minimum-Pass-level hint ("2 intersections I1–I2 and the railway crossing") and the instructor's naming clarification [CLARIF] yields the following concrete topology — **[TEAM]**, derived directly from the original diagram, not a change to the topology, so it does not require instructor approval:

- **`R1`** — West–East arterial, passing through `I1 → I3 → I5` (northern row).
- **`R2`** — West–East arterial, passing through `I2 → I4 → I6` (southern row).
- **`R3`** — North–South connector road `I1 ↔ I2`, crossing the railway at **`RC1`**.
- **`R4`** — North–South connector road `I3 ↔ I4`, crossing the railway at **`RC2`**.
- **`R5`** — North–South connector road `I5 ↔ I6`, crossing the railway at **`RC3`**.
- The double-track railway corridor runs horizontally West–East, crossing all 3 connector roads `R3`/`R4`/`R5` at `RC1`/`RC2`/`RC3`.

See `SYSTEM_DIAGRAMS.md`, **Diagram 1**, for a complete visual rendering of this network (rotated to the orientation the team requested: `R1`/`R2` run horizontally West–East, `R3`/`R4`/`R5` run vertically North–South, and travelling from North to South you encounter the pairs `I1`/`I2`, then `I3`/`I4`, then `I5`/`I6` in that order).

**Put simply, "arterial" and "connector" are just two names for two different roles, not some complicated terminology:**
- **`R1`, `R2` = arterial = 2 LONG roads.** Each runs end-to-end, connecting the 3 intersections in the same row (`R1` connects `I1-I3-I5`, `R2` connects `I2-I4-I6`). These are the 2 main through-routes, where vehicles travel straight through multiple intersections.
- **`R3`, `R4`, `R5` = connector = 3 SHORT roads.** Each connects exactly 1 pair of intersections (`R3` connects `I1-I2`, `R4` connects `I3-I4`, `R5` connects `I5-I6`). These are also the ONLY 3 roads that cross the railway.

So this classification is not based on "direction" (both types have their own orientation per Diagram 1), but on **role**: long/through-route (arterial) versus short/connecting only 1 pair + crossing the railway (connector).

This naming scheme accounts correctly for the 5 roads and 6 intersections [REQ], turns the Pass-minimum slice (`I1–I2–RC1`) into exactly a 1/3 repeating unit of the full network (satisfying "the complete design should duplicate this part" [REQ]), and clearly separates **arterial** (prioritizing vehicles travelling straight through, a candidate for green-wave coordination) from **connector** (lower baseline traffic, but the roads directly subject to railway pre-emption and congestion risk) — directly answering question F5 in `idea.md` about the difference between traffic priority and coordination sensitivity.

**Driving convention [TEAM, finalized 2026-08-22]:** the project is set in the context of **Vietnam, right-hand traffic (drive on the right)**, per Article 10 of the Law on Road Traffic Order and Safety No. 36/2024/QH15. A concrete example context is the signalized intersection at Mai Chí Thọ–Đồng Văn Cống in the former District 2 area, with the layout documented in H. D. Nguyen, *Capacity Analysis of Signalised Intersections in Motorcycle Dependent Cities*, Figure A-7. This is only a reference for real-world context, not a claim that the project's topology or timing was measured at this site. With right-hand traffic, the median is on the left and the curb/sidewalk is on the right, relative to the direction of travel.

Every road is bidirectional with 2 lanes per direction [REQ]. The Core lane split **[TEAM]**: the lane next to the median = through + left turn (permissive, yielding to opposing traffic), the lane next to the curb = through + right turn (permissive); there are no dedicated turn-arrow signals in the Core design. See `SYSTEM_DIAGRAMS.md`, **Diagram 2**, for the lane/direction layout under the right-hand-traffic convention; see **Section 5b** immediately below for a full analysis and the alternative (dedicated lanes + protected turn-arrow signals) that was carefully considered but not adopted into Core.

Turn-direction ratios and side roads/driveways are explicitly declared **out of scope [ASSUM]**: the brief permits omitting side roads as unimportant, and a project about *signal control logic* does not need a traffic-demand model to be convincing — it is the *events* from sensors (not simulated vehicles) that drive all reactive behavior.

**Distance and speed assumptions [ASSUM, Proposed — Awaiting Confirmation]:** rather than mapping onto one specific named real intersection, the network uses illustrative arterial distances `I1–I3=350 m`, `I3–I5=400 m`, `I2–I4=320 m`, `I4–I6=380 m` and an assumed arterial speed of 60 km/h (16.67 m/s). This speed matches the current maximum for the eligible urban road category under Circular 38/2024/TT-BGTVT, Article 6, Table 1. All values are adjustable parameters used to compute offsets in Section 20, and are not presented as field-survey measurements.

---

## 5b. Analysis: Shared lanes + permissive turns vs. dedicated lanes + protected turn-arrow signals

The team raised a question: if the inner lane allows both through traffic and permissive left turns, and the opposing direction also has a green for through traffic, do the 2 vehicles "collide"? The technical answer: **they do not automatically collide**, but this is indeed a real safety trade-off worth analyzing carefully before deciding — below is the full study requested by the team, examining not only the technical aspect but also operations, timing, and user experience.

### Option A — 2 lanes, permissive turns, 1 circular signal head/approach (current Core design)

**How it works:** during the green phase for one direction (e.g. arterial), BOTH opposing directions get green simultaneously (through + permissive left turn + permissive right turn). Left-turning vehicles must yield to oncoming through traffic themselves, exactly how many small/medium intersections in Vietnam operate when there is no dedicated turn-arrow signal (drivers judge the gap themselves before turning). This is **not a design flaw that causes immediate accidents** — it is a standard, valid operating model used widely around the world; the safety risk depends on driver judgment, not on faulty signal logic.

**Advantages:**
- Much shorter signal cycle: only 2 phases/intersection (arterial vs. connector) → lower average total delay for ALL road users.
- Far fewer actuators/sensors/states → reduces the risk of incorrect implementation within the limited PoC timeframe, freeing up time for the committed HD features (green-wave, congestion, watchdog, multi-node).
- The conflict matrix (Section 22) is simpler and easier to prove correct — important because this project is evaluated on safety correctness, not on the most realistic possible traffic simulation.

**Disadvantage:** the safety risk at left-turn points is higher than Option B, since it depends on driver judgment rather than absolute signal protection.

### Option B — 3 separate lanes (left/through/right), 3 arrow signals, protected left turn

**How it works:** each approach has a dedicated left-turn lane (left turn only), a dedicated through lane, and a dedicated right-turn lane — each lane has its own arrow signal. Left turns are ONLY permitted when the left-turn arrow is green, at which point the opposing direction is red (protected) — completely eliminating the permissive-left conflict point. This is a general protected-turn alternative, and is not presented as a feature measured at the contextual reference intersection.

**Advantages:** completely eliminates collision risk at the left-turn point since it no longer depends on driver judgment; intuitive and easy for drivers to understand.

**Disadvantages (this is the part the team asked to be studied carefully):**
1. **Timing/user experience worsens for everyone.** Protected left turns require a dedicated phase in the cycle (cannot run concurrently with the opposing direction). From 2 phases/intersection (arterial, connector), this rises to a minimum of 4 phases (arterial through+right, arterial dedicated left turn, connector through+right, connector dedicated left turn) — each additional phase also needs its own extra yellow + all-red clearance. Result: a significantly longer cycle, increasing average wait time for **every** road user, including those who never turn left. This is a direct trade-off of "safer for a small group" in exchange for "slower for everyone" — exactly the kind of timing/UX analysis the team wanted considered.
2. **Sharp increase in actuators and state-machine complexity.** From 1 circular signal head/approach to 3 arrow signals/approach × 4 approaches × (5–9 intersections depending on scope) — increasing the number of states, increasing the number of cases that must be proven safe in the conflict matrix (Section 22), and increasing the risk of a bug surviving to the demo.
3. **Requires per-lane sensors** for `OFF_PEAK_SENSOR` to react correctly (e.g. not activating the left-turn phase if no vehicle is waiting to turn left) — this reverses a decision already finalized for clear reasons in Section 23 (Rejected: "Per-lane signals/sensors").
4. **Demo schedule risk.** The team has already committed to a fairly ambitious HD scope (green-wave + congestion management + watchdog + multi-node + railway interlock). Adding another large feature area (protected-turn phasing) increases the risk of being grilled during Q&A on precisely the newly added part, and reduces the time available to solidify the already-committed parts — a genuine project-management risk, not just a technical one.
5. **Requires sufficient storage length** for the left-turn lane so it does not block the through lane behind it — this is a geometry/queueing problem that Section 5 already proactively placed out of scope from the start, since the project is a control system, not a capacity/road-geometry simulation.
6. **"Yellow trap" risk** — a real traffic-engineering issue: if permissive and protected phasing are mixed carelessly (e.g. a permissive left-turn phase ends while the opposing direction still has a longer green remaining), drivers can be tricked into believing it is safe to turn when it is not. Achieving absolute safety requires going **protected-only** (left turns permitted ONLY during the arrow phase, never during the circular green) — which reduces flexibility and can make the intersection "stiffer" when left-turn volume is low.

### Why permissive-left is not a safety gap — and why "accidents" do not apply to this PoC

The key point after discussing thoroughly with the team: **this PoC does not simulate individual vehicles or vehicle-to-vehicle collisions** — the system only simulates sensor events (keypresses) and displays signal states, exactly per the brief ("sufficient to display the state... by text messages"). So the question of "are there accidents" does not literally apply to the demo — no 2 vehicle objects exist to collide.

In operational theory (if actually deployed in the real world), 2 fundamentally different types of conflict must be distinguished:

| Conflict type | Example | Who arbitrates right-of-way? | Safe by definition? |
|---|---|---|---|
| Undetermined (prohibited) | 2 different roads both green at once | No rule at all — the signals contradict themselves | **No** — this is exactly what Safety Invariant #1 (Section 22) must absolutely prevent |
| Already governed by a rule (permitted) | One direction going through + the opposing direction making a permissive left turn | Traffic law: left turns must yield to oncoming through traffic | **Yes** — the standard operating method at most intersections worldwide |

Permissive-left falls into the second category: there is a clear arbitrating rule, it's just that the rule is enforced by humans rather than by the signal — this is a risk that was already codified in law before traffic signals existed, not a gap created by the signal system.

### Recommendation and final decision **[TEAM, finalized]**

**Core retains Option A (2 lanes, permissive).** An official assumption is added to Section 26: the system does not simulate and is not responsible for vehicle-to-vehicle collisions, since no vehicles are simulated — only sensor events and signal states. Option B (3 protected lanes) is recorded as a possible upgrade path to consider **in the future, not now** — no separate diagram is needed for it at this stage.

If the team later wants to revisit Option B (for example when extending beyond the course scope), the sensible step would be to first upgrade the 1–2 intersections closest to the railway (`I1`/`I2`), since that is where left turns interact directly with railway pre-emption and therefore carry the highest safety value — see Section 23, Optional/Stretch.

---

## 6. System Components

**Physical infrastructure:** 5 roads, 6 signalized intersections, 3 railway crossings (double track, 1 line per direction), 24 vehicle signal heads (4 approaches × 6 intersections), 24 pedestrian crosswalks with 24 push buttons + 24 pedestrian signals, 6 boom gates (2 per crossing), 6 sets of road-facing flashing lights (2 per crossing), 6 train signals (2 per crossing, 1 per rail direction).

**Logical controllers:** `L1–L6` (intersection local controllers, each corresponding to 1 physical intersection `I1–I6`), `RL1–RL3` (railway local controllers, each corresponding to 1 physical crossing `RC1–RC3`), `C1` (Central Controller). 10 logical controllers in total [CLARIF].

**Software/QNX architecture:** each logical controller is 1 or more QNX **processes**; controllers are distributed across multiple QNX **nodes**; nodes are distributed across a small number of physical/virtual machines during the demo (Section 18). A 1:1:1 mapping (controller = node = machine) is explicitly **not** assumed [CLARIF].

**Demo environment:** terminal-based state display for each controller/console [REQ — no GUI required], keyboard-based sensor/operator input simulation [REQ], optional test-vector logging via `/fs` [REQ].

---

## 7. Controller Architecture

```text
                                CENTRAL CONTROLLER (C1)
                          monitor · reconfigure · override
                        (never actuates physical devices)
                    ________________|________________
                   |                                  |
        INTERSECTION CONTROLLERS               RAILWAY CROSSING CONTROLLERS
             L1 L2 L3 L4 L5 L6                       RL1  RL2  RL3
              |  |  |  |  |  |                         |    |    |
             I1 I2 I3 I4 I5 I6                       RC1  RC2  RC3
      (physical intersections)                (physical railway crossings)

     Devices owned by each Lx:                 Devices owned by each RLx:
       - Vehicle signal heads                    - Boom gate
       - Pedestrian signals                       - Road-facing flashing lights
       - Vehicle detection sensors                - Train signal
       - Pedestrian push buttons                   - Train approach/exit sensors
                   ^                                       |
                   |________ railway state (sense-only) ___|
```

Core architectural rule [CLARIF]: the arrow from `RLx` to `Lx` carries only **state** (crossing state), never commands, and flows in only 1 direction. `Lx` cannot query or act upon railway devices; it can only *know* the current state of the crossing and react on its own side of the interlock.

---

## 8. Responsibilities of Each Controller

### 8.1 Central Controller (`C1`)

- **Input:** status reports from all 9 local controllers (signal states, pedestrian queues, sensor states, mode, faults), heartbeat.
- **Output/commands:** to `Lx`: `SET_MODE(PEAK_FIXED | OFF_PEAK_SENSOR)`, `SET_TIMING_PROFILE(parameters within validated limits)`, `REQUEST_OVERRIDE(type, target, duration)`; to `RLx`: only validated high-level requests (e.g. acknowledging/clearing a previously reported fault after physical repair is complete — see the reconciliation note below).
- **Monitoring responsibilities:** aggregate and display network-wide signal/pedestrian/railway state, maintain a fault log, detect unresponsive/disconnected controllers via heartbeat timeout.
- **Explicit limitations [REQ][CLARIF]:** cannot set a signal to a specific color; cannot directly actuate any railway device (boom gate, flashing lights, train signal); cannot force through a command that overrides a safety invariant a local controller has rejected; cannot prematurely end a latched pedestrian clearance.
- **Reconciling the brief's wording on railways [REQ][CLARIF]:** the brief states the operator "will occasionally send commands to the intersections, boom gate control and train approach signals system", while also making clear that Central never directly controls the signals at an intersection, and the lecture clarification states that only `RLx` may actuate railway devices. These 2 statements are **not contradictory** when read correctly: `C1`'s "command to boom gate control" means a high-level, validated command sent to `RLx` — the controller that owns that device — consistent with the same command abstraction used for `Lx` (never a raw actuation command sent straight down to hardware). `RLx` validates it itself and decides whether to execute it, retaining the right to refuse/NACK (Section 18).

### 8.2 Intersection Local Controller (`Lx`, controlling intersection `Ix`)

- **Devices controlled:** 4 vehicle signal heads, 4 pedestrian signals (at `Ix`).
- **Local sensors:** 4 per-approach vehicle detection sensors, 4 pedestrian push buttons, and (HD, only at the 2 intersections facing the crossing for each railway crossing) 1 advance/queue sensor.
- **Traffic logic:** runs 1 of 2 modes `PEAK_FIXED` / `OFF_PEAK_SENSOR`, enforces the 2-phase conflict matrix, clearance intervals, min/max green.
- **Pedestrian logic:** latches push-button presses, serves WALK during a compatible vehicle phase, runs the full WALK → FLASHING-DON'T-WALK → DON'T WALK sequence.
- **Railway information consumed:** crossing state (`OPEN/WARNING/CLOSING/CLOSED/TRAIN_PRESENT/OPENING/FAULT`) for the geographically relevant crossing (only the 2 controllers located directly on the connector road that crosses that railway line receive it — e.g. only `L1`/`L2` receive `RC1`'s state).
- **Fallback behavior:** continues running the full local logic (including sensor-driven) entirely without needing a link to Central; falls back to a self-selected mode based on time of day.
- **Interaction with Central:** receives `SET_MODE`/`SET_TIMING_PROFILE`/override, applying them only at the next safe phase boundary, rejecting and NACK-ing any command that violates a safety invariant.

### 8.3 Railway Local Controller (`RLx`, controlling crossing `RCx`)

- **Train detection:** 2 approach sensors (1 per rail direction). See Section 9 regarding the status of the exit sensor.
- **Boom gate control:** exclusive ownership of both gate arms of the crossing.
- **Flashing-light control:** exclusive ownership of both sets of road-facing flashing lights.
- **Train signal:** exclusive ownership of both train signals (1 per direction); enforces the gate-confirmed-closed-before-proceed invariant.
- **Crossing state machine:** `OPEN → WARNING → CLOSING → CLOSED → (TRAIN_PRESENT) → OPENING → OPEN`, with the `FAULT` state reachable from any point.
- **Fault handling:** fail-to-safe on any ambiguous sensor/gate state (Section 17).
- **Information shared with road controllers:** current crossing state, broadcast only to the 2 adjacent `Lx` on its connector road.
- **Reporting to Central:** every state transition and every fault, immediately.

All 3 `RLx` run *identical* logic — the brief gives no reason for them to differ, and inventing different behavior between crossings would only increase the test surface without any pedagogical value **[TEAM — declined to differentiate]**.

---

## 9. Sensors

| # | Sensor | Location | Owner | Event | Purpose / Simulation |
|---|---|---|---|---|---|
| 1 | Per-approach vehicle detection sensor | At the stop line, 1 per approach (4 per intersection) | `Lx` | `DEMAND_PRESENT` / `DEMAND_ABSENT` | Equivalent to a standard inductive loop: "is there currently a vehicle waiting at this approach?" One keypress per approach. |
| 2 | Advance/queue sensor **[HD]** | About 60–80 m upstream of the stop line, with 1 sensor on each intersection approach facing a railway crossing | `Lx` | `QUEUE_WARNING` | Picture 2 marks on one approach: mark A right at the stop line (sensor #1), mark B about 70 m upstream of it (sensor #2). Sensor #1 answers "is a vehicle waiting?"; sensor #2 answers "has the queue grown back to mark B?". Sensor #2 only serves the congestion-mitigation logic in Section 16, and is not used to extend a normal phase. This is a binary sensor, not a vehicle counter. Its own dedicated keypress. |
| 3 | Pedestrian push button | 1 per crosswalk side (4 per intersection) | `Lx` | `PED_REQUEST(side)` | Direct requirement from [REQ]. Keypress. |
| 4 | Train approach sensor | Fixed distance ahead of the crossing, 1 per rail direction (2 per crossing) | `RLx` | `TRAIN_APPROACHING(direction)` | Direct requirement from [REQ] — this is the event that starts the entire railway safety sequence (Section 14). Keypress. |
| 5 | Train exit/clearance-confirmation sensor (exit/clearance sensor) — **reserved, not used in Core logic [TEAM, revised]** | Immediately after the crossing, 1 per direction (2 per crossing) | `RLx` | *(not wired into the Core gate-opening decision)* | Originally intended to confirm the exact moment a train has cleared the crossing. **Changed:** the Core design now assumes every train takes the same fixed, known-in-advance amount of time to fully clear the crossing from the moment of arrival (Section 14), so re-opening the gate uses a timer rather than this sensor. The sensor is still kept in the physical device inventory as a reserved slot for a **future extension** — e.g. cross-checking whether the train actually cleared the crossing within the assumed time window, to catch cases of an abnormally slow or stuck train — but that check is explicitly outside the scope of the current Core safety logic. |
| 6 | Boom gate position sensor | On each gate arm (2 per crossing) | `RLx` | `GATE_CLOSED_CONFIRMED` / `GATE_OPEN_CONFIRMED` / `GATE_FAULT` | This sensor **does not care how long closing/opening takes** — it answers exactly 1 question at any given moment: "right now, is the gate closed or open?" This is the second verification layer in the design, entirely separate from the timing layer (Section 14 assumes the gate takes ~10 seconds to close — that figure is only used to *schedule in advance*). Hard rule: even after waiting the full scheduled 10 seconds, the system **never** infers "it's probably closed by now" on its own — it always waits for sensor #6 to confirm the real state; if the deadline passes and sensor #6 has still not reported `GATE_CLOSED_CONFIRMED`, that is a fault (Invariant #6, Section 22), not "closed a bit late, close enough." Keypress to simulate a fault. |

Only these 6 sensor types are included in the Core design. Vehicle counting, ANPR, weather sensors, per-lane sensors (see Section 5b), etc. were considered and excluded from Core — see Section 23.

**In summary, there are 2 layers throughout the railway safety sequence (Section 14):**
1. **Timing layer (planning):** assumed figures such as "gate takes ~10 seconds to close," "total lead time ~45 seconds" — used only to *schedule in advance*, never to make a safety decision.
2. **Verification layer (reality):** sensor #6 confirms the actual gate state at the exact moment a decision is needed — this is what actually decides whether the `train signal` may go to `PROCEED`.
Separating these 2 layers is deliberate, so that it is never the case that "scheduled time has elapsed → assume safe" without re-checking reality.

---

## 10. Actuators and Signals

| # | Actuator | Owner | States | Safe state |
|---|---|---|---|---|
| 1 | Vehicle signal head (per approach) | `Lx` | `RED / YELLOW / GREEN / FLASHING_RED` | `FLASHING_RED` (flashing on all directions, per the real-world power-outage convention for traffic signals — forcing every approach to yield) |
| 2 | Pedestrian signal (per crosswalk side) | `Lx` | `WALK / FLASHING_DONT_WALK / DONT_WALK` | `DONT_WALK` |
| 3 | Boom gate (per direction) | `RLx` | `UP / MOVING / DOWN / FAULT` | `DOWN` (any ambiguity is always resolved toward blocking the road, never toward allowing a train through unconfirmed) |
| 4 | Railway flashing light (per road approach direction) | `RLx` | `ON / OFF` | `ON` |
| 5 | Train signal (per rail direction) | `RLx` | `STOP / PROCEED` | `STOP` |

There is no dedicated turn-arrow actuator in the Core design (Section 5b, Section 23). The 3-arrow-signals/approach option (separate left/through/right lanes) is documented as a design choice that was considered — see Section 5b — but only as a future upgrade path, not a Core actuator.

---

[← Table of Contents](README.md) · [← Vision and Scope](01-vision-and-scope.md) · Next: [Operations and Behavior →](03-operation-and-behavior.md)
