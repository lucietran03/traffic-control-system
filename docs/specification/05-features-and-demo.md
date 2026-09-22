[← Table of Contents](README.md) · Chapter 5/6 · [← Safety and Real-Time](04-safety-and-realtime.md) · Next: [Assumptions and Summary →](06-assumptions-and-summary.md)

---

## 23. Feature Classification

### Core (mandatory to meet the assignment requirements)

- Design of 6 intersections + railway crossings [REQ]
- A local controller for each intersection, operating continuously [REQ]
- A central controller with monitoring/status/commands [REQ]
- Each local controller drives its own lights and survives loss of the central controller/communication [REQ]
- 3 groups of operating sequences: fixed / sensor-driven / advanced [REQ]
- Boom gate + flashing lights + train signal, fail-to-red [REQ][CLARIF]
- Sensors simulated via keypresses [REQ]
- Multiple QNX nodes with separate processes [REQ]
- Terminal-based status display [REQ]
- Pass-minimum PoC controller set (`L1`, `L2`, `RL1` at `I1`, `I2`, `RC1`) [REQ]

### HD-Target — LOCKED IN, WILL BE BUILT (per Section 4.2, not optional)

- The `L3`/`L4`/`RL2` extension (Section 4.2)
- Advance/queue sensor + demand-hold logic/drain phase (Section 16) — directly answers the assignment's "state and justify... congestion control" requirement
- Green-wave coordination along the arterial (Section 20) — substantive distributed-coordination content + real timing analysis
- Local authority to reject unsafe commands with a NACK response (Section 18) — a concrete, demonstrable safety argument
- Watchdog/supervisor process on each node (Section 17) — a small QNX process that checks whether the main controller is still alive, and if not, immediately forces outputs to a safe state
- Limited, self-expiring override from Central with clear dependency rules (Section 22)

### Optional — TO BE DONE LATER if time permits (not yet committed, NOT cancelled)

These are ideas the team **has not decided to drop** — they are simply not part of the mandatory build plan right now. If the HD-Target items above are finished early, this is the priority list for what to build next:

- Extend the PoC to the full 9-controller network (`L5/L6` at `I5/I6`, `RL3` at `RC3`)
- Dedicated turn-lane design + protected arrow signals for `I1`/`I2` (Option B, see Section 5b)
- A graphical interface (GUI) instead of terminal-only
- Real-time exit-sensor confirmation in place of a fixed timer (see the accepted trade-off in Section 14) — the physical sensor is already present in the design (Section 9, section 5), it is simply not yet wired into the logic

### Removed Entirely — will NOT be done, not even later (final decision, different from the Optional section above)

| Idea | Reason for removal |
|---|---|
| Statistical/random vehicle-arrival simulation | This is a *control system* project; sensor events (simulated via keypresses per [REQ]) are the correct input model — building a traffic simulator adds an entire separate subsystem that would need to be verified, with no marks awarded for it |
| A "scramble" (all-red) phase for pedestrians | Ordinary signalized intersections default to parallel walk; scramble adds a separate conflict-matrix mode with no basis in the requirements |
| Per-lane signals/sensors (within Core) | The assignment only requires 2 lanes per direction with a turn lane that "may have" [a signal] — per-lane detail multiplies complexity with no corresponding requirement; full trade-off analysis in Section 5b |
| A 4-time-window demand model (morning/midday/afternoon/night) | 2 windows (Peak/Off-Peak) are already sufficient to verify every mode-switching and coordination mechanism; adding a fourth window is just more numbers to explain, not more design |
| A directional-demand-skew / burst-arrival model | No sensor/actuator behavior depends on modeled arrival statistics — this is purely simulation realism with no effect on the control logic being assessed |
| A gradual green-wave resynchronization algorithm after a train event | The normal recovery/reprofile path already restores coordination; a separate resync phase would be a second state machine solving a problem the first state machine has already solved |
| Local-to-local coordination for connectors crossing the railway | The connectors are short and dominated by railway pre-emption; coordinating their timing has no significant travel benefit worth modeling |
| `C1` overriding railway equipment in an emergency situation | Explicitly removed for safety reasons — no remote authority may be permitted to bypass the gate-confirmed-closed interlock; consistent with the very recommendation the instructor stated |

---

## 24. Demo Scenarios

| # | Scenario | What It Verifies |
|---|---|---|
| 1 | Normal `PEAK_FIXED` operation at `I1`/`I2` (`L1`/`L2`) | Core 2-phase sequence, clearance interval |
| 2 | `OFF_PEAK_SENSOR` reacting to a simulated vehicle event | Sensor-based extension, default rest state on the major road |
| 3 | Pressing the pedestrian button → WALK → clearance → resume | The full pedestrian lifecycle, the latch mechanism |
| 4 | `SET_MODE`/`SET_TIMING_PROFILE` command from Central applied only at a safe boundary | Command abstraction, correct application at the safety boundary |
| 5 | Full train-approach sequence at `RC1` (`RL1`) | Warning → gate closes → closed confirmation → train proceed |
| 6 | A train occupying the crossing, a second train detected before the first clears | The adjacent intersection holds red toward the crossing, the gate-not-opened invariant (overlapping occupancy windows) |
| 7 | Crossing reopens + drain phase | Recovery, queue release, return to the previously used phase group |
| 8 | Injecting a boom-gate fault while a train is approaching | Train signal forced to `STOP`, fault reported, no unsafe proceed |
| 9 | Shutting down `C1` mid-demo | `DEGRADED_LOCAL`, every local controller keeps operating safely and autonomously |
| 10 | Restarting `C1` | Local controllers initiate resync themselves (pushing state up, not receiving commands down) |
| 11 | `REQUEST_OVERRIDE(CLEAR_ROUTE)` while a pedestrian WALK is in progress | The override is correctly queued/deferred, clearance is not cut short |
| 12 | `REQUEST_OVERRIDE` requested while railway pre-emption is in progress at the same intersection | Rejected locally (NACK), railway safety takes precedence |
| 13 | Queue warning from an advance sensor near a closed crossing | Demand-hold logic (holding red toward the crossing) |
| 14 | Green-wave offset across `I1–I3` (`L1`–`L3`, HD extension) | Arterial coordination, offset computed from distance/speed |
| 15 | Killing the process of an `Lx` | Watchdog/supervisor forces outputs to a safe state at that node |

---

## 25. Simulated Input

A single mnemonic keypress set, structured as `<type><location><detail>`. The following characters/digits identify the **physical location** (intersection or crossing); the keypress is routed internally to the controller (`Lx`/`RLx`) for that location:

| Prefix | Meaning | Example |
|---|---|---|
| `V<intersection><approach>` | Vehicle-detection sensor triggered | `V1N` = vehicle detected, North approach of `I1` (handled by `L1`) |
| `Q<intersection><approach>` | Advance/queue sensor triggered | `Q2W` = queue warning, West approach of `I2` (handled by `L2`) |
| `P<intersection><side>` | Pedestrian button pressed | `P1E` = pedestrian request, East crossing of `I1` (handled by `L1`) |
| `T<crossing><direction>` | Train approaching | `T1A` = train approaching `RC1` from direction A; `T1B` = direction B (handled by `RL1`) |
| `FG<crossing>` | Inject a gate/sensor fault | `FG1` = fault at the gate of `RC1` (handled by `RL1`) |
| `FL<intersection>` | Inject a local light hardware fault | `FL2` = fault at `I2` (handled by `L2`) |
| `M` | Toggle between `PEAK_FIXED`/`OFF_PEAK_SENSOR` (operator console) | — |
| `O<intersection><direction>` | Request a `CLEAR_ROUTE` override | `O1NS` = clear the north–south route at `I1` (handled by `L1`) |
| `D` | Dump the current network state to the screen/console | — |

Note: the `X<crossing><direction>` key (train exit) from an earlier draft has been removed from the Core input set, because gate reopening now uses a timer (Section 14) instead of relying on an exit sensor; this key may be reinstated later if an exit sensor is wired into an extension feature.

The console will clearly display the resulting state transition after every keypress, because the demo assessment requires visible, demonstrable behavior — not merely a keypress being accepted.

---

[← Table of Contents](README.md) · [← Safety and Real-Time](04-safety-and-realtime.md) · Next: [Assumptions and Summary →](06-assumptions-and-summary.md)
