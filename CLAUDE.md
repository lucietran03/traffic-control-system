# Task: Develop HD-Level Use Case Specifications for the Traffic Control System

I am preparing the **Initial Design Report** for the EEET2588/EEET2687 Real-Time Systems Engineering project.

Your task is to develop and refine the **Use Case Scenarios and Written Specifications** for Section **3.2 Functional Specifications**.

The goal is NOT to make the use cases unnecessarily long. They must be **precise, traceable, technically meaningful, and consistent with the finalized system design**, at a standard appropriate for a **Distinction / High Distinction engineering design report**.

---

# 1. Read the Project Context First

Before writing or modifying any use case, read the relevant project files in the repository.

Priority order:

1. `PROJECT_SPECIFICATION.md`
   - Treat this as the primary source of truth for the finalized system architecture, behaviour, scope, safety rules, operating modes, and controller responsibilities.

2. The finalized **System Assumptions** file.
   - Use the assumptions and their IDs (`NU-xx`, `DP-xx`, `TL-xx`, `TC-xx`, `CC-xx`, `RC-xx`, `PA-xx`) as explicit design constraints.

3. `SYSTEM_DIAGRAMS.md`
   - Use this to understand topology, controller relationships, naming, and system visualisation.

4. `lecture_clarification.md` or equivalent lecturer-clarification file.
   - Use this to verify requirements or interpretations that may not appear explicitly in the original brief.

5. The original project brief / `RTS Final Project` file.
   - Use this to verify the source requirements.

6. Existing README / idea files may provide context, but they are NOT authoritative if they conflict with the finalized Project Specification.

### Source precedence

If sources conflict, use:

`Finalized Project Specification`
→ `Finalized System Assumptions`
→ `Lecturer Clarifications`
→ `Original Project Brief`
→ `Other working/idea files`

Do NOT silently resolve a genuine unresolved contradiction.

If an important behavioural decision is still ambiguous, identify it and ask me before inventing a solution.

---

# 2. Use Case Scope

Unless explicitly stated otherwise, treat the **entire distributed Traffic Control System** as the system boundary.

Therefore, internal controllers such as:

- `C1`
- `L1–L6`
- `RL1–RL3`

are normally **internal system components**, NOT external UML actors.

Do not inconsistently treat an internal controller as an actor unless a particular use case explicitly declares a narrower subsystem boundary and there is a strong reason to do so.

Potential external actors may include, where appropriate:

- Central Control Room Operator
- Pedestrian
- Vehicle / road user
- Train / train-detection input
- external sensor/input sources

However, determine the correct actors from the actual project design rather than mechanically using this list.

---

# 3. Required Use Case Specification Format

Every use case must use a **two-column table**:

| Field | Description |
|---|---|
| Use Case | ... |
| Goal | ... |
| Primary Actors | ... |
| Secondary Actors | ... |
| Description | ... |
| Pre-conditions | ... |
| Triggers | ... |
| Basic Course of Events | ... |
| Alternative Paths | ... |
| Post-conditions | ... |
| Business Rules | ... |

Do NOT add additional fields unless there is a compelling reason and I approve it.

---

# 4. Rules for Each Field

## Use Case

Use:

`UC-XX — <Active, goal-oriented name>`

The name must describe the behaviour/outcome rather than an implementation component.

Good:
- `UC-04 — Handle Train Approach`
- `UC-05 — Serve Pedestrian Crossing Request`

Avoid:
- `Railway Controller`
- `RL1 Process`
- `Sensor Handler`

---

## Goal

Write **one concise sentence** describing the successful outcome of the use case.

Describe WHAT must be achieved, not HOW the software implements it.

Do not repeat the Description.

---

## Primary Actors

Identify the external actor that initiates the interaction or whose goal drives the use case.

Do NOT automatically list `C1`, `Lx`, or `RLx` as actors when the whole distributed system is the system boundary.

---

## Secondary Actors

List external actors/resources that participate in or support the interaction but do not initiate the primary goal.

Use `None` when appropriate.

Do not populate this field simply to make the specification appear more detailed.

---

## Description

Provide a concise **1–2 sentence overview** explaining:

- the context;
- what behaviour is covered;
- the general outcome.

Do not repeat the Goal or reproduce the Basic Course.

---

## Pre-conditions

State only conditions that must already be true before the use case begins.

They must be specific system states where possible.

Example:

`RC1 is OPEN and its railway controller is operational.`

Do NOT confuse a trigger with a pre-condition.

For example:

`A train is detected approaching RC1`

is normally a trigger, not a pre-condition.

Avoid trivial conditions such as "the system is powered on" unless they are genuinely relevant.

---

## Triggers

Specify the concrete event that starts the use case.

Examples:

- a pedestrian button is pressed;
- a train-approach sensor becomes active;
- an operator submits a mode-change request;
- vehicle demand becomes present.

Keep the trigger separate from the pre-conditions.

---

# 5. Basic Course of Events

This is the most important part of the specification.

Write the **normal successful path** as numbered chronological steps.

Example structure:

1. The external actor initiates the request.
2. The system detects/receives the event.
3. The system validates the applicable conditions.
4. The system performs the required safe transition.
5. The system reports or exposes the resulting state.
6. Normal operation resumes or the requested service completes.

Each step should describe meaningful:

**actor/input action → observable system response**

where appropriate.

### Do NOT:

- put failure branches inside the Basic Course;
- write pseudo-code;
- describe QNX APIs;
- mention `MsgSend()`, channels, process IDs, structs, threads, etc.;
- expose unnecessary implementation details;
- create artificial steps simply to make the flow longer.

Section 3 specifies **system behaviour**, not low-level implementation.

IPC implementation belongs in Section 5.

---

# 6. Alternative Paths

Alternative Paths must describe meaningful:

- deviations;
- exceptions;
- fault cases;
- safety responses;
- unavailable conditions.

Where possible, reference the Basic Course step where the branch occurs.

Use this format:

**A1 — Gate confirmation failure (Step 5):**  
If the required `CLOSED` confirmation is not received, the system keeps the train signal at `STOP`, enters the applicable safe state, reports the fault, and terminates the normal crossing sequence.

**A2 — ... (Step X):**  
...

State what happens after the alternative:

- return to Step X;
- resume normal operation;
- remain in a safe state;
- terminate the use case.

Do NOT write vague alternatives such as:

`Gate failure → stop train.`

The behaviour and resulting state must be explicit.

---

# 7. Post-conditions

Describe the observable system state after the use case completes.

Do NOT simply repeat the final Basic Course step.

Where relevant, account for both successful and safe exceptional outcomes.

Example:

- The requested pedestrian service has completed and its pending request is cleared.
- The crossing has returned to `OPEN`, or remains in a documented safe fault state if normal reopening cannot complete.

Post-conditions must remain consistent with the project's actual state model.

---

# 8. Business Rules

Use this field for the safety constraints, invariants, policies, and design rules governing the use case.

Where possible, explicitly trace them to the finalized System Assumptions.

Example:

**BR-1:** The train signal may show `PROCEED` only while the relevant gates are sensor-confirmed `CLOSED` (`RC-06`).

**BR-2:** Railway fault response must not depend on Central connectivity (`RC-10`).

**BR-3:** Yellow and all-red clearance intervals cannot be bypassed (`TL-01`).

Do NOT invent a new business rule merely to fill the field.

If an existing assumption already defines the rule, reference it.

---

# 9. Traceability Is Important

The use cases must form part of one coherent system design.

Maintain traceability between:

`Project Requirement`
→ `System Assumption`
→ `Use Case`
→ `State Chart`
→ `Sequence Diagram`
→ `Implementation Architecture`

The use cases should therefore be suitable for later mapping to:

- Section 4.1 State Charts;
- Section 4.2 Sequence / Behavioural Diagrams.

Do NOT introduce behaviour in Section 3 that has no support in the finalized Project Specification or assumptions.

Likewise, identify important finalized system behaviour that is not represented by any proposed use case.

---

# 10. Writing Style

Use professional engineering-report language.

Prioritise:

- precision;
- deterministic behaviour;
- concise wording;
- clear system boundaries;
- observable behaviour;
- traceability;
- safety constraints.

Avoid:

- unnecessary prose;
- marketing language;
- vague claims;
- repetitive explanations;
- implementation-level detail;
- restating the same information across Goal, Description, Basic Course, and Post-conditions.

A longer use case is NOT automatically better.

Every sentence must add useful behavioural information.

---

# 11. Important Project Principles

While reading the repository, preserve the finalized architecture, particularly the distinction between:

- Central coordination and local authority;
- intersection controllers and railway controllers;
- normal traffic modes and safety/overlay states;
- ordinary vehicle-presence detection and congestion/queue detection;
- railway pre-emption and normal traffic scheduling;
- safe local operation and Central connectivity;
- high-level Central requests and direct hardware actuation.

Do not allow a use case to accidentally give `C1` direct authority over physical signals, boom gates, flashers, or train signals if the finalized architecture assigns that authority to local controllers.

Similarly, safety-critical actions must remain local where specified by the project.

---

# 12. Workflow

Do NOT immediately generate a large number of final use cases.

First:

1. Read the project files.
2. Identify the important externally observable system behaviours.
3. Propose a concise list of candidate use cases.
4. For each candidate, state:
   - proposed ID;
   - name;
   - primary actor;
   - one-sentence purpose;
   - which project assumptions it traces to.
5. Check whether any candidates overlap and should be merged.
6. Check whether any important behaviour is missing.
7. Check whether any proposed use case is actually an internal implementation behaviour rather than a genuine use case.

Then present the proposed use-case set to me for review.

**Do not write the full specification tables until I approve the use-case list.**

---

# 13. After Approval

Once I approve the use-case list, develop each use case using exactly:

| Field | Description |
|---|---|
| **Use Case** | `UC-XX — ...` |
| **Goal** | ... |
| **Primary Actors** | ... |
| **Secondary Actors** | ... |
| **Description** | ... |
| **Pre-conditions** | ... |
| **Triggers** | ... |
| **Basic Course of Events** | 1. ... <br> 2. ... <br> 3. ... |
| **Alternative Paths** | **A1 — ... (Step X):** ... <br><br> **A2 — ... (Step Y):** ... |
| **Post-conditions** | ... |
| **Business Rules** | **BR-1:** ... (`XX-XX`) <br> **BR-2:** ... (`XX-XX`) |

Before finalising each use case, perform a consistency check against the Project Specification and System Assumptions.

If you encounter an unresolved design decision that materially affects the behaviour, **ask me rather than inventing an answer**.
