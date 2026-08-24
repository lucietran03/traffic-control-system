# Task: Create HD-Level Mermaid Behavioural Sequence Diagrams for the Distributed Traffic-Control System

I am preparing Section 4.2, **Behavioural Specifications**, for the EEET2588/EEET2687 Real-Time Systems Engineering Initial Design Report.

The report requirement is:

> “Include behavioral specifications such as sequence diagram(s) or collaboration diagrams to map out the chronological flow of actions.”

Create a concise but technically rigorous set of Mermaid sequence diagrams for the project repository:

https://github.com/lucietran03/traffic-control-system

Do not modify any repository files. Return the completed diagrams directly in the response.

## 1. Read the current project sources

Fetch and read the latest repository version before drawing.

Use:

1. `RTS_Final Project.pdf`
   - This is the official assignment brief and the source of client requirements.

2. `system_assumptions_tables.md`
   - This contains the finalised design decisions, numeric parameters, safety constraints, and traceability IDs.

3. `scenarios_and_written_specifications.md`
   - Use the latest approved ten-use-case version if it is available.
   - If the repository version still contains only six use cases, state that it is outdated and use the ten-use-case baseline specified below.

4. `PROJECT_SPECIFICATION.md`
   - This is an internal consolidated design document.
   - Use it to check architectural and behavioural consistency, but do not present it as an official external requirement source.

5. `lecture_clarification.md`
   - Use it only to interpret the brief.
   - Any final team-selected behaviour should be traced through an assumption ID where available.

6. `SYSTEM_DIAGRAMS.md`
   - Use it for topology, physical locations, controller ownership, and naming.

Use this reasoning precedence:

`Official RTS project brief`
→ `Finalised system assumptions`
→ `Final approved use cases`
→ `Internal Project Specification`
→ `Lecturer clarification notes`
→ `Other working files`

Do not silently resolve a genuine contradiction. Report any unresolved contradiction before drawing.

## 2. Approved use-case baseline

Use this final order:

- `UC-01 — Serve Vehicle Demand`
- `UC-02 — Serve Pedestrian Crossing Request`
- `UC-03 — Coordinate Arterial Traffic Progression`
- `UC-04 — Protect a Railway Crossing for an Approaching Train`
- `UC-05 — Manage Road Traffic During and After a Railway Closure`
- `UC-06 — Respond to a Railway Equipment Fault`
- `UC-07 — Configure Traffic Operating Parameters`
- `UC-08 — Apply a Bounded Clear-Route Override`
- `UC-09 — Monitor Network Status and Faults`
- `UC-10 — Continue Local Operation During Central Link Loss`

The sequence diagrams must cover all ten use cases, but one diagram may cover multiple closely related use cases.

Do not create one diagram mechanically for every use case if that produces duplicated interaction flows.

## 3. Architecture that must be preserved

Use the following naming consistently:

- `I1–I6`: physical signalised intersections.
- `L1–L6`: intersection local controllers.
- `RC1–RC3`: physical railway crossings.
- `RL1–RL3`: railway-crossing local controllers.
- `C1`: Central Controller.
- Central Control Room Operator: external human actor.

Controller authority:

- Each `Lx` exclusively controls the vehicle and pedestrian signals at its own `Ix`.
- Each `RLx` exclusively controls the gates, flashers, and train signals at its own `RCx`.
- `C1` monitors and sends high-level requests only.
- `C1` must never directly set a traffic light colour or actuate a gate, flasher, or train signal.
- Railway status flows from `RLx` to its two adjacent `Lx` controllers.
- That railway-to-intersection path is status-only; `Lx` must never command or query railway equipment.
- Safety-critical local action must not wait for Central acknowledgement.
- `DEGRADED_LOCAL` is a connectivity condition, not a replacement traffic mode.
- Each controller continues its local safety logic when Central is unavailable.

## 4. Required sequence-diagram set

Produce the following eight diagrams.

### SD-01 — Serve Off-Peak Vehicle Demand

Primary use-case coverage:

- UC-01

Use one representative intersection `Ix` and controller `Lx`; state that the same interaction applies to `I1–I6`.

Suggested participants:

- Vehicle
- Vehicle Presence Sensor
- `Lx`
- Vehicle Signals

Show the chronological successful flow:

1. A vehicle arrives and activates the approach detector.
2. The detector reports `DEMAND_PRESENT` to `Lx`.
3. `Lx` records or latches the demand.
4. `Lx` evaluates the active mode and phase eligibility.
5. If a conflicting phase is active, minimum green completes.
6. `Lx` commands yellow for 4 seconds.
7. `Lx` commands all-red for 2 seconds.
8. `Lx` activates the requested green.
9. Green runs for at least 8 seconds.
10. While demand persists, `Lx` extends green in 4-second increments up to 40 seconds.
11. When demand clears or the cap is reached, `Lx` completes the safe clearance.
12. The served demand clears, and normal phase selection resumes.

Use:

- `loop` for the 4-second extension checks;
- `alt` for demand clearing versus reaching the cap;
- an alternative showing no demand and arterial rest;
- a note for the anti-starvation rule (`DP-06`).

Do not simulate individual vehicles moving through the intersection. The vehicle is only the source of the detector event.

### SD-02 — Serve a Pedestrian Crossing Request

Primary use-case coverage:

- UC-02

Suggested participants:

- Pedestrian
- Pedestrian Button
- `Lx`
- Vehicle Signals
- Pedestrian Signal

Show:

1. Pedestrian presses the button.
2. The button reports `PED_REQUEST`.
3. `Lx` latches one request.
4. Repeated presses are coalesced.
5. `Lx` waits for or schedules the next compatible vehicle phase.
6. The compatible vehicle phase begins.
7. The pedestrian signal displays `WALK`.
8. It transitions to `FLASHING_DONT_WALK`.
9. It returns to `DONT_WALK`.
10. Only after clearance completes may a conflicting vehicle movement receive green.
11. `Lx` clears the pending request.
12. The pedestrian service ends successfully.

Use:

- `opt` or `alt` for repeated presses;
- `alt` for railway restriction delaying service;
- `critical` or a clearly labelled note to show that pedestrian clearance cannot be truncated;
- an alternative showing a Central override waiting or being rejected.

Do not show pedestrian crossings at `RC1–RC3`.

### SD-03 — Configure and Apply an Arterial Coordination Profile

Primary use-case coverage:

- UC-03
- UC-07

Use the R1 chain as the representative example:

- `L1 → L3 → L5`

State that `L2 → L4 → L6` on R2 follows the same pattern with its own offsets.

Suggested participants:

- Central Control Room Operator
- `C1`
- `L1`
- `L3`
- `L5`
- Local Vehicle Signals

Show:

1. The operator submits a `SET_TIMING_PROFILE` request.
2. `C1` validates the requested network-level profile format.
3. `C1` sends each `Lx` its own high-level timing parameters and offset.
4. Each `Lx` independently validates its parameters.
5. Each controller returns `ACK` or `NACK`.
6. Accepted profiles remain pending until the next safe phase boundary.
7. Each `Lx` applies its own offset locally.
8. `L1` starts the reference arterial green.
9. `L3` starts its corresponding green after the 21-second cumulative offset.
10. `L5` starts its corresponding green after the 45-second cumulative offset.
11. Each controller reports its active profile and phase state to `C1`.
12. `C1` displays successful coordinated operation.

Use:

- `par` for profile distribution and independent local validation;
- `alt` for `ACK` versus `NACK`;
- a note explaining that `C1` distributes parameters but never commands light colours;
- an alternative for a missing or stale coordination profile, causing standalone operation;
- an alternative for railway disruption, followed by recovery through the next valid profile;
- no separate gradual-resynchronisation algorithm.

Also include `SET_MODE` as a concise alternative branch if it shares the same validation and safe-boundary application path.

Trace timing and coordination behaviour to `TC-01`–`TC-05`, `TL-04`, and `PA-09`.

### SD-04 — Protect a Railway Crossing for an Approaching Train

Primary use-case coverage:

- UC-04

Use `RC1`, `RL1`, `L1`, and `L2` as the representative Pass-scope example.

Suggested participants:

- Train
- Train Approach Sensor
- `RL1`
- Road-Facing Flashers
- Boom Gates
- Gate Position Sensors
- Train Signal
- `L1`
- `L2`
- `C1`

Show:

1. The train activates the approach sensor.
2. The sensor reports `TRAIN_APPROACHING` to `RL1`.
3. `RL1` registers the train’s expected arrival and 20-second occupancy window.
4. `RL1` immediately activates the flashers.
5. In parallel, `RL1` reports `WARNING` to `L1`, `L2`, and `C1`.
6. `L1` and `L2` independently clear and suppress their movement toward `RC1`.
7. After the flash-only warning interval, `RL1` commands both gates to close.
8. The gate-position sensors report the actual gate state.
9. Only after confirmed closure does `RL1` set the relevant train signal to `PROCEED`.
10. The crossing remains protected while the occupancy window remains active.
11. `RL1` returns the train signal to `STOP`.
12. Only after every active occupancy window has elapsed does `RL1` reopen the gates.
13. After the gates reopen, the flashers stop.
14. `RL1` broadcasts `OPEN` to `L1`, `L2`, and `C1`.
15. The normal railway-crossing sequence completes.

Use:

- `par` for immediate flasher activation, status distribution, and occupancy-window registration;
- `critical` around gate confirmation before `PROCEED`;
- `alt` for confirmed versus unconfirmed closure;
- `opt` or `alt` for a second train;
- `loop` or an explicit guard while any occupancy window remains active;
- a note that the approximately 45-second value is the total warning-to-arrival budget, not one message delay;
- a note that the reserved exit sensor is not used by Core reopening logic.

Do not draw any message from `L1`, `L2`, or `C1` commanding railway equipment.

### SD-05 — Suppress and Drain Road Traffic Around a Railway Closure

Primary use-case coverage:

- UC-05

Suggested participants:

- `RLx`
- Crossing-Facing Queue Detector at the north-side intersection
- North-side `Lx`
- North-side Vehicle Signals
- Crossing-Facing Queue Detector at the south-side intersection
- South-side `Lx`
- South-side Vehicle Signals

Use generic names or the representative `RC1`/`L1`/`L2` configuration, but do not mix generic and concrete identifiers in the same diagram.

Show:

1. `RLx` reports a non-open crossing state to both adjacent `Lx` controllers.
2. Each `Lx` checks whether its toward-crossing movement is active.
3. If active, it completes minimum green, yellow, and all-red.
4. The toward-crossing signal is held at red.
5. Compatible cross-traffic and away-from-crossing movements may continue.
6. Queue detectors independently report `QUEUE_WARNING` when their fixed threshold is occupied.
7. `RLx` later reports `OPEN`.
8. Each affected `Lx` checks its own queue detector.
9. If `QUEUE_WARNING` remains active, the controller begins a connector drain phase.
10. The controller checks the binary warning every 4 seconds.
11. The drain ends when the warning clears or the 60-second cap is reached.
12. Required clearances complete.
13. Each intersection resumes its previous normal phase family.

Use:

- `par` for the two adjacent intersections reacting independently;
- `alt` for no queue warning versus a drain phase;
- `loop` for 4-second queue checks;
- `alt` for early clearance versus the 60-second cap.

Make explicit that:

- the detector is binary and does not count vehicles;
- the traffic lights suppress feeding toward the crossing but do not physically reroute vehicles;
- railway suppression does not create a third normal traffic-demand mode.

Trace to `CC-01`–`CC-03` and `TL-01`.

### SD-06 — Contain and Report a Railway Equipment Fault

Primary use-case coverage:

- UC-06

Suggested participants:

- Gate Position Sensor
- `RLx`
- Train Signal
- Boom Gates and Flashers
- Adjacent `Lx` Controllers
- `C1`
- Central Control Room Operator

Show:

1. `RLx` requests or supervises gate closure.
2. The required `CLOSED` confirmation is missing, contradictory, or explicitly faulted.
3. `RLx` detects that the safe condition cannot be verified.
4. In parallel:
   - `RLx` immediately forces or holds the train signal at `STOP`;
   - `RLx` applies the documented safe crossing outputs;
   - `RLx` reports `FAULT` to the adjacent `Lx` controllers;
   - `RLx` reports the fault to `C1`.
5. Adjacent `Lx` controllers hold the toward-crossing movements at red.
6. `C1` displays the fault to the operator.
7. The fault remains latched.
8. After physical repair, the operator submits a high-level fault-clear request through `C1`.
9. `RLx` independently verifies the physical condition.
10. `RLx` returns `ACK` and clears the fault, or returns `NACK` and retains the safe state.
11. The interaction ends in verified normal readiness or the safe fault state.

This diagram must use a `par` block to prove that the local `STOP` action does not wait for Central reporting or acknowledgement.

Use:

- `critical` for the immediate local safety response;
- `alt` for valid versus premature fault clearance;
- `break` only if Mermaid syntax remains readable for the terminated unsafe normal path.

Do not label the report as “fire-and-forget” unless that term is explicitly defined later in Section 5. Describe only the observable non-blocking relationship.

Trace to `RC-06`, `RC-09`, `RC-10`, `PA-09`, and `PA-10`.

### SD-07 — Validate and Apply a Bounded Clear-Route Override

Primary use-case coverage:

- UC-08

Suggested participants:

- Central Control Room Operator
- `C1`
- Target `Lx`
- Pedestrian Sequencer
- Railway Status Input
- Vehicle Signals

Show the successful chronological flow:

1. The operator selects a target through-movement and finite duration.
2. `C1` sends the high-level `REQUEST_OVERRIDE`.
3. The target `Lx` validates:
   - target movement;
   - requested duration;
   - conflict matrix;
   - active pedestrian clearance;
   - adjacent railway status;
   - local fault state.
4. `Lx` returns `ACK`.
5. `Lx` completes the current protected interval and mandatory clearances.
6. `Lx` activates the requested through-movement locally.
7. The override timer begins when the override becomes active.
8. The override expires, is cancelled, or is validly renewed.
9. `Lx` safely terminates the override through mandatory clearance.
10. `Lx` returns to its previous normal mode.
11. `Lx` reports completion to `C1`.
12. `C1` displays the result to the operator.

Use:

- `alt` for:
  - pedestrian clearance in progress;
  - conflicting railway pre-emption;
  - invalid or over-limit duration;
  - local fault;
- `NACK` for every unsafe request;
- a note that only `Lx` actuates its physical traffic signals;
- the new `PA-11` rule if it exists in the current assumptions:
  - finite duration;
  - maximum 5 minutes;
  - automatic expiry unless validly renewed;
  - earlier cancellation permitted.

If `PA-11` has not yet been added to the repository, explicitly report that traceability gap before generating this diagram. Do not silently cite the internal Project Specification as authority.

### SD-08 — Monitor Controllers, Lose Central Link, and Resynchronise

Primary use-case coverage:

- UC-09
- UC-10

Suggested participants:

- Central Control Room Operator
- `C1`
- Generic Local Controller
- Local Clock
- Local Sensors
- Local Actuators

The “Generic Local Controller” represents any `Lx` or `RLx`. Do not draw nine duplicate lifelines.

Show:

1. While connected, the local controller sends heartbeats every second.
2. It reports state transitions and faults to `C1`.
3. `C1` updates the network display.
4. The operator reviews current controller status.
5. Three consecutive Central heartbeats are missed.
6. The local controller declares the Central link unavailable and enters `DEGRADED_LOCAL`.
7. Central coordination and override availability are disabled locally.
8. The controller retains its last validated timing parameters.
9. The local clock continues selecting Peak or Off-Peak operation where applicable.
10. Local sensors continue driving local behaviour.
11. Local actuators continue operating under local authority.
12. Central marks the controller state as stale or unavailable.
13. Communication later returns.
14. The local controller sends its complete current state to `C1`.
15. `C1` replaces its stale view with the reported actual state.
16. Only after state synchronisation does the local controller accept new Central requests.
17. Normal coordination resumes, completing the recovery.

Use:

- `loop` for heartbeat and normal status reporting;
- `alt` for link available versus three missed heartbeats;
- `par` to show local operation continuing while Central marks the node stale;
- `critical` or a note for local-state push before accepting new commands;
- an alternative where the link remains unavailable and safe autonomous operation continues indefinitely;
- an optional railway event during disconnection, demonstrating zero Central dependency.

Do not show communication loss forcing all-red, `FLASHING_RED`, frozen timing, or frozen mode selection.

Trace to `PA-07`, `PA-08`, `TC-04`, and `RC-10`.

## 5. Mermaid syntax requirements

Use only Mermaid `sequenceDiagram`.

For every diagram:

- provide one independent fenced `mermaid` code block;
- include a YAML title;
- include `accTitle` and `accDescr`;
- use `autonumber`;
- declare participants explicitly and in a logical left-to-right order;
- use `actor` only for humans, vehicles, pedestrians, or trains where appropriate;
- use `participant` for controllers, sensors, and actuators;
- use aliases to keep participant headings short;
- use `->>` for requests, commands, and physical events;
- use `-->>` for acknowledgements, status reports, and responses;
- use `-)` only when explicitly representing an asynchronous/non-blocking event;
- use activation bars sparingly;
- use `loop`, `alt`, `opt`, `par`, `critical`, and `break` only where they add behavioural meaning;
- keep every message concise and observable;
- avoid implementation-level QNX APIs, process IDs, channel IDs, thread names, and message structs;
- avoid using the standalone word `end` inside message labels because Mermaid may parse it as block termination;
- keep each diagram readable on an A4 report page;
- ensure every Mermaid block parses independently in the current Mermaid Live Editor.

Do not produce a flowchart or state diagram as a substitute.

## 6. Behavioural-modelling rules

The diagrams must show chronological interaction, not duplicate state-chart content.

Therefore:

- show who initiates each event;
- show which controller makes each decision;
- show which controller owns each physical actuation;
- show acknowledgements and rejections where externally observable;
- show parallel local safety action where required;
- show complete normal endings;
- show only important alternative and fault paths;
- do not enumerate every internal timer tick unless it changes the observable interaction;
- do not turn Section 4.2 into an IPC structure specification.

Detailed message types, fields, QNX channels, and implementation structs belong in Section 5.2.

## 7. Traceability requirements

After each diagram, provide three concise bullets:

- **Covers:** relevant use-case IDs.
- **Constraints:** relevant assumption IDs.
- **Official requirement:** relevant page of `RTS_Final Project.pdf`, where directly applicable.

Do not cite `PROJECT_SPECIFICATION.md` as an official requirement.

If behaviour is a team design decision, cite its assumption ID.

If required behaviour has no corresponding assumption ID, identify it as a traceability gap instead of citing the internal specification.

## 8. Quality checks before answering

Confirm all of the following:

1. All ten approved use cases are covered by at least one sequence diagram.
2. Every Main Flow reaches a complete observable outcome.
3. Alternative branches either rejoin a named point or terminate in an explicit state.
4. Central never directly actuates physical equipment.
5. `Lx` never commands railway equipment.
6. Railway safety never waits for Central.
7. Gate-confirmed closure occurs before train `PROCEED`.
8. Gates remain closed while any occupancy window remains active.
9. Railway pre-emption preserves intersection yellow and all-red clearance.
10. Pedestrian clearance cannot be truncated.
11. Queue detection is binary and is not represented as vehicle counting.
12. An unsafe Central request produces `NACK`.
13. `DEGRADED_LOCAL` retains the last validated parameters while local mode selection continues.
14. Reconnection pushes local actual state to Central before new commands are accepted.
15. The diagrams complement rather than duplicate the five state charts in Section 4.1.
16. Every Mermaid block is syntactically valid and legible.

## 9. Required response format

Respond in this order:

1. **Coverage decision**
   - Confirm whether eight diagrams are sufficient.
   - Explain briefly why one sequence diagram per use case is unnecessary.

2. **Repository version checked**
   - State the commit hash used.
   - Identify whether the repository contains the approved ten-use-case version.
   - Identify whether `PA-11` exists.

3. **Contradictions or traceability gaps**
   - List only genuine problems.
   - Do not invent resolutions.

4. **SD-01 Mermaid code and traceability**

5. **SD-02 Mermaid code and traceability**

6. **SD-03 Mermaid code and traceability**

7. **SD-04 Mermaid code and traceability**

8. **SD-05 Mermaid code and traceability**

9. **SD-06 Mermaid code and traceability**

10. **SD-07 Mermaid code and traceability**

11. **SD-08 Mermaid code and traceability**

12. **Use-case coverage summary**
   - Use concise bullets rather than a large table.

13. **Final consistency and Mermaid validation check**

Return only the diagrams and supporting analysis. Do not edit or commit repository files.
