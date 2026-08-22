# EEET2588 Real-Time Systems Project — Master Project Design Prompt

You are acting as the **lead systems architect for this project**.

Your job is to understand all available project information and then **design the project itself at the system level**.

You are NOT being asked to write the Initial Design Report.

You are NOT being asked to implement or code the system yet.

Instead, your primary deliverable is a comprehensive Markdown file named:

`PROJECT_SPECIFICATION.md`

This file must become the team's **single source of truth for the entire project**.

It should describe, in sufficient detail, **what system we are going to build, how it should behave, which features it contains, how its components interact, what assumptions we are making, how failures are handled, and what scenarios we intend to demonstrate**.

Later, we will derive the Initial Design Report, UML diagrams, QNX architecture, implementation, testing, and demonstration from this specification.

---

# 1. Read All Project Sources First

Before designing anything, read all relevant files in the project.

At minimum, read:

## `RTS Final Project`

This is the **official project specification from the lecturer**.

Treat it as the primary authority for formal requirements.

---

## `lecture clarification`

This contains explanations and clarifications given directly by the lecturer during class.

This file is extremely important.

Many details in it are either:

* not explicitly stated in the official brief;
* clarifications of ambiguous statements in the brief;
* indications of what the lecturer expects from a strong solution.

Treat lecturer clarification as authoritative supplementary information.

If it appears to conflict with the official specification, explicitly flag the issue instead of silently deciding.

---

## `idea.md`

This file is **NOT the project design**.

It is mostly a brainstorming document containing:

* questions;
* uncertainties;
* things I thought might need decisions;
* possible ideas;
* issues I did not know how to solve.

Do not blindly answer every question in this file.

Do not assume all questions are important.

Use it to identify unresolved design areas.

If a question is unnecessary, irrelevant, premature, or does not materially affect the project, you may ignore or remove it.

---

## `README.md`

This was written early, before the system was fully understood.

Some content may be correct.

Some may be:

* assumptions;
* premature design decisions;
* incorrect interpretations;
* arbitrary values.

Validate everything in README against:

1. official specification;
2. lecturer clarification;
3. sound engineering reasoning.

Do not preserve an idea merely because it already appears in README.

---

# 2. Your Actual Goal

You need to **design the complete project concept**.

Think of this as:

> "Before anyone starts drawing UML or writing C code, exactly what system are we building?"

You need to make enough design decisions that the team can later move from:

`PROJECT_SPECIFICATION.md`

to:

* Initial Design Report;
* physical layout diagrams;
* use cases;
* state charts;
* sequence diagrams;
* task architecture;
* QNX processes;
* IPC message design;
* source code;
* testing;
* final demonstration.

The project specification must therefore be much more detailed than a summary of the assignment.

---

# 3. HD Is the Target

The project is intended to target an **HD-level result**.

However:

**HD does NOT mean maximum feature count.**

The official project specifically indicates that complexity is valuable only when it is:

* sensible;
* justified;
* correctly implemented;
* demonstrable;
* supported by evidence.

Therefore, optimise the design for:

* strong real-time-system reasoning;
* distributed architecture;
* local autonomy;
* safety;
* fault handling;
* meaningful coordination;
* adaptive behaviour where justified;
* good IPC opportunities;
* realistic sensors;
* observable system behaviour;
* strong demonstration scenarios.

Avoid adding features purely because they sound advanced.

Every major feature should have a reason to exist.

---

# 4. Reconstruct the System Correctly

Before proposing enhancements, establish what the system actually contains.

Pay particular attention to the lecturer clarification concerning:

* 6 signalised road intersections;
* one local controller for each intersection;
* additional railway-crossing local controllers;
* central controller;
* train-line behaviour;
* boom gates;
* railway flashing lights;
* train signals;
* train detection sensors;
* pedestrian buttons;
* pedestrian signals;
* car/vehicle sensors;
* fixed timing;
* sensor-driven operation;
* updateable timing;
* central override;
* railway congestion;
* coordination between intersections;
* QNX nodes and processes.

Do not confuse the following concepts:

### Physical infrastructure

Examples:

* roads;
* lanes;
* intersections;
* tracks;
* pedestrian crossings;
* traffic lights;
* train signals;
* boom gates;
* sensors.

### Logical controllers

Examples:

* Central Controller;
* I1–I6 Local Intersection Controllers;
* railway-crossing controllers.

### Software architecture

Examples:

* QNX nodes;
* processes;
* threads/tasks;
* IPC;
* synchronisation.

### Physical demonstration environment

Examples:

* laptops;
* PCs;
* VMs;
* QNX targets.

One logical controller does NOT automatically equal one QNX node.

One QNX node does NOT automatically equal one physical computer.

Do not lock the project into such mappings unless there is a reason.

---

# 5. Distinguish Facts From Decisions

For every important item, classify it as one of:

* **Official Requirement**
* **Lecturer Clarification**
* **Team Design Decision**
* **Engineering Assumption**
* **Proposed HD Enhancement**
* **Needs Lecturer Confirmation**

Do not turn assumptions into requirements.

Do not invent exact numbers simply to make the document look complete.

For example, if train detection lead time is not specified, do not silently state:

> Train is detected exactly 60 seconds before arrival.

Instead, either:

* derive a justified value;
* propose a value and label it as an assumption;
* or mark it as an open design decision.

---

# 6. Design the Project

After understanding all sources, develop the actual system.

The design should consider the following areas.

These are not mandatory features; evaluate them and select what forms the strongest coherent system.

## Traffic operation

Consider:

* fixed-time traffic sequences;
* peak-hour traffic behaviour;
* off-peak behaviour;
* sensor-driven operation;
* configurable timing;
* road priority;
* turning movements;
* right-turn arrows if justified;
* intersection coordination;
* green-wave behaviour;
* congestion management.

---

## Pedestrian operation

Consider:

* pedestrian buttons;
* request latching;
* pedestrian WALK signal;
* pedestrian DON'T WALK signal;
* minimum crossing time;
* interaction with traffic phases;
* interaction with railway pre-emption;
* what happens if multiple pedestrian requests occur.

---

## Railway operation

Consider the complete lifecycle:

1. train detected;
2. railway controller informed;
3. neighbouring road controllers informed;
4. road traffic cleared if necessary;
5. flashing warnings activate;
6. boom gates close;
7. gate state verified;
8. train receives permission to proceed;
9. train passes;
10. crossing reopens;
11. traffic returns to normal.

Also define abnormal behaviour such as:

* boom-gate failure;
* train signal failure if relevant;
* crossing unable to secure;
* communication failure.

---

## Railway congestion management

Use the lecturer's clarification seriously.

When the railway crossing is closed, traffic should not simply continue being directed toward the blocked crossing.

Determine a reasonable strategy for:

* suppressing movements toward the crossing;
* allowing alternative turning movements;
* avoiding queues extending back into upstream intersections;
* coordinating adjacent intersections.

Do not describe this merely as "traffic rerouting" unless the traffic-light system can realistically influence that movement.

Clearly state what the lights can actually accomplish.

---

## Central control

Define what the Central Controller can do.

Possible responsibilities include:

* monitor all local controllers;
* receive state-change events;
* display current light states;
* receive faults;
* display railway state;
* change operating mode;
* modify timing parameters;
* request sequence changes;
* issue override commands;
* observe connection health.

Also explicitly define what Central **cannot** do.

The system must preserve local-controller authority over individual physical signals.

---

## Local autonomy

Each local controller should have clearly defined independent behaviour.

Determine what happens when:

* Central is unavailable;
* communication drops;
* commands stop arriving;
* stale data is received;
* another controller becomes unreachable.

Local controllers should retain safe operation where required.

---

# 7. Define the Full Project Feature Set

After analysis, classify proposed features into:

## Core Features

Features required for the system to satisfy the assignment.

## HD-Target Features

Features that significantly strengthen:

* real-time reasoning;
* distributed coordination;
* adaptability;
* reliability;
* safety;
* demonstration quality.

## Optional / Stretch Features

Useful features that may be implemented only if time permits.

## Rejected Features

Ideas considered but intentionally excluded because they:

* add unnecessary complexity;
* provide little assessment value;
* cannot be realistically demonstrated;
* conflict with the architecture;
* are too risky for the implementation schedule.

Explain the reasoning for these classifications.

---

# 8. Define the Complete System Behaviour

The project specification must describe how the system behaves under important scenarios.

At minimum, consider:

### Normal operation

* normal fixed timing;
* normal sensor-driven operation.

### Pedestrian request

Example concept:

`button press → request stored → safe phase reached → WALK → clearance → normal traffic resumes`

### Vehicle sensor event

Explain how detected demand affects the local sequence.

### Train approaching

Define the entire system response.

### Train currently occupying crossing

Define how road traffic behaves.

### Railway crossing reopening

Define recovery to normal traffic behaviour.

### Boom-gate failure

Define:

* train signal response;
* road traffic response;
* Central notification;
* recovery requirements.

### Central timing update

Define how timing changes are requested and safely adopted.

### Central override

Define what an override is allowed to change and how normal operation resumes afterward.

### Communication loss

Define behaviour when a local controller loses contact with Central.

### Local controller communication with neighbouring controllers

Determine whether this is required and, if so, what information is exchanged.

---

# 9. Define System Modes

Determine the useful operating modes for the final design.

Do not automatically use every possible mode.

Possible examples include:

* PEAK_FIXED;
* OFF_PEAK_SENSOR;
* RAILWAY_PREEMPTION;
* CENTRAL_OVERRIDE;
* DEGRADED_LOCAL;
* FAULT_SAFE.

Choose an appropriate set.

For each mode specify:

* purpose;
* entry condition;
* behaviour;
* permitted commands;
* exit condition;
* priority relative to other modes.

If railway or safety behaviour must override normal traffic operation, make that priority explicit.

---

# 10. Define Safety Rules and Invariants

Create explicit system-level safety rules.

Examples of the type of rules required:

* conflicting vehicle movements must never receive green simultaneously;
* pedestrian WALK must never conflict with an active vehicle movement through that crossing;
* unsafe crossing state must result in STOP for the train;
* local intersection operation must not freeze because Central becomes unavailable;
* transitions must include appropriate clearance intervals.

Do not simply copy these examples.

Determine the complete set appropriate for the chosen design.

These safety rules should later drive state charts and testing.

---

# 11. Define Sensors and Actuators

Create a complete proposed inventory.

For every sensor define:

* name;
* location;
* physical purpose;
* controller that owns it;
* data/event generated;
* why it is realistic;
* how it may be simulated during demonstration.

Potential sensors may include:

* vehicle detectors;
* pedestrian buttons;
* train approach detectors;
* boom-gate state sensors.

Only include realistic and useful sensors.

For every actuator define:

* owning controller;
* possible states;
* safe state.

Potential actuators include:

* traffic signals;
* turn arrows;
* pedestrian signals;
* boom gates;
* railway flashing lights;
* train signals.

---

# 12. Define Controller Responsibilities

Create a responsibility specification for each controller class.

At minimum:

## Central Controller

Define:

* inputs;
* outputs;
* commands;
* monitoring responsibilities;
* limitations.

## Intersection Local Controller

Define:

* controlled devices;
* local sensors;
* traffic logic;
* pedestrian logic;
* railway information it consumes;
* fallback behaviour;
* Central interaction.

## Railway-Crossing Local Controller

Define:

* train detection;
* boom-gate control;
* flashing-light control;
* train signal;
* crossing state;
* failure handling;
* information shared with road controllers;
* Central reporting.

If different railway crossings need different behaviour, explain why.

---

# 13. Define Controller Relationships

Specify all meaningful logical communication paths.

For example:

`Central ↔ Intersection Controller`

`Central ↔ Railway Controller`

`Railway Controller → Nearby Intersection Controllers`

`Neighbouring Intersection Controller ↔ Intersection Controller`

Only include communication paths that are actually justified.

For each relationship explain:

* what information needs to move;
* why;
* whether it is command, status, event, acknowledgement, or fault;
* whether the communication is safety-relevant;
* expected behaviour if communication fails.

Do NOT design actual C structs yet unless necessary.

This is still a system-level specification.

---

# 14. Real-Time Requirements

Identify the actual real-time characteristics of the design.

For every important event, describe:

* trigger;
* expected response;
* relative urgency;
* whether a deadline exists;
* whether the event is periodic or asynchronous;
* what happens if the response is late.

Examples may include:

* traffic phase timers;
* pedestrian crossing intervals;
* train approach warning;
* railway crossing closure;
* status updates;
* heartbeat/connection detection;
* sensor input;
* emergency override.

If exact numerical deadlines are not yet known, specify that they need to be derived rather than inventing numbers.

---

# 15. Traffic Timing Philosophy

The official topology does not provide final signal timings.

Develop a methodology for choosing them.

Consider:

* real intersection modelling;
* road distances;
* expected vehicle speed;
* main-road priority;
* peak/off-peak demand;
* intersection-to-intersection travel time;
* railway-crossing proximity;
* pedestrian crossing duration.

If exact measurements require external research or lecturer confirmation, state this clearly.

Do not fabricate realistic-looking values without justification.

---

# 16. Real-World Reference

Determine whether the project should be based on:

* the provided abstract topology;
* a real-world intersection/network mapped onto that topology;
* or the provided topology enhanced using realistic measurements.

Recommend the strongest option for an HD-level project.

If changing the provided topology would require lecturer approval, flag it clearly.

---

# 17. Demonstration Design

The project must eventually be demonstrated.

Therefore define **demonstration scenarios alongside the system design**.

For every important feature, determine how an assessor could visibly verify it.

Potential scenarios include:

* normal traffic operation;
* pedestrian button press;
* vehicle sensor event;
* mode switch;
* timing update;
* approaching train;
* railway pre-emption;
* boom-gate failure;
* train STOP signal;
* Central communication failure;
* local autonomous recovery;
* Central override;
* congestion response.

Where keyboard input is used to simulate sensors, propose a clear key mapping.

Example:

`T1` → Train approaching crossing 1

`P1N` → Pedestrian request on north side of I1

However, develop a sensible final scheme rather than copying these examples automatically.

The specification should describe **what will be demonstrated**, not implementation code.

---

# 18. Keep the Project Implementable

All design decisions must eventually be feasible using:

* QNX;
* C or C++;
* QNX IPC;
* QNX synchronisation mechanisms;
* multiple processes;
* multiple QNX nodes.

However, do NOT start writing code.

Do NOT prematurely decide implementation details such as exact C structs, filenames, functions, mutex placement, or thread APIs unless they are necessary to validate architectural feasibility.

The objective right now is to finish the **system design**.

---

# 19. Questions and Unresolved Decisions

You are allowed to ask me questions.

However, do not ask dozens of low-value questions like the current `idea.md`.

Only ask when a decision:

* materially changes system behaviour;
* changes project scope;
* affects architecture;
* affects implementation difficulty;
* affects demonstration;
* could significantly affect the grade;
* cannot be confidently resolved from the lecturer material.

When asking:

1. state the unresolved issue;
2. explain why it matters;
3. give the realistic options;
4. explain the trade-offs;
5. recommend one option;
6. ask me to confirm.

Batch related questions.

If you can make a reasonable recommendation yourself, include the recommendation in the specification and label it **Proposed — Awaiting Confirmation** rather than stopping all work.

---

# 20. Required Deliverable: `PROJECT_SPECIFICATION.md`

Create or rewrite:

`PROJECT_SPECIFICATION.md`

This is the main output of this task.

It should contain at least the following sections:

# Traffic Light Control System — Project Specification

## 1. Project Vision

Explain in plain engineering language what system we are building and what makes the chosen solution strong.

## 2. Source of Requirements

Summarise which requirements come from the official brief and which important clarifications come from the lecturer.

## 3. Design Goals

Define the major goals of the system.

## 4. Scope

### 4.1 Full Conceptual System

### 4.2 Planned Proof of Concept

### 4.3 Optional Scope

## 5. Physical Network

Define the roads, intersections, railway crossings and relevant topology.

## 6. System Components

Include all major physical and logical components.

## 7. Controller Architecture

Define all logical controllers and responsibilities.

## 8. Sensors

Define selected sensors and justification.

## 9. Actuators and Signals

Define traffic, pedestrian and railway outputs.

## 10. Operating Modes

Fully define the chosen modes and mode priority.

## 11. Normal Traffic Behaviour

Define vehicle control logic conceptually.

## 12. Pedestrian Behaviour

Define the complete pedestrian interaction model.

## 13. Railway Behaviour

Define complete normal railway-crossing behaviour.

## 14. Railway Pre-emption and Traffic Clearing

Define how approaching trains influence nearby traffic.

## 15. Railway Congestion Management

Define how the system prevents traffic accumulation near closed crossings.

## 16. Failure and Safety Behaviour

Include all selected failure cases and safe responses.

## 17. Central Controller Behaviour

Define monitoring, commands and override.

## 18. Local Autonomy

Define behaviour without Central.

## 19. Controller Coordination

Define information exchange between controllers.

## 20. Timing and Coordination Strategy

Define how timings will be derived and coordinated.

## 21. Real-Time Requirements

Identify timing-critical events and priorities.

## 22. Safety Invariants

List rules that must never be violated.

## 23. Feature Classification

### Core

### HD-Target

### Stretch

### Rejected / Deferred

## 24. Demonstration Scenarios

Define how each important project claim will eventually be demonstrated.

## 25. Simulation Inputs

Define proposed keyboard/simulated sensor events where appropriate.

## 26. Assumptions

Maintain a clear list of all assumptions.

Each assumption should include:

* assumption;
* reason;
* design consequence;
* whether lecturer confirmation is needed.

## 27. Open Design Decisions

Only unresolved high-impact decisions belong here.

## 28. Questions for Team / Lecturer

Separate questions that can be answered internally from questions requiring lecturer confirmation.

## 29. Traceability

For major features, indicate whether they originated from:

* official specification;
* lecturer clarification;
* team design;
* HD enhancement.

## 30. Final Proposed System Summary

Provide a concise but complete description of the system after all current decisions.

---

# 21. Quality Standard for the Specification

The file should be detailed enough that another engineer could read only:

`PROJECT_SPECIFICATION.md`

and understand:

* exactly what we intend to build;
* why each major feature exists;
* how the whole system behaves;
* what each controller owns;
* how railway and road traffic interact;
* what occurs during failures;
* what assumptions remain;
* what makes the project HD-level;
* what will eventually be demonstrated.

Do not fill the file with generic textbook explanations.

Every section should relate directly to **our traffic-light project**.

Avoid vague statements such as:

> "The system should be safe and efficient."

Instead specify what safety or efficiency means in this system.

---

# 22. Do Not Produce the Initial Design Report Yet

Do NOT structure the output around the assignment-report headings simply because an Initial Design Report is due.

The report will be generated later **from this project specification**.

The conceptual flow must be:

`Official Requirements + Lecturer Clarifications`

↓

`PROJECT_SPECIFICATION.md`

↓

`Initial Design Report`

↓

`Detailed Architecture`

↓

`QNX Implementation`

↓

`Testing`

↓

`Technical Demonstration`

The project design comes first.

---

# 23. Final Working Principle

You are not helping me "answer an assignment."

You are helping me **design the system that the assignment will document and demonstrate**.

Whenever there is a choice between:

**something that looks sophisticated in the report**

and

**something that makes the actual system stronger, more coherent, safer, more demonstrable, and easier to defend**

choose the second.

Build one coherent HD-level project, not a collection of unrelated features.

After reading all source files, begin developing `PROJECT_SPECIFICATION.md`.

If major design decisions remain uncertain, make your best engineering recommendation, mark them clearly, and ask me only the high-impact questions needed to finalise them.
