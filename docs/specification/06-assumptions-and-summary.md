[← Table of Contents](README.md) · Chapter 6/6 · [← Features and Demo](05-features-and-demo.md)

---

## 26. Assumptions

| # | Assumption | Rationale | Design Consequence | Instructor confirmation needed? |
|---|---|---|---|---|
| 1 | Drive on the right (right-hand traffic, Vietnamese convention); the lane next to the median = left+through, the lane next to the curb = right+through | The project's real-world context is Vietnam; the original brief does not mandate choosing a specific country/convention, it only encourages basing the design on a real intersection | Determines the drawing orientation of Diagram 2 and the lane-assignment approach in Section 5 | No |
| 2 | Arterial speed 60 km/h; illustrative segment distances: `I1–I3=350m`, `I3–I5=400m`, `I2–I4=320m`, `I4–I6=380m` | The speed matches the current maximum applicable to this class of qualifying urban road under Circular 38/2024/TT-BGTVT. The brief and the lecture clarification require distance or travel time to feed into deriving coordination, so concrete, adjustable numbers are used instead of only stating a formula. | Feeds the coordination offsets computed in Section 20: 21s, 24s, 19s, and 23s | No |
| 3 | Min green 8 seconds, max green 40 seconds, yellow 4 seconds, all-red 2 seconds | A typical technical range for urban traffic signal timing, not tied to any specific national standard | Bounds every phase timer in the design | No |
| 4 | `PEAK_FIXED` cycle exactly 90 seconds: Phase A (arterial) 48s green+4s yellow+2s all-red=54s; Phase B (connector) 30s green+4s yellow+2s all-red=36s; not deliberately made an exact divisor of the 120s train headway, since pre-emption is event-driven and does not depend on cycle boundaries | A representative mid-size intersection cycle; the 48:30 green split matches arterial priority | Basis for fixed-mode timing and green-wave offsets (Section 20) | No |
| 5 | Railway warning lead time ≈ 45 seconds, derived from lead-flash + gate-close + safety margin | The brief gives no number; the value is derived from a safety-oriented method rather than an arbitrary invented figure | Determines the approach-sensor placement distance and the pre-emption response budget | No — flagged as adjustable |
| 6 | Fixed crossing-occupancy interval ≈ 20 seconds, identical for every train | Simplifies the reopen logic into a timer instead of requiring exit-sensor confirmation, per the team's decision | Gate reopening (Section 14) uses a timer; the exit sensor is reserved for later, not part of Core | No — flagged as adjustable; the trade-off is documented (an abnormally slow or stuck train is not detected within Core scope) |
| 7 | Core uses 2 lanes/direction, permissive turns, no arrow signals (instead of 3 protected lanes) | Fully analyzed in Section 5b: the schedule risk + increased cycle length (UX impact) of the 3-lane option outweigh the safety benefit at the left-turn point, given the scope of this course's assessment | Keeps the Core actuator/sensor/conflict-matrix simple; the 3-lane option is recorded as a future upgrade direction, not part of Core | No — an internal team decision |
| 8 | The system does not simulate specific vehicles or collisions between vehicles — it only simulates sensor events (keypresses) and light states | The PoC only displays light state via the terminal [REQ]; no vehicle/driver is simulated. Collision risk from permissive left turns depends on real-world right-of-way rules, which falls outside the scope of what the system can simulate or be responsible for (see Section 5b) | Confirms Core keeps the 2-lane permissive design; the demo will never depict a "vehicle" or an "accident," only valid light/sensor states | No |
| 9 | Only 2 demand profiles, Peak/Off-Peak | Sufficient to verify every mode transition without needing additional unfounded figures | Simplifies `SET_MODE` and time-of-day auto-selection | No |
| 10 | No statistical/random modeling of vehicle arrivals; sensors are purely event-driven | The project is a control system, not a traffic simulator | The sensor input model is based entirely on keypress events | No |
| 11 | 1-second heartbeat, link considered lost after 3 consecutive misses | Fast enough for a time-compressed demo, following the standard missed-N pattern | Basis for the timing of entry into `DEGRADED_LOCAL` | No |
| 12 | Override capped at 5 minutes, auto-expiring | Prevents a forgotten override from permanently altering operation | Bounds the `CENTRAL_OVERRIDE` mode | No |
| 13 | Pedestrian crossings exist only at intersections, not at railway crossings | The brief places pedestrian crossings only "at each intersection" | Simplifies the pre-emption/pedestrian interaction analysis | No |
| 14 | The demo topology remains generic and parameterized; the Mai Chí Thọ–Đồng Văn Cống intersection is used only as a contextual real-world reference | Satisfies the intent of the brief without claiming that the illustrative topology or timing was surveyed at that site | Keeps the brief's topology unchanged while making the real-world reference traceable | No, by design |
| 15 | Demo deployment uses 2 physical machines (Section 27) | Fewer hardware/network points of failure during a live demo, which was weighed; a QNX node is logical and location-independent, so 2 machines can still run every separate process required | The deployment diagram targets 2 machines; it can be extended to 3 later without any code changes | No |
| 16 | A train approach sensor that goes completely silent ("dead") **cannot be detected** by the Core design — only a sensor stuck *active* can be detected (Section 17) | The Core sensor design has no heartbeat/self-test signal independent of the detection line, so a dead sensor is indistinguishable from "no train currently present"; claiming otherwise would overstate the capability of a simple sensor | A silently dead sensor leaves the crossing unable to actively warn (fail-open, not fail-safe) — an openly acknowledged residual risk, not something the system claims to solve; the remedy (adding a periodic self-test/heartbeat signal) is outside Core | No — an internal scope clarification |

---

## 27. Previously Open Design Decisions — Finalized on 2026-08-22

All 3 items below were previously open in an earlier draft; the team has now decided:

1. **Commitment to the HD extension for the PoC — FINALIZED: yes, it will be built.** The team is targeting HD. The `L3`/`L4`/`RL2` extension (Section 4.2) is part of the build plan, not a stretch goal. Green-wave coordination (Section 20) is therefore a feature that *is actually demoed*, not just designed on paper.
2. **Final number of physical machines — FINALIZED: 2 machines recommended.** Rationale: a QNX "node" is a logical/software concept, not a physical one — message passing over Qnet works identically whether 2 processes sit on the same machine or on different machines. So 2 physical machines can still run every controller process on its own separate QNX node, fully satisfying the "multiple QNX nodes with separate processes" requirement [REQ]. A third machine would only add real risk on demo day (more hardware, more network configuration, one more thing that can fail at boot/connect time) without adding any capability the actual requirement needs. If the team later wants a demo that is more visually convincing as "obviously 2 separate boxes," moving to 3 machines is only a deployment-configuration change, not a design change.
3. **Watchdog/supervisor process on each node — FINALIZED: yes, it will be built, since the team is targeting HD.** In simple terms: this is a very small, auxiliary QNX process that runs on each node with exactly one job — "check whether the real controller process on this node is still responding; if not, immediately force that node's output into the defined safe state (Section 10)." It is cheap to build, does not interact with the rest of the traffic logic at all, and directly demonstrates a genuine real-time monitoring pattern (Section 17, Section 23). The specific scheduling (which sprint it is built in) is up to the team.

**Additional decisions — finalized in the same diagram/lane update round:**

4. **Driving convention — FINALIZED: Vietnam, right-hand traffic.** See Section 5.
5. **Core lane choice — FINALIZED: 2 permissive lanes, no arrow signals.** The 3-lane protected option is recorded as a possible future upgrade direction (Section 5b) — not for this course.
6. **The PoC does not simulate vehicles/vehicle collisions — FINALIZED.** The system only simulates sensor events and light states; no "vehicle" or "accident" is ever demoed (see Assumption #8, Section 26).

---

## 28. Questions for the Team / Instructor

**Short answer: nothing in this specification currently requires instructor approval.** The only condition the instructor raised for needing approval is *changing the given map/topology* (e.g., changing the number of lanes or the intersection layout). This design does not do that — Section 5 only assigns names and orientation to roads that were already implied in the official diagram and the minimum-Pass hint; choosing the right-hand-traffic convention and the Vietnam real-world reference likewise are not a topology change. Every numeric timing value in Section 26 is presented as a derivable/adjustable technical assumption, not a requirement statement, so it also does not need approval.

The only remaining thing that needs "confirming with someone" is purely a team preference, not a question for the instructor: if the team later wants the report to reference a specific, named real intersection in Vietnam (instead of the generalized real-world parameterization in Assumption 13), that is just a brief internal decision — no need to email the instructor about it.

---

## 29. Traceability

| Feature | Source |
|---|---|
| 6 intersections, 5 roads, the railway corridor, boom gate/flasher, local self-control of lights, Central monitoring only, 3 groups of operating sequences, override, multi-node QNX, the Pass-minimum slice | [REQ] |
| A total of 9 local controllers, `RLx` exclusively owning railway devices, the requirement for a train signal + fault reporting, the expected simulation keymap, guidance for a 2–3 machine deployment | [CLARIF] |
| The separation of `Lx`/`Ix` and `RLx`/`RCx` naming, the approach for mapping topology to road names (Section 5), the right-hand-traffic convention, the Core 2-lane-permissive lane decision (Section 5b), the 2-phase intersection model, the parallel-walk pattern for pedestrians, the priority hierarchy, command-abstraction details, the degraded-mode resync approach, the decision to use a timer for crossing occupancy, the list of excluded features | [TEAM] |
| Every numeric timing value, sensor placement distances, heartbeat/timeout figures, the fixed crossing-occupancy interval | [ASSUM] |
| Advance/queue sensors + congestion-driven demand suppression, green-wave coordination, the watchdog/supervisor process, the capped auto-expiring override, local rejection of unsafe commands | [HD] |

---

## 30. Final Proposed System Summary

The system is a distributed traffic-and-railway control network that puts safety first: 6 intersection local controllers (`L1–L6`, each corresponding to one physical intersection `I1–I6`) and 3 railway local controllers (`RL1–RL3`, each corresponding to one physical crossing `RC1–RC3`) all retain exclusive authority, can operate offline with respect to their own physical devices, and are coordinated — but never commanded at the light level — by a single Central Controller (`C1`) that can only monitor, reconfigure within safety limits, and issue a bounded override which the local controller has the authority to refuse. The network is set in a Vietnamese context (right-hand traffic), with topology `R1`/`R2` (arterial, West–East) and `R3`/`R4`/`R5` (connector, North–South, crossing the railway corridor at `RC1`/`RC2`/`RC3`). The railway crossing is the centerpiece of the safety argument (gate-confirmed-closed before train-proceed is allowed, a fixed occupancy-window timer governing safe reopening, fail-to-safe under any ambiguity), while the 6-intersection network is the centerpiece of the coordination/distribution argument (local 2-phase phasing with permissive lanes, sensor-driven off-peak behavior, green-wave offsets along the arterial, and a structural anti-starvation mechanism). The proof-of-concept that will definitely be built is the Pass-minimum controller set `L1`/`L2`/`RL1` (at `I1`/`I2`/`RC1`) plus `C1`, built from the same general-purpose, configuration-driven controller software — genuinely duplicable (not just a promise) across the full 6-intersection, 3-crossing network. The team is committed to building the HD-target extension (`L3`/`L4`/`RL2` at `I3`/`I4`/`RC2`), which is precisely what turns multi-intersection coordination and congestion-driven demand suppression into a demo that is genuinely real rather than merely theoretical, deployed across 2 physical machines to keep demo-day risk low while still satisfying the multi-QNX-node requirement. The dedicated turn-lane + protected arrow-signal design (Section 5b) has been fully researched and documented as a well-founded upgrade option, for `I1`/`I2` should the team have spare time remaining after completing the committed HD scope.

---

[← Table of Contents](README.md) · [← Features and Demo](05-features-and-demo.md)
