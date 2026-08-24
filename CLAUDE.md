# Task: Create HD-Level Mermaid State Charts for the Distributed Traffic-Control System

I am preparing Section 4.1, **Intersection State Charts**, for the EEET2588/EEET2687 Real-Time Systems Engineering Initial Design Report.

The report requirement is:

> “Provide State Charts describing the lights’ behavior at each intersection. You should include relevant State Chart(s) alongside your use cases.”

Create a concise but technically rigorous set of Mermaid state charts based on the finalised project repository:

https://github.com/lucietran03/traffic-control-system

## 1. Read the authoritative project sources

Before producing any diagram, read the latest versions of:

1. `PROJECT_SPECIFICATION.md`
2. `system_assumptions_tables.md`
3. `lecture_clarification.md`
4. `SYSTEM_DIAGRAMS.md`
5. `scenarios_and_written_specifications.md`, if present

Use this precedence if any wording conflicts:

`PROJECT_SPECIFICATION.md`
→ `system_assumptions_tables.md`
→ `lecture_clarification.md`
→ other files

Do not use README or working notes as authority when they conflict with these sources.

Do not invent unresolved states, timings, signals, sensors, commands, or controller authority.

## 2. System boundary and controller responsibilities

Preserve these architecture rules:

- `I1–I6` are physical intersections.
- `L1–L6` are their intersection controllers.
- `RC1–RC3` are physical railway crossings.
- `RL1–RL3` are their railway controllers.
- `C1` is the Central Controller.
- Each `Lx` exclusively controls its own vehicle and pedestrian signals.
- Each `RLx` exclusively controls its own gates, road-facing flashers, and train signals.
- `C1` may monitor and send high-level requests but must never directly actuate physical signals.
- Railway status flows from `RLx` to its two adjacent `Lx` controllers as read-only status.
- `DEGRADED_LOCAL` is a connectivity/availability condition, not a hazard mode and not a replacement for `PEAK_FIXED` or `OFF_PEAK_SENSOR`.

## 3. Required diagrams

Produce the following five diagrams.

### SC-01 — Generic Intersection Vehicle-Signal Phase Sequencer

This diagram applies to all six intersection controllers `L1–L6`.

Show the observable vehicle-light states and transitions for the two-phase intersection:

- `ARTERIAL_GREEN`
- `ARTERIAL_YELLOW`
- `ALL_RED_A_TO_B`
- `CONNECTOR_GREEN`
- `CONNECTOR_YELLOW`
- `ALL_RED_B_TO_A`

Represent:

- `PEAK_FIXED`:
  - arterial green = 48 seconds;
  - yellow = 4 seconds;
  - all-red = 2 seconds;
  - connector green = 30 seconds;
  - yellow = 4 seconds;
  - all-red = 2 seconds;
  - total cycle = 90 seconds;
  - ordinary vehicle sensors do not alter these durations.

- `OFF_PEAK_SENSOR`:
  - minimum green = 8 seconds;
  - green extension = 4-second increments;
  - maximum green = 40 seconds;
  - when no demand is present, the intersection rests on the arterial phase;
  - vehicle and latched pedestrian demand are scheduled equally;
  - a pending connector demand cannot be indefinitely starved;
  - after no more than one opposing arterial service opportunity, the connector request must be served, subject to clearance.

Show transitions using:

`event [guard] / observable action`

Do not use source code or QNX API calls.

### SC-02 — Generic Pedestrian-Signal State Chart

This diagram applies to every pedestrian crossing side at `I1–I6`.

Show:

- `DONT_WALK`
- `REQUEST_LATCHED`
- `WALK`
- `FLASHING_DONT_WALK`
- return to `DONT_WALK`

Represent:

- one button press creates one pending request;
- repeated presses are coalesced;
- WALK begins only with a compatible vehicle phase;
- active WALK and pedestrian clearance cannot be truncated;
- a request received during railway pre-emption remains latched;
- a stuck-active button creates only one request and raises a fault;
- a permanently silent button cannot be detected by the Core design.

Do not model pedestrian crossings at `RC1–RC3`, because they are outside the selected scope.

### SC-03 — Intersection Supervisory and Safety Overlay State Chart

Show the high-level authority affecting an `Lx` without duplicating the detailed phase sequence from SC-01.

Include:

- `NORMAL_OPERATION`
  - containing `PEAK_FIXED` and `OFF_PEAK_SENSOR`;
- `CENTRAL_OVERRIDE`;
- `RAILWAY_PREEMPTION`;
- `FAULT_SAFE`.

Represent the final priority:

1. `FAULT_SAFE` — absolute local safety veto;
2. `RAILWAY_PREEMPTION`;
3. `CENTRAL_OVERRIDE`;
4. normal `PEAK_FIXED` / `OFF_PEAK_SENSOR`.

Show:

- normal mode changes apply only at a safe phase boundary;
- Central override is bounded and auto-expiring;
- override is rejected with `NACK` if unsafe;
- an override cannot truncate pedestrian clearance;
- an override conflicting with railway pre-emption is rejected;
- railway pre-emption suppresses only the movement toward the affected crossing;
- the movement passes through ordinary yellow and all-red clearance before reaching red;
- compatible cross-traffic, away-from-crossing movements, and pedestrian phases may continue;
- `FAULT_SAFE` produces all-way `FLASHING_RED` for vehicle signals and `DONT_WALK` for pedestrian signals;
- fault clearance is explicit, never automatic.

Do not show `DEGRADED_LOCAL` as a higher or lower hazard mode in this diagram. It belongs in SC-05 because it is orthogonal to the light-control mode hierarchy.

### SC-04 — Generic Railway-Crossing Lifecycle

This diagram applies identically to `RL1–RL3`.

Use these main states:

- `OPEN`
- `WARNING`
- `CLOSING`
- `CLOSED`
- `TRAIN_PRESENT`
- `OPENING`
- `FAULT`

Show:

- train approach detection begins the warning sequence;
- flashers activate immediately in `WARNING`;
- adjacent intersections receive crossing status;
- gates begin closing after the flash-only warning interval;
- train signal remains `STOP` until gate-position sensors confirm the required gates are `CLOSED`;
- train signal changes to `PROCEED` only after confirmed closure;
- the approximately 45-second value is a warning-to-arrival budget, not one single state duration;
- a 20-second occupancy window is registered for each simulated train;
- a second train adds another active occupancy window;
- gates remain closed until every active occupancy window has elapsed;
- the train signal returns to `STOP` before reopening;
- after reopening, flashers stop and `OPEN` is broadcast;
- missing or contradictory gate confirmation transitions to `FAULT`;
- a stuck-active train-approach input transitions to a safe-side fault;
- `FAULT` keeps the train signal at `STOP` and applies the documented safe outputs;
- fault clearing requires verified repair and an accepted local fault-clear request;
- a permanently silent train-approach sensor is an accepted Core limitation and must be shown only as a note, not as a detectable transition.

Do not use the reserved train-exit sensor to control reopening. Core reopening is timer-based.

### SC-05 — Central Connectivity and Local Autonomy

Show the connectivity states:

- `CENTRAL_CONNECTED`
- `DEGRADED_LOCAL`
- `RESYNCHRONISING`

Represent:

- heartbeat every 1 second;
- three consecutive missed heartbeats enter `DEGRADED_LOCAL`;
- the controller retains its last validated timing parameters;
- the local clock continues selecting `PEAK_FIXED` or `OFF_PEAK_SENSOR`;
- traffic, pedestrian, railway, and fault logic continue locally;
- Central coordination and override availability are disabled while disconnected;
- restored communication enters `RESYNCHRONISING`;
- the local controller sends its complete current state to `C1`;
- new Central requests are not accepted until that state exchange completes;
- successful state exchange returns to `CENTRAL_CONNECTED`.

Make it clear that connectivity loss alone does not force all-red, `FLASHING_RED`, or frozen operation.

## 4. Relationship to the use cases

After the diagrams, provide a short mapping using bullets, not a large table:

- SC-01 → UC-01 and UC-07
- SC-02 → UC-02 and UC-08
- SC-03 → UC-05, UC-06, UC-07, and UC-08
- SC-04 → UC-04 and UC-06
- SC-05 → UC-09 and UC-10

Use the final renumbered use-case names from the repository. If the repository contains different current numbering, report the discrepancy before drawing and use the repository’s latest approved numbering.

## 5. Mermaid requirements

Use only Mermaid `stateDiagram-v2`.

For every diagram:

- provide one independent fenced `mermaid` code block;
- include a YAML title;
- include `accTitle` and `accDescr`;
- use stable ASCII state IDs with readable displayed labels;
- use `[*]` for initial and terminal pseudostates where appropriate;
- use `direction LR` or `direction TB` according to whichever is clearer;
- use composite states only where they materially improve readability;
- use `<<choice>>` only for genuine guarded branching;
- use notes for assumptions and accepted limitations;
- label every important transition with its event, guard, or timeout;
- keep text concise enough to render legibly in an engineering report;
- avoid decorative styling unless it clearly distinguishes normal, warning, and fault states;
- do not rely on unsupported transitions between internal states belonging to different composite states;
- ensure every code block parses independently in the current Mermaid Live Editor.

Do not generate flowcharts or sequence diagrams as substitutes for state charts.

## 6. Quality checks before answering

Before finalising:

1. Check every state and transition against the repository.
2. Confirm that all mandatory yellow and all-red clearances are preserved.
3. Confirm that Central never directly actuates any light, gate, flasher, or train signal.
4. Confirm that railway safety remains local when Central is unavailable.
5. Confirm that no gate opens while any occupancy window remains active.
6. Confirm that no train signal shows `PROCEED` without sensor-confirmed gate closure.
7. Confirm that an in-progress pedestrian clearance cannot be truncated.
8. Confirm that `DEGRADED_LOCAL` is not presented as a hazard-priority mode.
9. Confirm that the queue detector is not represented as a vehicle counter.
10. Confirm that every diagram is readable when rendered on an A4 report page.

## 7. Required response format

Respond in this order:

1. **Coverage decision** — briefly justify why these diagrams are sufficient and whether one generic `Lx` chart can validly represent `L1–L6`.
2. **Assumptions or contradictions found** — list only genuine unresolved issues. Do not invent a resolution.
3. **SC-01 Mermaid code**
4. **SC-02 Mermaid code**
5. **SC-03 Mermaid code**
6. **SC-04 Mermaid code**
7. **SC-05 Mermaid code**
8. **Use-case-to-state-chart mapping**
9. **Final consistency check**

Do not modify repository files. Return the diagrams and analysis directly in the response.
