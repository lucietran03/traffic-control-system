[← Table of Contents](README.md) · Chapter 3/6 · [← Network and Architecture](02-network-and-architecture.md) · Next: [Safety and Real-Time →](04-safety-and-realtime.md)

---

## 11. Operating Modes

**Simple explanation first, detailed table below:**
- `PEAK_FIXED` = peak hours — lights run on a fixed schedule, without waiting for vehicles.
- `OFF_PEAK_SENSOR` = off-peak hours — lights wait for a vehicle/pedestrian before turning green.
- `RAILWAY_PREEMPTION` = a train is approaching — the direction of travel toward the railway must turn red earlier than normal, regardless of which of the two modes above is currently active.
- `CENTRAL_OVERRIDE` = the operator at the central office issues a special command (e.g., clearing the way for a priority convoy).
- `DEGRADED_LOCAL` = communication with the central office is lost — the lights continue to run safely on their own, using the most recently known configuration.
- `FAULT_SAFE` = a fault is detected — the lights switch to the safest state (flashing red) and stop responding normally.

| Mode | Purpose | Entry Condition | Behavior | Permitted Central Commands | Exit Condition | Priority |
|---|---|---|---|---|---|---|
| `PEAK_FIXED` | Predictable timing, easy to coordinate under high demand | By time of day or `SET_MODE` | Fixed 2-phase cycle, split according to arterial priority; ordinary vehicle-detection sensors are monitored only for status/fault purposes and are not used for extension; the advance/queue sensor (Section 9, item 2) still monitors congestion independently, regardless of mode | `SET_TIMING_PROFILE`, `SET_MODE`, override | `SET_MODE` or a time-of-day change | Normal (7) |
| `OFF_PEAK_SENSOR` | Demand-responsive operation when traffic volume is low | By time of day or `SET_MODE` | Selects/extends phases based on vehicle-detection sensors plus latched pedestrian requests, subject to min/max green limits | `SET_TIMING_PROFILE`, `SET_MODE`, override | `SET_MODE` or a time-of-day change | Normal (7) |
| `RAILWAY_PREEMPTION` | Protect the railway crossing | `RLx` reports `WARNING`/`CLOSING`/`CLOSED` for the adjacent crossing | Overlays the current mode of `Lx`: forces the direction heading toward the crossing to red via a safe clearance sequence, temporarily suspends normal phase selection for that approach, and prioritizes "drain" directions (clearing the queued backlog) after reopening | No command is accepted if it would turn the direction toward the crossing green again | Crossing reports `OPEN` and the drain phase is complete | 2 |
| `CENTRAL_OVERRIDE` | Special situations (e.g., clearing the way for a VIP convoy) [REQ] | `REQUEST_OVERRIDE` is accepted | Forces a specified direction to green for a limited duration | (it is itself the command) | Duration expires or it is explicitly canceled | 4 |
| `DEGRADED_LOCAL` | Continue operating safely without the central office | Heartbeat from `C1` times out | Retains the most recently known valid **timing parameters**; the **mode (Peak/Off-Peak) is not frozen** — it continues to be selected automatically by the local clock exactly as when `C1` is present | None (the link is down) | Connection is restored + the local unit pushes its entire current state up to `C1` | Parallel (not part of the hazard hierarchy — this is a readiness state, not a hazard) |
| `FAULT_SAFE` | Prevent a detected hazard | Local hardware conflict, watchdog trip, or (for `RLx`) an unconfirmed gate state when there is train risk | Forces outputs to the safe state (Section 10), stops responding to commands unrelated to safety | No command is accepted except a fault-clear command (which may require operator action) | Explicit clearing, never automatic | 1 (highest) |

---

## 12. Normal Traffic Behavior

Each intersection is modeled as a 4-way, 2-phase standard intersection (arterial vs. connector), without per-lane phasing (the Core's 2-lane permissive model — see Section 5b if the team wants to consider a separate turn-lane phasing option):

- **Phase A** — arterial (`R1`/`R2`) through+left green, connector red.
- Yellow (4 seconds **[ASSUM]**) → All-red clearance (2 seconds **[ASSUM]**).
- **Phase B** — connector (`R3`/`R4`/`R5`) through+left green, arterial red; compatible pedestrian WALK runs concurrently.
- Yellow → All-red clearance → repeat.

`PEAK_FIXED`: total cycle is **exactly 90 seconds [ASSUM]**, split as follows: Phase A (arterial) = 48 seconds green + 4 seconds yellow + 2 seconds all-red = 54 seconds; Phase B (connector) = 30 seconds green + 4 seconds yellow + 2 seconds all-red = 36 seconds; 54 + 36 = 90 seconds, consistent with the higher priority assigned to the arterial (green ratio 48:30 ≈ 3:2, Section 5). Sensors are still monitored but do not alter timing in this mode, preserving the determinism that coordination/green-wave requires.

**Why 90 seconds, with no need to divide evenly into the train headway (2 minutes = 120 seconds):** unlike some earlier designs that tied the light cycle to train headway, the 90-second cycle here was **not** chosen to divide evenly into 120 seconds, because `RAILWAY_PREEMPTION` (Sections 14, 15) is event-driven — it interrupts the currently running phase immediately (via the normal safe clearance sequence) regardless of where it is in the cycle, without waiting for a cycle boundary. This approach is more robust than assuming trains always arrive at exact multiples of the light cycle, since real trains in practice do not run on that kind of schedule.

`OFF_PEAK_SENSOR`: a phase only receives green when its approach(es) report demand or are holding a latched pedestrian request; green is extended in 4-second **[ASSUM]** steps up to a 40-second **[ASSUM]** cap while demand persists; when there is no demand anywhere, the intersection rests on the arterial phase (defaulting to main-road priority) [REQ — "differ between R1-R5, teams must state and justify"]. A secondary road never has to wait longer than one maximum extension of the opposing phase plus its own min green — this is a structural anti-starvation limit, not a weighted algorithm.

Mode changes (`PEAK_FIXED ↔ OFF_PEAK_SENSOR`, whether triggered by the local clock or commanded by the central office) are only applied at the boundary of the next phase, never in the middle of a phase **[TEAM]** — this preserves all clearance/min-green invariants regardless of when the mode-change request arrives.

---

## 13. Pedestrian Behavior

- 4 crosswalks per intersection (North/East/South/West), 1 push-button per side [REQ].
- A single button press **latches** into one pending request for that side; repeated presses while already latched have no additional effect (pressing 10 times = 1 request) **[TEAM]**.
- Pedestrian WALK runs **concurrently** with the vehicle phase that does not conflict with that crosswalk (the parallel-walk style common at signalized intersections in Vietnam), not a separate all-red "scramble" phase — a scramble phase was considered and rejected because it adds complexity with no justification (Section 23).
- Signal sequence: `WALK → FLASHING_DONT_WALK (clearance) → DONT_WALK` **[TEAM]** — matching the common 3-state pedestrian signal model, not a simple 2-state signal.
- Served in both `PEAK_FIXED` (guaranteed every cycle, since both phases run every cycle) and `OFF_PEAK_SENSOR` (a latched request is scheduled just like vehicle demand).
- Once WALK has begun, it must run through its full clearance cycle before that crosswalk can be interrupted by anything, including an override from the central office (Safety Invariant, Section 22).
- A pedestrian request arriving while railway pre-emption is in progress is **latched, not dropped**. Because pre-emption only forces the *direction heading toward the crossing* to red (Section 15), and pedestrian crosswalks exist only at intersections, not at railway crossings **[TEAM — scope note: pedestrian crosswalks are out of scope at `RCx`, since the assignment only places them "at each intersection"]**, this request is typically still served normally during a non-conflicting phase.
- A push-button input stuck active is collapsed into a single pending request and flagged as a fault. A button that is permanently silent cannot be detected without an independent health signal, since it cannot be distinguished from the "not pressed" state **[ASSUM]**.

---

## 14. Railway Behavior

The entire normal life cycle, owned entirely by `RLx` [CLARIF]:

1. **Train detection** — the approach sensor triggers → `RLx` immediately turns on the flashing lights and immediately notifies the 2 adjacent `Lx` and `C1` (state → `WARNING`).
2. **Traffic clearing begins** — the adjacent `Lx` begins safely transitioning to red for the direction heading toward the crossing (completing any clearance already in progress; no abrupt green cutoff — Section 22).
3. **Gate begins closing** — after the initial flashing-only warning interval, `RLx` commands both gate arms to lower (state → `CLOSING`).
4. **Gate confirms closed** — position sensors confirm both arms are lowered (state → `CLOSED`).
5. **Setting the train signal** — `PROCEED` is issued **only when** the gate has confirmed `CLOSED`; otherwise `STOP` is held and a fault is reported (Safety Invariant).
6. **Occupancy window begins** — as soon as the approach sensor triggers, `RLx` also computes a fixed **crossing occupancy window** for that train: `estimated arrival time + a fixed duration to fully clear the crossing`. This is a timer-based substitute for detection via an exit sensor (Section 9, item 5) — the **[ASSUM]** assumption is that every train takes the same known amount of time to fully clear the crossing after arriving, so a timer is sufficient and no exit sensor is needed for this decision.
7. **Train passing** — the state is `TRAIN_PRESENT` for as long as *any* occupancy window (step 6) for any direction has not yet expired.
8. **Gate reopens** — only when **every** active occupancy window (both directions, including a second train arriving shortly after) has expired does `RLx` raise the gate (state → `OPENING → OPEN`), turn off the flashing lights, and broadcast `OPEN`. If a second train is detected before the first train's window expires, the gate simply stays as it is — its occupancy window is added to the set of windows that must all expire before reopening (naturally handling the case of two trains arriving close together, same direction or opposite directions, without any special dedicated logic).
9. **Traffic returns to normal** — the adjacent `Lx` resumes the most recent prior phase (not a full reset) and runs an extended drain phase on the connector road to release the vehicle queue that built up while closed (Section 16), before returning to the normal cycle.

**Timing calculation method [ASSUM, Proposed — Awaiting Confirmation]:** the assignment does not provide figures for lead time, so the values below are *derived*, not fabricated:
- **Lead time from warning to train arrival** ≈ flashing-only warning (5 seconds) + gate-closing time (10 seconds) + safety margin for closed confirmation (5 seconds) + additional margin for the adjacent intersection's own safe clearance sequence (~25 seconds) ⇒ target of **~45 seconds from train detection to the train reaching the crossing**. The approach sensor's placement distance is derived from `assumed maximum train speed × 45 seconds` (e.g., 80 km/h ≈ 22 m/s ⇒ ≈1000 m).
- **Crossing occupancy duration** (step 6 above) ≈ an assumed fixed value, e.g., **20 seconds**, representing train length + crossing width at the assumed train speed, the same for every simulated train.

Both values are presented clearly in the report as derived, adjustable parameters, not fabricated facts.

**Accepted trade-off:** using a fixed timer instead of an exit-confirmation sensor means the current design does not detect the edge case of a train running abnormally slowly or getting stuck on the crossing. This is a deliberate simplification, clearly documented for the Core design — not an oversight — and the retained exit sensor (Section 9, item 5) is precisely the identified direction for closing this gap later if time permits. This trade-off has been reviewed a second time, and the team **has deliberately chosen to keep it as is** (not escalating it to Core/HD) in order to protect the schedule for scope already committed elsewhere (Section 23). The term "fail-safe" used throughout this document refers specifically to the gate-confirmed-closed-before-`PROCEED` interlock (Invariant #5/#6) and the fail-to-red behavior on all other described faults — it is **not** a claim that every imaginable railway failure scenario, including a train stuck beyond the assumed occupancy window, has been covered.

---

## 15. Railway Pre-emption and Traffic Clearing

**Simple description:** "pre-emption" here works similarly to how an emergency vehicle is given priority at an intersection — the only difference is that what is being protected is the railway crossing, not a vehicle. As soon as `RLx` detects an approaching train, it does not wait for the adjacent intersection's normal cycle to reach the connector phase's turn; it immediately notifies the adjacent `Lx` that "a train is approaching," and each `Lx` reprioritizes so that the specific direction leading to the crossing loses its green (via the normal safe clearance sequence, not abruptly) well before the gate needs to lower.

Pre-emption is **directional, not intersection-wide [TEAM]**: only the direction(s) leading toward a closing/closed crossing are forced red; cross traffic and pedestrian phases that do not conflict with that direction continue to be served normally by the same `Lx`. For example, at `I1` (adjacent to `RC1` via `R3`), only the `R3` direction heading toward the crossing is affected — the `R1` arterial phase and any crosswalk not conflicting with the `R3` direction continue to operate on their normal schedule. This approach is both more realistic and safer to implement than freezing the entire intersection, since it avoids inventing new conflict cases beyond the intersection's normal conflict matrix.

The transition to red for the direction heading toward the crossing is always completed via the normal yellow + all-red clearance sequence (Safety Invariant) — pre-emption changes *scheduling priority*, never *skips* the clearance sequence, since doing so would itself create a hazard right at the intersection.

---

## 16. Managing Railway-Induced Congestion

According to the instructor's explanation, the goal is to stop *pushing* traffic toward a closed crossing, rather than inventing a traffic-rerouting capability that the light system cannot actually provide. Specifically, the traffic light system can do exactly 3 things, no more — this specification states that clearly rather than implying the system "solves" congestion:

1. **Stop feeding demand toward the crossing.** As soon as pre-emption begins, the direction heading toward the crossing stops receiving green until the crossing reopens (Section 15) — this is the primary lever.
2. **Prioritize directions leading away from the crossing and cross traffic** at the adjacent intersection while it is closed, so vehicles already queued between the intersection and the crossing have a chance to turn off elsewhere instead of accumulating — only where the existing lane/turn geometry allows it; the specification does not claim the system can create a new physical route.
3. **Run a bounded drain phase after reopening** — extending green on the connector road in the same 4-second steps as normal `OFF_PEAK_SENSOR`, **as long as the advance/queue sensor still reports `QUEUE_WARNING`**, up to a separate **60-second** cap (higher than the normal 40-second cap, applying only to this recovery phase). The drain phase stops as soon as `QUEUE_WARNING` clears **or** the 60-second cap is reached, whichever comes first. This rule is fully determinable from a single binary sensor (no need to know how many vehicles/meters long the queue is) — if an examiner asks "how do you compute drain duration from a Boolean sensor," this is the concrete answer.

The advance/queue sensor (Section 9, item 2) exists specifically to detect when the vehicle queue on the approach heading toward the crossing is growing back toward the crossing, feeding both the demand-cutoff logic during pre-emption and the drain-phase sizing calculation during recovery.

---

[← Table of Contents](README.md) · [← Network and Architecture](02-network-and-architecture.md) · Next: [Safety and Real-Time →](04-safety-and-realtime.md)
