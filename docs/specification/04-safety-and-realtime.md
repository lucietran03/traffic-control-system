[← Table of Contents](README.md) · Chapter 4/6 · [← Operations and Behavior](03-operation-and-behavior.md) · Next: [Features and Demo →](05-features-and-demo.md)

---

## 17. Fault and Safety Behavior

| Fault | Response |
|---|---|
| Gate does not confirm `CLOSED` before the deadline while a train is approaching | `FAULT_SAFE` (crossing): `RLx` immediately forces/holds the train signal at `STOP`, **simultaneously (not as a follow-up step, not as a precondition)** reporting the fault to `C1`; the approach direction toward the crossing is held red until the fault is cleared manually. `RLx` never waits for `C1` to acknowledge before forcing `STOP` (Section 18). |
| Train-approach sensor stuck **active** (keeps reporting `TRAIN_APPROACHING` and never clears) | **Detectable.** If the signal remains active far longer than the derived lead-time-plus-occupancy budget (Section 14) without any train being confirmed as having passed, `RLx` treats this as an anomaly, reports a fault, and fails toward the safe direction — holding the crossing at `WARNING`/gate down until the operator clears the fault. This matches real-world practice: level-crossing equipment always fails toward blocking traffic. |
| Train-approach sensor **goes completely silent** (stops reporting signals, "dies") | **Accepted limitation — the Core design does NOT detect this.** A sensor that has completely stopped reporting looks, from `RLx`'s point of view, identical to "no train is currently approaching" — the Core sensor design has no independent heartbeat/self-test signal to distinguish these two situations. This means a fully dead sensor silently strips that crossing of its proactive train-warning capability — this is a **fail-open** direction, not fail-safe, and the design does not claim to detect it. This is stated plainly rather than silently glossed over; the identified remediation (adding a periodic self-test/heartbeat signal independent of the train-detection signal path) is out of scope for the Core (Section 26). |
| Gate stuck open while a train is approaching | The train signal is forced to `STOP`; `C1` raises a **critical** alarm; adjacent intersections only receive a fault notification to log the situation — the specification states plainly that **no software action here can physically stop the train**; this is a genuine limitation, not something the system claims to remedy |
| Gate stuck closed, no train | Reported to `C1` as a nuisance fault; the train signal keeps its safe default; vehicle traffic simply waits (safe, merely inefficient); requires the operator to clear the fault manually after the physical repair — `C1` cannot force the physical gate to move (Safety Invariant: no remote override of railway equipment) |
| Vehicle sensor or pedestrian button stuck active | Detected and reported as a local fault. The vehicle sensor is treated as continuous demand; repeated button signals are coalesced into a single pending pedestrian request. |
| Vehicle sensor or pedestrian button permanently silent | Accepted limitation — undetectable without an independent heartbeat/self-test, since silence cannot be distinguished from no vehicle present or no one pressing the button. If the fault is reported by an explicit diagnostic or fault injection, the corresponding demand input fails to the asserted state. |
| Local controller software fails | Fails to the actuator's safe state (Section 10) — implemented in the PoC via **[HD]** a lightweight QNX watchdog/supervisor process on each node (Section 23) that detects the controller process has died and forces outputs to their safe state |
| Loss of communication, `Lx ↔ C1` | `DEGRADED_LOCAL` (Section 11) |
| Loss of communication, `Lx ↔ Lx'` (coordination between peers) | All coordination features (green-wave offset) degrade to standalone timing; the core safety behavior of both controllers is entirely unaffected — no controller ever blocks waiting on another for safety reasons |

---

## 18. Central Controller Behavior

`C1` monitors the state, timing profile, faults, and railway status of all **9 local controllers** (`L1-L6` + `RL1-RL3` — `C1` does not monitor itself, hence 9, not the network's total of 10 logical controllers), and issues 3 command groups: `SET_MODE`, `SET_TIMING_PROFILE`, `REQUEST_OVERRIDE` [REQ/CLARIF — a "command abstraction," never a raw light color]. Each command receives one of the following outcomes: `ACK` (accepted, applied immediately), `ACK_ACCEPTED_PENDING` (accepted but must wait through a guard interval or a safe phase boundary before being applied — e.g., a `REQUEST_OVERRIDE` arriving while pedestrian clearance is running), `NACK` (rejected for safety reasons, with a specific reason), or `ERROR` (could not be processed, e.g., a malformed command — differing from `NACK` in that `NACK` is a valid command judged unsafe, whereas `ERROR` means the command itself could not be processed). The local controller must respond (`ACK`/`ACK_ACCEPTED_PENDING`/`NACK`/`ERROR`) within **1 second** of receiving the command — even when the outcome is `ACK_ACCEPTED_PENDING`, *acknowledging receipt of the command* never waits until the command is actually applied; a deferred request is re-validated immediately before it is triggered **[ASSUM, PA-12]**. `NACK` means the local controller has judged that command to be unsafe (e.g., an override request while railway pre-emption is under way at that intersection), and this is displayed to the operator as a rejected/errored command — it is never silently dropped.

`C1` decides the time-of-day mode for the entire network as the default source of truth (to keep things synchronized during the demo), but every local controller still keeps and uses its own fallback clock as soon as `C1` becomes unreachable (Section 11) — this is precisely what lets "Central decides the mode" and "local must remain safe without Central" both hold true at the same time.

`REQUEST_OVERRIDE(CLEAR_ROUTE, target, duration)` **[TEAM]**: sent to the local controller `Lx` of the target intersection; forces a specified direction to green for a bounded duration (default maximum 5 minutes **[ASSUM]**, automatically expiring back to the prior mode if not renewed — preventing a forgotten override from permanently breaking normal operation). It is rejected outright if the target intersection currently has railway pre-emption under way/imminent on that direction, and it never interrupts an ongoing pedestrian clearance (Section 22).

**Railway fault reporting never routes around `C1` [TEAM].** When `RLx` detects a railway equipment fault (Section 17), forcing the train signal to `STOP` and reporting the fault to `C1` are 2 **independent, parallel** actions — `RLx` never waits for `C1` to acknowledge before forcing `STOP`. This matches the instructor's description (the train receives the stop signal immediately when a fault occurs; the fault is *then* reported to the control room afterward) and preserves the local-autonomy principle (Section 19) for the system's single most important safety decision. `C1`'s only role after receiving the fault is to display it, and once the physical repair is complete, to send a validated fault-clear request to `RLx` (Section 8.1) — `RLx` still retains the right to refuse if the fault condition has not actually cleared.

---

## 19. Local Autonomy

Every safety-related decision made by `Lx` or `RLx` — non-conflicting phase assignment, clearance timing, railway pre-emption response, gate-confirmed-before-proceed — is computed **entirely locally**, with no runtime dependency on whether `C1` is reachable. When the link to Central is lost:

- It does **not** halt sensor-driven operation (`OFF_PEAK_SENSOR` continues to use purely local sensor input).
- It does **not** halt railway protection (`RLx`'s logic does not depend on Central at any point in its state machine).
- It puts the controller into `DEGRADED_LOCAL`: **retaining the most recent valid timing parameters** (green/yellow/all-red durations, extension caps, coordination offset) rather than reverting to an arbitrary hardcoded default — but **the mode (Peak/Off-Peak) is not frozen at the "last known mode"**: from the moment connectivity is lost, the controller's local clock becomes the sole authority for switching `PEAK_FIXED ↔ OFF_PEAK_SENSOR`, exactly as when `C1` was present, just using the most recently validated timing profile for whichever mode is currently selected.
- When reconnected, **the local controller pushes its entire current actual state up to `C1`** (not the other way around) — this avoids a window in which `C1` could apply a stale command to a state that has since changed.

Loss of the peer-to-peer link between 2 local controllers (used only for optional coordination, Section 20) never affects the core safety behavior of either controller — coordination gracefully degrades to standalone; it is never a precondition for maintaining safety.

---

## 20. Timing and Coordination Strategy

**A method, not made-up numbers [ASSUM]:** the cycle length, phase-split ratios, and offsets are derived from the distance/speed assumptions in Section 5, following the standard approach the assignment itself suggests (measure the distance between intersections, assume a travel speed, derive the time for a platoon of vehicles to reach the next light, shift that light's green-start time by that travel time — i.e., a green wave). The final values are **tunable parameters**, not a claim of verified real-world figures.

**Concrete worked example [ASSUM]** (the assignment mentions twice — both in the original brief and in the lecture clarification — that distances must be measured/estimated to derive timing, so here we work the numbers out fully rather than just stating the formula): assume distances `I1–I3 = 350m`, `I3–I5 = 400m` on `R1`; `I2–I4 = 320m`, `I4–I6 = 380m` on `R2`; arterial speed 60 km/h = 16.67 m/s.
- Offset `L1→L3` = 350 / 16.67 ≈ **21 seconds**
- Offset `L3→L5` = 400 / 16.67 ≈ **24 seconds** (45 seconds total from `L1`)
- Offset `L2→L4` = 320 / 16.67 ≈ **19 seconds**
- Offset `L4→L6` = 380 / 16.67 ≈ **23 seconds**

**Coordination scope [TEAM]:** offsets are computed only for the arterial chains (`I1–I3–I5` on `R1`, `I2–I4–I6` on `R2`) — coordinating the connectors (`R3/R4/R5`) across the railway corridor was considered and rejected, because these roads are short, have low continuity, and their dominant timing factor is railway pre-emption, not platoon progression (Section 23).

**Important note — these are 2 DIFFERENT, unrelated notions of "coordination":** the `TC` coordination above is coordination among **road intersections** along the same arterial axis (`Lx`). `RL1`, `RL2`, and `RL3` (the 3 railway crossings), on the other hand, are **entirely independent of one another** — they share no state, and there is no model of "1 train running diagonally triggers all 3 crossings in sequence." Each `RLx` only knows about and reacts to a train at its own crossing (see Section 8.3).

**Who computes it [TEAM]:** `C1` computes and distributes the offset profile (it has the network-wide view the calculation requires), but each `Lx` applies the offset to its own local sequencer — so coordination is simply a *timing parameter* delivered through the same `SET_TIMING_PROFILE` command group as everything else, not a new command type or a new authority granted to `C1`.

**Coordination fault [TEAM]:** any `Lx` that loses its offset input (Central unreachable, or a peer's report has gone stale) falls back to standalone fixed/sensor timing — it never blocks waiting on a peer.

**Green-wave disruption due to railway events [TEAM]:** after a pre-emption event, the affected `Lx`'s phase clock keeps running, and the next `SET_TIMING_PROFILE` it receives (or the state push upon reconnection) re-establishes the offset — the design **does not** attempt to build a dedicated resynchronization phase, since doing so would only add a state machine with no independent safety/demo value beyond "coordination eventually recovers," which the normal code path already guarantees (Section 23 — Rejected: gradual resync algorithm).

---

## 21. Real-Time Requirements

| Event | Trigger | Nature | Deadline (target) | Behavior on miss |
|---|---|---|---|---|
| Start of railway safety sequence | Approach sensor triggers | Hard, aperiodic | Local reaction within 200 ms; adjacent-intersection notification within 500 ms end-to-end (a very wide margin against the ~45-second physical lead-time budget, Section 14) | N/A — this is the single deadline the entire design is built to never miss; the local reaction is a simple sensor-to-actuator path with no remote dependency |
| Gate-closed confirmation before train-proceed | Gate position sensor | Hard | Must confirm before the derived deadline (Section 14) | Train signal defaults to `STOP`, fault reported (fail-to-safe, never fail-to-proceed) |
| Phase sequencer tick | Internal timer | Soft, periodic | 100 ms tick, ±50 ms jitter tolerance | Only affects cosmetic smoothness; no safety impact |
| Pedestrian button press → latch registration | Button press | Soft | < 500 ms (human-perceptible threshold) | Only delays acknowledgment; the request is never lost |
| Light state change → report to `C1` | Any light/pedestrian/mode state change | Firm — explicitly required by the assignment [REQ] | < 200 ms | Logged as a sign of communication degradation; no impact on local safety |
| Heartbeat | Periodic liveness | Firm, periodic | 1-second cycle; 3 consecutive misses (~3 seconds) ⇒ link declared lost | Triggers `DEGRADED_LOCAL` |
| Central command → `ACK`/`ACK_ACCEPTED_PENDING`/`NACK`/`ERROR` (Section 18, PA-12) | `SET_MODE`/`SET_TIMING_PROFILE`/override | Soft/firm | < 1 second — this deadline is for **acknowledging receipt of the command**, not for the command to finish being applied (`ACK_ACCEPTED_PENDING` still counts as an on-time response) | 2 retries, after which `C1` marks the controller unreachable/raises a communication alarm |
| Fault detection → alarm | Any sensor/gate/hardware fault | Hard (it is itself a safety report) | Faster than the normal status path — sent immediately, never batched | N/A — fault reporting is never rate-limited |
| Override applied | `REQUEST_OVERRIDE` accepted | Soft, deliberately bounded by safety, not instantaneous | Applied at the next safe phase boundary (no fixed deadline, by design) | N/A — by design: an override is *never* applied instantaneously if doing so would skip a clearance interval |

**Demo time scaling [TEAM]:** a single global, configurable speed-up factor is applied uniformly to every timer in the system (phase durations, pedestrian intervals, railway lead time, heartbeat/timeout thresholds) so that the relative safety margins are preserved at demo speed — scaling only some timers (e.g., traffic but not railway) was considered and rejected, since it would distort exactly the safety margins the demo needs to demonstrate.

---

## 22. Safety Invariants

These are the rules that the state chart, task architecture, and test scenarios must all be traceable back to:

1. No 2 conflicting vehicle directions at the same intersection may receive `GREEN` simultaneously (as determined by that intersection's static conflict matrix).
2. No pedestrian `WALK` may be activated simultaneously with a `GREEN`/`YELLOW` vehicle direction that conflicts with that crossing.
3. Every transition between conflicting directions must pass through `YELLOW` and then `ALL_RED`, for at least the configured minimum interval, before the next conflicting direction receives `GREEN`.
4. Once a `WALK` phase has begun, it must complete its full clearance before that crossing again serves a conflicting vehicle `GREEN`.
5. The train signal never displays `PROCEED` unless that crossing's boom gate has been sensor-confirmed `CLOSED`.
6. If the gate's closed status cannot be confirmed before the required deadline, the train signal defaults to `STOP` and a fault is reported — any ambiguity is always resolved toward the safer state, never "assumed fine."
7. Only the owning `RLx` may actuate its boom gate, flashing lights, or train signal — no other controller, including `C1`, may issue an actuation command to railway equipment.
8. The gate must not reopen while any train's computed crossing-occupancy window (Section 14, step 6) has not yet expired, in either direction — including when a second train is detected while the first is still occupying the crossing.
9. A direction leading straight into a crossing must not display `GREEN` while that crossing is `CLOSING`, `CLOSED`, `TRAIN_PRESENT`, or `FAULT`.
10. Every local controller must be able to run its full safety-related logic (phase assignment, clearance, railway response) when `C1` is unreachable.
11. A command from `C1` that violates any invariant above must be rejected and reported by the receiving controller — it must never be silently applied.
12. An override must never interrupt an ongoing pedestrian clearance, and must never command a direction toward a crossing when that crossing is not yet `OPEN`.
13. The loss of a single sensor or a single communication link may degrade functionality (e.g., falling back to fixed timing, or to standalone coordination) but must never, by itself, create an unsafe output state.

### Priority Order When Multiple Conditions Occur Simultaneously — **[TEAM, finalized 2026-08-23]**

**Transparency note:** the previous draft borrowed a ranking suggestion from `idea.md` almost verbatim. But as the team pointed out, `idea.md` is merely a list of prompting questions generated by a different AI to spark ideas — it was never confirmed or verified by the team against the system's actual behavior, so using it as the basis for a core safety section was not sufficiently rigorous. The section below has been **re-derived from scratch**, by examining each pair of conditions to see whether they genuinely contend for the same output, based on the concrete behavior described in Sections 11–21.

**Step 1 — These are the genuinely mutually exclusive MODES (a controller is in exactly one of these states at any given time):**

```text
1. FAULT_SAFE                    — does not actually "rank" alongside the other modes; this is
                                    an absolute local veto, applied when the controller can no
                                    longer trust its own output
2. RAILWAY_PREEMPTION            — overlays the running mode, but ONLY affects the approach
                                    direction toward the crossing (Section 15), not the whole
                                    intersection
3. CENTRAL_OVERRIDE              — can overlay the background mode, but is REJECTED if it
                                    conflicts with an ongoing RAILWAY_PREEMPTION (Section 18) —
                                    so it always ranks below item 2
4. PEAK_FIXED / OFF_PEAK_SENSOR  — the background mode, running when none of the 3 items above
                                    apply
```

(`DEGRADED_LOCAL` sits outside this axis — it describes "whether Central is still reachable," not a hazard that needs ranking.)

**Step 2 — These are CONSTRAINTS that apply to EVERY mode in Step 1, not a separate competing "mode" — ranking them as their own priority tiers (as the old draft did) is incorrect:**

- **An ongoing pedestrian clearance must always be allowed to run to completion** (Invariants #4, #12), regardless of which mode is active. On close examination of the operation as described in Section 15: `RAILWAY_PREEMPTION` only forces **one specific vehicle direction** (the one heading toward the crossing) to red — it never needs to turn green a direction that conflicts with an ongoing pedestrian WALK. So, in practice, pedestrian clearance **never actually contends** with `RAILWAY_PREEMPTION` — this is a constraint that always holds, not a tier to be ranked above or below item 2.
- **Halting congestion demand intake + the drain phase (Section 16)** is *internal* behavior of `PEAK_FIXED`/`OFF_PEAK_SENSOR`, not a separate mode standing on equal footing to compete with `CENTRAL_OVERRIDE`.
- **Pedestrian requests and vehicle sensor demand are treated EQUALLY** when scheduling phases — this is explicitly stated in Section 13 ("a latched request is treated exactly like vehicle demand when scheduling"). The old draft ranked these as 2 separate tiers (items 6 and 7, with pedestrians ranked above vehicles) — **this directly contradicts Section 13** and has been corrected here.

**Official conclusion (finalized):**

```text
MODE priority axis (mutually exclusive):
  1. FAULT_SAFE                      - absolute local veto
  2. RAILWAY_PREEMPTION              - only affects the approach direction toward the crossing
  3. CENTRAL_OVERRIDE                - rejected if it conflicts with item 2
  4. PEAK_FIXED / OFF_PEAK_SENSOR    - background mode

Constraints applying to EVERY mode above (not a separate priority tier):
  - An ongoing pedestrian clearance is never interrupted
  - Halting congestion demand intake + the drain phase is internal logic of the background mode
  - Pedestrian requests and vehicle sensor demand are treated equally
```

The team has confirmed: pedestrians only cross at intersections, never across the railway tracks, so pedestrian WALK and `RAILWAY_PREEMPTION` have no scenario that genuinely conflicts directly. The team has also confirmed the contradiction with Section 13 (the old draft ranked "pedestrian requests" above "vehicle sensor demand," while Section 13 states clearly that the two are treated equally) is real and has been correctly fixed in this finalized version.

---

[← Table of Contents](README.md) · [← Operations and Behavior](03-operation-and-behavior.md) · Next: [Features and Demo →](05-features-and-demo.md)
