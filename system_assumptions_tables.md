**1.2 System Assumptions**

<< Document any assumptions you made about how the system will operate. You must explicitly state and justify the assumptions used for timing, traffic coordination, and congestion control. You should also state any assumptions regarding differences in traffic demand and priority between roads R1–R5. >>

To bridge the gap between abstract requirements and concrete software implementation, specific operational parameters must be established. The following tables outline the foundational parameters guiding the system architecture, ensuring realistic traffic flow, correct safety behaviour, and reliable interprocess communication.

Supporting calculations for numeric assumptions are provided in **Appendix B** and referenced directly from the relevant row. External sources are cited in IEEE numbered style `[n]`, with full references provided at the end of this document.

---

**1.2.1 Network and Road Usage Assumptions (NU-xx)**

| **ID** | **Assumption** | **Justification** | **Design Implication** |
| --- | --- | --- | --- |
| NU-01 | The system uses **Vietnam's right-hand traffic convention**. In each direction, the median-side lane carries through and permissive left-turn traffic, while the kerb-side lane carries through and permissive right-turn traffic. | Vietnamese law requires road users to travel on the right [6]. For real-world context, the design refers to the documented layout of the signalised Mai Chí Thọ–Đồng Văn Cống intersection [7]; official Ho Chi Minh City documentation places both roads in the former District 2 [8]. This does not imply that the project reproduces that site's topology or measured timing. | All lane layouts, simulated sensor mappings, permitted movements, and conflict definitions follow right-hand traffic. |
| NU-02 | Each approach has **two lanes per direction**. The median-side lane carries through and permissive left-turn movements; the kerb-side lane carries through and permissive right-turn movements. The Core design uses no dedicated turn lanes or protected turn-arrow phases. | Two lanes per direction are required by the brief, while dedicated turn lanes and arrow signals are optional. The shared-lane design retains all turning movements while avoiding the additional phases, per-lane sensing, and implementation complexity of a protected-turn design. | Each approach uses one circular vehicle signal head and approach-level sensing. The signal controller prevents prohibited phase conflicts, while permissive turns remain subject to the applicable yielding rule. |
| NU-03 | Each intersection has **four approaches** and **four pedestrian crossing sides**. Each approach has one vehicle signal head and one vehicle-presence sensor; each crossing side has one pedestrian button and one pedestrian signal head. | This symmetric layout supports one configurable controller design for all six intersections and is consistent with the lecturer's clarification that four pedestrian buttons may be modelled at each intersection. | Each `Lx` manages four vehicle signals, four vehicle-presence sensors, four pedestrian buttons, and four pedestrian signals. |
| NU-04 | Minor side roads, private access roads, and unsignalised approaches outside **R1–R5** are outside the system boundary. | The brief permits unimportant side roads to be omitted so that the model can focus on the specified network. | Unmodelled roads contribute no sensor input, traffic demand, or timing requirement. |
| NU-05 | **R1 and R2 are arterial roads**, forming the chains `I1–I3–I5` and `I2–I4–I6`. **R3, R4, and R5 are connector roads** linking `I1–I2`, `I3–I4`, and `I5–I6`, respectively; each connector crosses the railway at `RC1`, `RC2`, or `RC3`. | This classification follows the supplied network topology and distinguishes the continuous roads suited to coordination from the shorter roads affected by railway crossings. | R1 and R2 receive arterial priority and green-wave coordination. R3–R5 use railway pre-emption and congestion-suppression logic. |
| NU-06 | Lane directions and permitted movements remain **fixed**; dynamic lane reversal and time-of-day lane reassignment are not modelled. | Neither feature is required, and both would add control states unrelated to the assessed system behaviour. | Controllers may change signal timing and sequencing, but not lane direction or permitted movements. |
| NU-07 | Vehicle sensing and vehicle-signal actuation operate at **approach level**, not per lane or turning movement. Pedestrian inputs and signals operate separately by crossing side, as defined in NU-03. | The brief does not require per-lane sensing or turn-specific signals, and the Core shared-lane design does not need that granularity. | `OFF_PEAK_SENSOR` responds to approach-level demand and cannot distinguish whether a detected vehicle intends to travel straight or turn. |

---

**1.2.2 Traffic Demand and Road Priority Assumptions (DP-xx)**

| **ID** | **Assumption** | **Justification** | **Design Implication** |
| --- | --- | --- | --- |
| DP-01 | Two normal traffic-demand modes are modelled: `PEAK_FIXED` and `OFF_PEAK_SENSOR`. No additional morning, evening, or night demand tier is defined. | These two modes cover the brief's fixed-timing and sensor-driven approaches. Its advanced/updateable approach is implemented through validated `SET_MODE` and `SET_TIMING_PROFILE` commands, rather than as a third demand tier. | Normal mode selection uses only `PEAK_FIXED` and `OFF_PEAK_SENSOR`. Railway pre-emption, Central override, degraded operation, and fault handling remain separate overlay or safety states. |
| DP-02 | The clock-time boundary between Peak and Off-Peak operation is **configurable** rather than fixed by the architecture. | The brief specifies different time-of-day sequence patterns but provides no exact transition time. A configurable boundary also supports a compressed demonstration schedule. | `C1`, or the local controller's clock when Central is unavailable, selects the applicable normal mode without hard-coding a specific hour into the control logic. |
| DP-03 | No numeric vehicle-arrival rate or stochastic vehicle-generation model is used. Traffic demand is represented by discrete sensor events. | The brief permits keyboard-simulated sensor inputs and does not require traffic-flow simulation. Invented vehicles-per-hour figures would not affect any controller decision or be independently verifiable. | Ordinary demand is represented by binary approach states, `DEMAND_PRESENT` and `DEMAND_ABSENT`. The system does not count vehicles or estimate a numeric queue length; the separate advance detector only reports whether a queue has reached its fixed detection point. |
| DP-04 | Road priority is **qualitative** rather than numerically demand-weighted. R1 and R2 have higher base priority as arterial roads; R3, R4, and R5 have lower base priority as connector roads and are treated equally. | The brief requires the team to state and justify its demand and priority assumptions but provides no traffic counts or priority weights. The arterial–connector distinction follows the network roles defined in NU-05 without inventing unverifiable numeric demand. | `PEAK_FIXED` allocates more green time to the arterial phase, while `OFF_PEAK_SENSOR` rests on the arterial phase when no demand is present. Connector approaches remain subject to separate railway pre-emption and congestion-suppression rules. |
| DP-05 | The arterial–connector priority classification is **static** across Peak and Off-Peak operation. | A road's network role is distinct from its instantaneous sensor demand. Reclassifying roads from transient sensor states would make controller behaviour less predictable without adding a required capability. | Sensor demand may affect phase selection or extension, but it never changes which roads are classified as arterial or connector. |
| DP-06 | A waiting connector approach must not be starved indefinitely. In `OFF_PEAK_SENSOR`, a latched connector demand is served after no more than one opposing arterial service opportunity has completed, subject to the required yellow and all-red clearance intervals. | Higher arterial priority must not prevent a lower-priority connector from eventually receiving service. | Persistent arterial demand cannot repeatedly extend or immediately reselect the arterial phase while a connector request remains pending. The phase-selection and extension-cap logic enforces the service bound without a separate weighting algorithm. |

---

**1.2.3 Traffic-Light Timing Assumptions (TL-xx)**

| **ID** | **Assumption** | **Justification** | **Design Implication** |
| --- | --- | --- | --- |
| TL-01 | Configured timing limits are: **minimum green 8 s**, **maximum off-peak green 40 s**, **yellow 4 s**, and **red-clearance 2 s** for every conflicting phase transition. | The brief supplies no timing values. These tunable PoC values provide bounded operation; the selected yellow and red-clearance intervals are consistent with published signal-timing guidance [3]. | Every conflicting phase change completes yellow and red clearance before the next green. No mode, pre-emption event, or Central command may skip these intervals. |
| TL-02 | `PEAK_FIXED` uses a **90 s two-phase cycle**: the arterial phase receives 48 s green and the connector phase receives 30 s green, with each followed by the TL-01 clearance intervals. | The resulting 54 s arterial allocation and 36 s connector allocation give the arterial a larger share, consistent with DP-04. Railway pre-emption remains event-driven and does not depend on the 120 s peak train headway. See **Appendix B1** for the cycle breakdown. | The 90 s cycle and its green split remain fixed while `PEAK_FIXED` is active, except when a higher-priority safety or override state intervenes. |
| TL-03 | `OFF_PEAK_SENSOR` serves a phase with vehicle demand or a latched pedestrian request. Green extends in **4 s increments** while demand persists, up to the TL-01 maximum; with no demand, the intersection rests on the arterial phase. | This implements the brief's sensor-driven approach while retaining the arterial default established in DP-04. | Phase selection and extension use bounded boolean-demand logic and enforce the anti-starvation guarantee in DP-06. |
| TL-04 | A switch between `PEAK_FIXED` and `OFF_PEAK_SENSOR`, whether clock-selected or requested by `C1`, takes effect at the next safe phase boundary rather than mid-phase. | Deferring the switch preserves minimum-green and clearance requirements. | The sequencer records a pending mode change and applies it only after the current phase and required clearances complete. |
| TL-05 | Pedestrian signals use `WALK → FLASHING_DONT_WALK → DONT_WALK`. WALK runs with a compatible vehicle phase rather than in a separate pedestrian-scramble phase. | The three pedestrian indications and the requirement to hold conflicting vehicles red are consistent with published pedestrian-signal guidance [4]. A separate scramble phase is not required by the brief. | A pedestrian phase is scheduled only with vehicle movements that do not conflict with its crossing. |
| TL-06 | A pedestrian-button press creates one latched request; repeated presses do not create additional requests. The request is served when its compatible phase next runs and is scheduled equally with vehicle demand in `OFF_PEAK_SENSOR`. | Latching prevents a request from being lost, while coalescing repeated presses prevents one user from repeatedly extending service. | Each crossing side uses one pending flag, cleared only after its requested WALK and clearance sequence completes. |

---

**1.2.4 Traffic Coordination Assumptions (TC-xx)**

| **ID** | **Assumption** | **Justification** | **Design Implication** |
| --- | --- | --- | --- |
| TC-01 | Illustrative arterial distances are `I1–I3 = 350 m`, `I3–I5 = 400 m`, `I2–I4 = 320 m`, and `I4–I6 = 380 m`. The assumed arterial travel speed is **60 km/h (16.67 m/s)**. | The brief and lecturer clarification require distance or travel time to inform coordination but do not require a physical survey. These values are tunable design parameters, not claimed measurements. The selected speed is consistent with the current Vietnamese maximum for qualifying urban divided roads or one-way roads with at least two motor-vehicle lanes [2]. | These inputs determine the arterial offsets in TC-02. Changing them requires recalculating the offsets, not changing the coordination architecture. |
| TC-02 | Green-wave coordination applies only to `I1–I3–I5` on R1 and `I2–I4–I6` on R2. Each offset is calculated as inter-intersection distance divided by assumed travel speed. | The arterials form continuous routes suited to platoon progression; the shorter connector roads are dominated by railway pre-emption and are not coordinated. See **Appendix B2** for the calculated offsets. | Only the `Lx` sequencers on the two arterial chains apply these offsets; railway-controller timing is unaffected. |
| TC-03 | `C1` distributes each arterial offset through `SET_TIMING_PROFILE`, while every `Lx` remains responsible for applying the parameter to its own sequencer. | This preserves the brief's local-control rule: Central may coordinate timing but never directly actuates a signal. | The offset is a validated timing parameter, not a new Central authority or a separate local operating mode. |
| TC-04 | If a valid coordination profile is unavailable or stale, an `Lx` continues standalone fixed or sensor-driven operation without waiting for `C1`. | Local operation must continue when Central or its communication link is unavailable. | Coordination is a best-effort enhancement and never a dependency for safe local operation. |
| TC-05 | After railway pre-emption disrupts an intersection's phase clock, no separate resynchronisation state machine is used; the next valid timing profile re-establishes the ordinary offset. | The normal timing-profile path already restores coordination, so a second recovery mechanism adds complexity without an independent safety benefit. | The controller resumes safe standalone cycling first and recovers coordination when it can next apply a valid profile. |

---

**1.2.5 Congestion Control Assumptions (CC-xx)**

| **ID** | **Assumption** | **Justification** | **Design Implication** |
| --- | --- | --- | --- |
| CC-01 | Each crossing has one binary **advance/queue detector** on each of its two crossing-facing intersection approaches, positioned approximately **60–80 m upstream of the stop line**. | A separate upstream detection point can report that a queue has reached a defined threshold without requiring vehicle counting or continuous queue-length estimation. | `QUEUE_WARNING` is used only by congestion suppression and recovery logic, not by ordinary TL-03 phase extension. |
| CC-02 | From the start of railway pre-emption until the crossing reports `OPEN`, the movement toward that crossing receives no green. Compatible away-from-crossing and cross-traffic movements may be prioritised where the existing geometry permits. | This implements the lecturer's instruction to stop feeding traffic toward a closed crossing without claiming that signals can physically reroute vehicles. | Congestion suppression modifies scheduling within the current normal mode; it does not create a third traffic-demand mode. |
| CC-03 | After reopening, a connector drain phase extends in **4 s increments** while `QUEUE_WARNING` remains active, ending when the warning clears or a separate **60 s cap** is reached. | The bounded rule clears backlog without pretending that a binary detector can calculate vehicle count or queue length. The 60 s cap is a tunable recovery parameter. See **Appendix B3** for the rule summary. | Drain duration is determined by repeated boolean checks and a hard cap, after which ordinary phase selection resumes. |

---

**1.2.6 Railway Crossing Assumptions (RC-xx)**

| **ID** | **Assumption** | **Justification** | **Design Implication** |
| --- | --- | --- | --- |
| RC-01 | Each crossing has two boom gates, two road-facing flasher sets, two train signals, two train-approach sensors, and two gate-position sensors. Two exit sensors are reserved for a future enhancement but do not participate in Core reopening logic. | The double-track corridor carries trains in both directions and requires symmetric detection and protection. | The owning `RLx` exclusively controls the gates, flashers, and train signals; other controllers receive status only. |
| RC-02 | `RL1–RL3` independently manage `RC1–RC3`. They share no cross-crossing state and do not model one train propagating through all three crossings. | The lecturer distinguished railway controllers from intersection controllers, and the supplied topology provides no requirement for cross-crossing coordination. | Each `RLx` broadcasts status only to the two adjacent `Lx` controllers; no `Lx` may command railway equipment. |
| RC-03 | Train detection provides an approximately **45 s warning-to-arrival budget**, including flasher lead, gate movement, closed-confirmation margin, and adjacent-intersection clearance. | The brief provides no warning time. A component-based budget is traceable and accounts for road-signal pre-emption as well as gate operation; published crossing guidance likewise treats warning time and nearby-signal pre-emption as an integrated timing problem [5]. See **Appendix B4**. | The budget determines the required reaction window and, with RC-08, the illustrative approach-sensor placement distance. |
| RC-04 | Each simulated train occupies a crossing for a fixed **20 s** after its expected arrival. Gates remain closed until every active occupancy window, including a second train from either direction, has elapsed. | A fixed timer simplifies the Core demonstration by avoiding dependence on live exit detection. | `RLx` tracks overlapping occupancy windows and reopens only after all have expired. |
| RC-05 | Because Core reopening is timer-based, an abnormally slow or stalled train that remains after its 20 s window is not detected. | This is an accepted Core limitation, stated explicitly rather than hidden behind the unused exit sensors. | Exit-sensor confirmation is the identified future enhancement; it is not part of any current reopening decision. |
| RC-06 | A train signal may show `PROCEED` only while the relevant gates are sensor-confirmed `CLOSED`. Elapsed gate-closing time is never accepted as confirmation. | Scheduling estimates and physical verification serve different purposes; only the position sensor can establish the safety condition. | Missing or contradictory confirmation forces `STOP` and raises a local fault. |
| RC-07 | `RL1`, `RL2`, and `RL3` run identical control logic and differ only by configuration. | No source requirement justifies crossing-specific behaviour. | One generic railway-controller implementation is deployed three times. |
| RC-08 | The assumed maximum train speed is **80 km/h (22.2 m/s)**. | The brief supplies no train speed. This is an explicit, tunable planning assumption used only to size the detection distance. | With the RC-03 budget, the illustrative placement distance is approximately 1,000 m; see Appendix B4. |
| RC-09 | `C1` may send a high-level validated request to `RLx`, such as requesting fault clearance after repair, but never a raw gate, flasher, or train-signal actuation command. | This reconciles the brief's Central-command wording with the lecturer's rule that railway equipment remains under its local controller. | `RLx` validates every request and ACKs or NACKs it; unsafe or premature requests have no effect. |
| RC-10 | On a railway-equipment fault, `RLx` forces the train signal to `STOP` locally and reports the fault to `C1` as an independent parallel action. | The safety response must not depend on Central connectivity or acknowledgement. | `RLx` never waits for `C1` before applying its local safe state. |
| RC-11 | A train-approach input stuck active beyond its diagnostic timeout is detectable and causes a safe-side fault response. A permanently silent input is not detectable without an independent health signal. | A binary detection line cannot distinguish "no train" from a silent failure, so claiming otherwise would overstate the design. | A silent failure is an accepted fail-open residual risk; an independent sensor heartbeat or self-test is the identified out-of-Core mitigation. |

---

**1.2.7 Pedestrian, Sensing, and Abnormal-Operation Assumptions (PA-xx)**

_Pedestrian Operation_

| **ID** | **Assumption** | **Justification** | **Design Implication** |
| --- | --- | --- | --- |
| PA-01 | Pedestrian crossings are modelled only at `I1–I6`, not at `RC1–RC3`. | The brief places pedestrian facilities at road intersections; track-crossing pedestrian facilities are outside the selected scope. | Railway controllers own no pedestrian inputs or signals. |
| PA-02 | A pedestrian request received during railway pre-emption remains latched. It is served in a compatible phase if available; otherwise it waits until the conflicting restriction clears. | Railway pre-emption should not discard demand, but it must still take precedence over any conflicting movement. | The ordinary request latch remains active across the railway event, while the conflict matrix determines when service is safe. |
| PA-03 | A push-button input stuck active is coalesced into one pending request and reported as a fault. A permanently silent button is not detectable without an independent health signal. | A binary input can expose a continuously asserted fault but cannot distinguish no button press from a dead input. | Stuck-active input cannot create unlimited requests; silent failure is an accepted limitation unless explicit self-test is added. |

_Vehicle Sensing_

| **ID** | **Assumption** | **Justification** | **Design Implication** |
| --- | --- | --- | --- |
| PA-04 | Each approach has one binary stop-line vehicle-presence detector. It drives phase selection and extension only in `OFF_PEAK_SENSOR`; the separate CC-01 detector serves only congestion logic. | Approach-level demand is sufficient for the selected shared-lane design. Keeping the two detector roles separate preserves fixed timing and avoids claiming vehicle counts. | `PEAK_FIXED` timing is unaffected by ordinary presence demand; `QUEUE_WARNING` never substitutes for ordinary stop-line demand. |
| PA-05 | Pedestrian demand is generated only by push-button actuation; passive pedestrian-presence detection is not modelled. | Push-button events provide deterministic, demonstrable input consistent with the brief. | A `PED_REQUEST` originates only from the relevant button input. |
| PA-06 | A vehicle detector stuck active is detectable and is treated as continuous demand. A permanently inactive detector is not distinguishable from an empty approach without an independent health signal. | Inferring failure from a long period with no vehicle event would produce false faults on genuinely quiet approaches. | Silent detector failure is an accepted limitation; an explicitly reported or injected detector fault fails that approach to `DEMAND_PRESENT`. |

_Abnormal and Failure Operation_

| **ID** | **Assumption** | **Justification** | **Design Implication** |
| --- | --- | --- | --- |
| PA-07 | Controllers send a heartbeat every **1 s** and declare the Central link unavailable after **three consecutive missed heartbeats**. They retain the last validated timing parameters, while their local clocks continue selecting Peak or Off-Peak mode. | The brief requires continued local operation after communication or Central failure. The timeout is a tunable PoC diagnostic value. | `DEGRADED_LOCAL` removes coordination and override availability but does not freeze local mode selection or force all-red. |
| PA-08 | On reconnection, the local controller sends its complete current state before accepting new Central commands. | The local state may have advanced while Central's view was stale. | Central resumes commanding only after the reconnection state exchange completes. |
| PA-09 | A Central request that would violate a safety invariant is rejected with NACK rather than applied or silently discarded. | Central may coordinate and request bounded overrides but never bypass local safety authority. | Every local command handler validates the request before changing state and returns ACK or NACK. |
| PA-10 | A controller or supervised-process fault affects only the owning location, whose watchdog forces the documented safe outputs. | One local failure must not cascade through the distributed network. | `FAULT_SAFE` remains local: intersection outputs use their safe road state, while railway outputs hold gates down and train signals at `STOP` where actuation remains available. |

---

**References**

[1] School of Engineering, RMIT University, "EEET2588 Real-Time Systems – Design Project," project brief, 2026. Statements attributed to the lecturer's in-class clarification are drawn from the team's class notes in `lecture_clarification.md`.

[2] Ministry of Transport of Vietnam, *Circular 38/2024/TT-BGTVT: Regulations on Speed and Safe Distance for Motor Vehicles and Special-Purpose Vehicles Participating in Road Traffic*, effective 1 January 2025, art. 6, table 1. Available: https://vanban.chinhphu.vn/?docid=211873&pageid=27160

[3] Federal Highway Administration, *Traffic Signal Change and Clearance Interval Pooled Fund Study: Synthesis Report*, FHWA-HOP-23-037, 2023, pp. 25–26. Available: https://ops.fhwa.dot.gov/publications/fhwahop23037/fhwahop23037.pdf

[4] Federal Highway Administration, *Manual on Uniform Traffic Control Devices for Streets and Highways*, 11th ed., 2023, sec. 4I.06. Available: https://mutcd.fhwa.dot.gov/pdfs/11th_Edition/part4.pdf

[5] Federal Highway Administration and Federal Railroad Administration, *Highway-Rail Crossing Handbook*, 3rd ed., FHWA-SA-18-040/FRA-RRS-18-001, 2019, ch. 2. Available: https://highways.dot.gov/safety/hsip/xings/highway-rail-crossing-handbook-third-edition/chapter-2-engineered-treatments-1

[6] National Assembly of Vietnam, *Law on Road Traffic Order and Safety*, No. 36/2024/QH15, art. 10, effective 1 January 2025. Available: https://xaydungchinhsach.chinhphu.vn/toan-van-luat-trat-tu-an-toan-giao-thong-duong-bo-119240909105718285.htm

[7] H. D. Nguyen, *Capacity Analysis of Signalised Intersections in Motorcycle Dependent Cities*, doctoral dissertation, Technische Universität Darmstadt, 2020, fig. A-7, pp. 120–121. Available: https://tuprints.ulb.tu-darmstadt.de/server/api/core/bitstreams/4f012cc2-25f6-4408-986f-4ac3440501b0/content

[8] Ho Chi Minh City Department of Transport, Guidance No. 6460/HD-SGTVT, *Ho Chi Minh City Official Gazette*, no. 94, 15 December 2018, road list entries 20 and 31. Available: https://www.congbao.hochiminhcity.gov.vn/cong-bao/van-ban/huong-dan/so/6460-hd-sgtvt/ngay/12-11-2018/tai-ve/43340

---

_Source: this table is derived from, and must be kept consistent with, the finalised Vietnamese-language project specification (`spec-vi/01`–`06`, `PROJECT_SPECIFICATION.vi.md`) and its companion diagrams (`SYSTEM_DIAGRAMS.vi.md`). If the specification changes, this table should be regenerated from it rather than edited independently, to avoid the two documents drifting apart again._

---

## Appendix B: Supporting Assumption Derivations

### B1. Peak Fixed-Cycle Timing

| Phase | Green | Yellow | Red clearance | Phase allocation |
| --- | ---: | ---: | ---: | ---: |
| Arterial | 48 s | 4 s | 2 s | 54 s |
| Connector | 30 s | 4 s | 2 s | 36 s |
| **Total cycle** |  |  |  | **90 s** |

The arterial receives 18 s more green than the connector, implementing the qualitative priority in DP-04. These are tunable configuration values rather than measured site timings.

### B2. Arterial Green-Wave Offsets

The calculation uses `offset = distance ÷ 16.67 m/s`, based on TC-01.

| Segment | Distance | Segment offset | Cumulative offset from chain start |
| --- | ---: | ---: | ---: |
| `L1 → L3` on R1 | 350 m | 21 s | 21 s |
| `L3 → L5` on R1 | 400 m | 24 s | 45 s |
| `L2 → L4` on R2 | 320 m | 19 s | 19 s |
| `L4 → L6` on R2 | 380 m | 23 s | 42 s |

### B3. Congestion Drain Rule

| Input or limit | Value |
| --- | --- |
| Queue input | Binary `QUEUE_WARNING` from CC-01 |
| Recheck/extension increment | 4 s |
| Maximum drain green | 60 s |
| Termination | `QUEUE_WARNING` clears or the 60 s cap is reached |

The binary detector does not support a queue-length formula. Drain time is therefore selected by repeated state checks within a bounded interval.

### B4. Railway Warning Lead Time and Sensor Placement

| Component | Duration | Running total |
| --- | ---: | ---: |
| Flashing-only warning | 5 s | 5 s |
| Gate-closing allowance | 10 s | 15 s |
| Closed-confirmation margin | 5 s | 20 s |
| Adjacent-intersection clearance allowance | 25 s | **45 s** |

Using the RC-08 maximum train speed:

`Approach distance = 45 s × 22.2 m/s ≈ 999 m ≈ 1,000 m.`

The timing and placement values are tunable PoC assumptions. Actual deployment would require a site-specific railway and traffic engineering study.
