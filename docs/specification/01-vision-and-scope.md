[← Table of Contents](README.md) · Chapter 1/6 · Next: [Network and Architecture →](02-network-and-architecture.md)

---

## 1. Project Vision

We are building a **distributed, fail-safe traffic and railway-crossing control system**, not a centrally controlled light board. The core characteristic of the system is that **authority over physical outputs always resides locally** — nine local controllers, comprising 6 intersection local controllers `L1–L6` (each owning its own physical intersection `I1–I6`) and 3 railway local controllers `RL1–RL3` (each owning its own physical crossing `RC1–RC3`) — each controller owns its own set of physical devices and continues to operate safely, independent of the rest of the network. The 10th node, the **Central Controller `C1`**, has no actuation authority (no physical actuation) whatsoever — it exists only to *observe*, *coordinate*, *reconfigure within safety limits*, and *override within limits that must never violate a local safety invariant*.

What makes this project strong for EEET2588 is not the number of simulated intersections, but the fact that it produces real, demonstrable answers to the core questions of real-time systems: what happens when a message arrives late, when a link drops, when two safety events contend for the same actuator at once, and when a component dies partway through a critical processing chain. The railway crossing was deliberately chosen as the centerpiece of the safety argument (a genuine life-safety interlock: gate-confirmed-closed before train-proceed is allowed), while the 6 intersections serve as the vehicle for demonstrating distributed coordination (green-wave-style offsets), local autonomy, and command abstraction from the center.

The design deliberately excludes *traffic simulation* (modeling vehicle arrivals, statistical demand) from scope. This is a **control system** project: the *events* from sensors (simulated via keypresses, per [REQ]) are what the system reacts to, not a traffic-flow model that needs to be validated in its own right.

---

## 2. Origin of Requirements

| Source | Role |
|---|---|
| `RTS_Final Project.pdf` | The official source. Defines: 6 intersections, 5 roads, a double-track railway corridor running diagonally, boom gate + flashing red lights, minimum headway between trains, the principle that each local controller controls its own lights, Central only monitors/does not actuate, 3 groups of operating sequences (fixed / sensor / advanced), override capability, the multi-QNX-node requirement, the minimum Pass scope (`I1–I2` + crossing), and the recommendation to base the design on a real intersection. |
| `lecture_clarification.md` | An official supplement. Clarifies: a total of 9 local controllers (6 intersection + 3 railway, not "of the same kind"), railway local controllers exclusively own the boom gate/flasher/train-signal, intersection local controllers may only *sense* railway state, the requirement for an explicit train signal with fault reporting, the Central→Local command flow (never Central→light directly), sensors simulated via keypresses with an explicit keymap, distance-based green-wave-style coordination, the concept of reducing congestion caused by the railway, and the flexibility between QNX nodes and physical machines (1 machine can run multiple logical nodes; the class's general guidance is roughly 2–3 physical machines for a full demo session). |
| `idea.md` | **Not a design.** This is a brainstorm/question-listing document. It is used only to determine which questions are *important and high-impact* (answered explicitly below) versus questions of low value/premature to address (resolved implicitly with a reasonable default, without revisiting each one). |
| `README.md` | An early draft. Its architecture diagram (`L1–L6` / `I1–I6`, `RL1–RL3` / `RC1–RC3`, `C1`) and directory structure match the decisions below and are retained; its scope statements are superseded by this document. |

---

## 3. Design Goals

1. **Safety is structural, not incidental.** Every hazardous situation (two directions green at once, a train moving while the gate is open, a pedestrian mid-crossing) is prevented by an invariant enforced *locally*, never relying on trusting that a remote message will arrive correctly and on time.
2. **Local autonomy is real, not nominal.** Every local controller must demonstrably be able to operate fully safely when `C1` is turned off.
3. **Central's authority is monitoring + coordination + limited override only**, never direct actuation. This distinction must be visible in the IPC design, not just stated.
4. **Complexity must be justified.** Every feature beyond the minimum Pass level must tie to a specific real-time-systems teaching point (fault handling, coordination, timing analysis, degraded-mode operation) — features that only add surface area are explicitly excluded (Section 23).
5. **The design must scale by duplication, not by rewriting.** The Pass-minimum controller set (`L1`, `L2` at `I1`, `I2`, and `RL1` at `RC1`) must run the *same* general-purpose controller binary as `L3–L6` / `RL2–RL3`, differing only in configuration — this is precisely the intent of "the complete design should duplicate this part" [REQ].
6. **Every claim must be demonstrable.** No feature is included unless it can be directly triggered and observed within the 15-minute demo [REQ].

---

## 4. Scope

### 4.1 Full Conceptual System

The full design covers the entire network: 5 roads (`R1–R5`), 6 signalized intersections (`I1–I6`) each with its own local controller (`L1–L6`), 3 railway crossings (`RC1–RC3`) each with its own local controller (`RL1–RL3`), and 1 Central Controller (`C1`) — 10 logical controllers in total [REQ][CLARIF]. Every feature group described in this document (fixed/sensor/advanced sequences, pedestrian handling, the full railway lifecycle including faults, congestion reduction, coordination/green-wave, override, degraded-mode operation) belongs to the full conceptual design and is reflected in the Initial Design Report, state charts, and task architecture — regardless of whether it is actually implemented in code.

### 4.2 Planned Proof of Concept

**Core PoC (definitely being built, meeting the minimum Pass level [REQ]):**
Intersections `I1`/`I2` (controllers `L1`, `L2`), connecting road `R3`, railway crossing `RC1` (controller `RL1`), and `C1`. This is a fully functional "one-third slice" — real QNX processes, real IPC, a real railway interlock, real degraded-mode behavior.

**HD-Target extension — finalized [TEAM, finalized on 2026-08-22]:**
The team is targeting HD, not the Pass level, so this extension is now part of the build plan rather than a "if time permits" item. Duplicate the Core slice once more along an arterial axis to add `I3`/`I4` (controllers `L3`, `L4`), road `R4`, and railway crossing `RC2` (controller `RL2`) — chosen specifically because a single slice alone cannot demonstrate arterial coordination (green-wave-style offsets require at least 2 consecutive intersections on the same road, e.g., `I1–I3` on `R1`). This extension is precisely what elevates the demo from "one working pair of intersections" to "a genuinely coordinated distributed controller network" — this is what actually creates the HD differentiation.

**Full 9-controller build (`I5`/`I6` via `L5`/`L6`, `R5`, `RC3` via `RL3`):** Optional/stretch — see section 4.3. Because the controller software is general-purpose and configuration-driven (Goal 5, Section 3), the *design* already covers this fully; whether a third instance is actually stood up at demo time is a resourcing decision, not a design gap.

### 4.3 Optional Scope

Design of dedicated turn lanes + protected arrow signals (see the analysis in Section 5b), a graphical user interface (GUI) beyond terminal text [REQ permits terminal-only], statistical/random modeling of vehicle arrivals, persistent data logging beyond the `/fs` usage already permitted for test vectors [REQ], and the third network slice (`L5/L6` at `I5/I6`, `RL3` at `RC3`) are optional/stretch — see Section 23 for the full classification and rationale.

---

[← Table of Contents](README.md) · Next: [Network and Architecture →](02-network-and-architecture.md)
