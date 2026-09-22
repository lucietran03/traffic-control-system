# Traffic Light Control System — Project Specification (Table of Contents)

**English translation of `PROJECT_SPECIFICATION.vi.md` / `spec-vi/`.** The Vietnamese version remains the team's primary working copy; this English version is the official submission copy, translated from the Vietnamese chapters as of 2026-09-22. If the two ever diverge again, the Vietnamese version is authoritative until the next translation pass.

Component names, state labels, command names, and technical terms (`L1–L6`, `I1–I6`, `RC1–RC3`, `RLx`, `C1`, `PEAK_FIXED`, `GREEN`, etc.) are kept in English throughout, to match the code/diagrams/UML.

Because the original document is long, the specification is split into chapters under [`docs/specification/`](.). This file is just the **table of contents + shared reference material** (source-label table, naming conventions).

---

## Table of Contents

| Chapter | Content | Sections |
|---|---|---|
| [`01-vision-and-scope.md`](01-vision-and-scope.md) | Project vision, origin of requirements, design goals, scope (Core PoC / HD-target / optional) | Sections 1–4 |
| [`02-network-and-architecture.md`](02-network-and-architecture.md) | Physical network (including the 5b lane-selection analysis), system components, controller architecture, per-controller responsibilities, sensors, actuators | Sections 5–10 |
| [`03-operation-and-behavior.md`](03-operation-and-behavior.md) | Operating modes, traffic/pedestrian/railway behavior, railway pre-emption, congestion management | Sections 11–16 |
| [`04-safety-and-realtime.md`](04-safety-and-realtime.md) | Fault/safety behavior, Central behavior, local autonomy, timing/coordination strategy, real-time requirements, safety invariants | Sections 17–22 |
| [`05-features-and-demo.md`](05-features-and-demo.md) | Feature classification, demo scenarios, simulated inputs | Sections 23–25 |
| [`06-assumptions-and-summary.md`](06-assumptions-and-summary.md) | Assumptions, resolved open decisions, questions for the team/instructor, requirement traceability, final summary | Sections 26–30 |

Illustrative diagrams (ASCII, English): [`SYSTEM_DIAGRAMS.md`](../../SYSTEM_DIAGRAMS.md).

---

## Source labels (used throughout every chapter)

| Label | Meaning |
|---|---|
| **[REQ]** | Official requirement — stated directly in `RTS_Final Project.pdf` |
| **[CLARIF]** | Instructor clarification — stated in `lecture_clarification.md` |
| **[TEAM]** | Team decision — a hard call made to resolve a point the brief left open |
| **[ASSUM]** | Technical assumption — a value/behavior set to make the design concrete, with a stated rationale, and adjustable |
| **[HD]** | Proposed advanced (HD) feature — optional, added to strengthen the real-time/distributed-systems story |
| **[CONFIRM]** | Needs instructor confirmation — genuinely ambiguous relative to the official material |

If a decision was made at the team's own discretion (rather than being a hard requirement), it is additionally marked **Proposed — Awaiting Confirmation** so the team can revisit it later without having to dig up the original reasoning.

---

## Naming convention — read this first

The single biggest source of confusion in earlier drafts was conflating a **physical location** with the **controller** that manages it. The whole specification keeps these clearly separate:

| Physical thing (infrastructure) | Controller that owns it (logic/software) |
|---|---|
| `I1`–`I6` — 6 physical signalized intersections | `L1`–`L6` — 6 intersection **local controllers** (`Lx` controls `Ix`) |
| `RC1`–`RC3` — 3 physical railway crossings | `RL1`–`RL3` — 3 railway **local controllers** (`RLx` controls `RCx`) |
| `R1`–`R5` — 5 physical roads | *(no controller — roads are not controlled)* |
| Central control room | `C1` — **Central Controller** |

So: "`I1`" always means *the intersection as a place*; "`L1`" always means *the software/process controlling the signals at that place*. This matches the original architecture diagram in `README.md` (which already used `L1–L6` correctly from the start).

## Road-direction convention

The team set the project's real-world context in **Vietnam** (right-hand traffic), per Article 10 of Road Traffic Order and Safety Law No. 36/2024/QH15. The concrete example context cited is the Mai Chí Thọ–Đồng Văn Cống signalized intersection in the former District 2 area, with the layout recorded in H. D. Nguyen's thesis, Figure A-7. This is only a reference for realistic lane/signal context, not a claim that the assignment's topology or timing was measured at that intersection. Details and design consequences are in [`02-network-and-architecture.md`](02-network-and-architecture.md), Sections 5 and 5b.
