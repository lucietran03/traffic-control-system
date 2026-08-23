# 3.2 Scenarios and Written Specifications

> Detail chronological use case scenarios that validate your core system assumptions. Keep the written flow brief and step-by-step.

The following scenarios outline the step-by-step chronological interactions between external entities and the distributed traffic-control system to validate the project's core architectural assumptions. These written specifications detail the system's behavior across standard traffic demand, pedestrian integration, railway pre-emption, hardware fault containment, and network communication. By mapping out these critical operational extremes, the scenarios demonstrate how the system balances central coordination via inter-process communication (IPC) with independent, local fail-safe authority and graceful degradation during network failures.

## 3.2.1 Off-Peak Sensor Actuation

In `OFF_PEAK_SENSOR` mode, the intersection defaults to resting on the arterial phase (R1/R2) to maintain high-priority traffic flow. This scenario illustrates the system's responsiveness when a vehicle arrives on an idle connector road (R3-R5). The local controller (Lx) guarantees a bounded service window by executing standard clearance intervals and extending the green light in 4-second increments up to a strict 40-second maximum cap, preventing indefinite starvation of the connector road.

**Table 10: Use Case 01 Specification**

| Field | Description |
|---|---|
| **Use Case** | UC01: Off-Peak Sensor Actuation |
| **Goal** | Serve vehicle demand on an idle connector road without indefinitely starving the arterial phase. |
| **Primary Actors** | Vehicle |
| **Secondary Actors** | None |
| **Description** | A vehicle triggers the stop-line sensor on an idle connector road. The system safely transitions the phase and extends green up to 40 seconds. |
| **Pre-conditions** | The vehicle presence sensor (`VD_x`) on the connector approach becomes active. |
| **Trigger** | A vehicle activates the presence sensor (`VD_x`). |
| **Basic Course of Events** | 1. The system applies 4 seconds of yellow and 2 seconds of all-red clearance to the arterial phase.<br>2. The connector phase transitions to green for a guaranteed minimum of 8 seconds.<br>3. Green extends in 4-second increments while demand persists.<br>4. The phase terminates at the strict 40-second maximum cap. |
| **Alternative Paths** | **3a. Demand clears before the maximum cap:**<br>1. The vehicle clears the approach, and `VD_x` reports `DEMAND_ABSENT`.<br>2. The system completes the current 4-second increment, then initiates the clearance intervals.<br>3. The system returns to resting on the arterial phase. |
| **Post-conditions** | Connector vehicle demand is served. The system returns to resting on the arterial phase. |
| **Business Rules** | **Bounded Connector Service (DP-06):** Latched connector demand must be served after at most one opposing arterial service opportunity, subject to required clearances.<br><br>**Timing and Extension Limits (TL-01 and TL-03):** Green extends in 4-second increments up to a strict 40-second maximum, accompanied by mandatory 4-second yellow and 2-second all-red clearance intervals.<br><br>**Arterial Priority Default (DP-04):** In the absence of demand, the intersection defaults to resting on the arterial phase (R1/R2). |

## 3.2.2 Pedestrian Parallel Walk

The pedestrian parallel walk scenario demonstrates the system's ability to integrate foot traffic without requiring a disruptive all-red scramble phase. When a pedestrian activates the push-button, the request is latched and scheduled concurrently with the next non-conflicting vehicle phase. The local controller enforces a strict three-state safety sequence, ensuring pedestrian safety and proper clearance before conflicting vehicle traffic is permitted to proceed.

**Table 11: Use Case 02 Specification**

| Field | Description |
|---|---|
| **Use Case** | UC02: Pedestrian Parallel Walk |
| **Goal** | Serve a pedestrian request safely alongside a compatible vehicle phase. |
| **Primary Actors** | Pedestrian |
| **Secondary Actors** | None |
| **Description** | A pedestrian request is latched and served alongside a compatible vehicle phase, concluding with a three-state clearance sequence. |
| **Pre-conditions** | The pedestrian crossing currently displays `DONT_WALK`. |
| **Trigger** | A pedestrian presses the push-button (`PB_x`). |
| **Basic Course of Events** | 1. The system latches the pending request.<br>2. The system schedules `WALK` concurrently with the next non-conflicting vehicle phase.<br>3. The system applies a `FLASHING_DONT_WALK` clearance before the vehicle phase concludes.<br>4. The signal returns to `DONT_WALK`, and the latch clears. |
| **Alternative Paths** | **1a. The pedestrian presses the button repeatedly:**<br>1. The system coalesces repeated presses into a single pending request (TL-06).<br>2. The flow returns to Step 2 of the Basic Course of Events. |
| **Post-conditions** | The pedestrian request latch is cleared. The signal rests at `DONT_WALK`. |
| **Business Rules** | **Latched and Coalesced Demand (TL-06):** One button press creates one pending request; repeated presses are coalesced. Requests have equal scheduling status to vehicle demand.<br><br>**Concurrent Three-State Sequence (TL-05):** The pedestrian sequence (`WALK` → `FLASHING_DONT_WALK` → `DONT_WALK`) must run concurrently with a non-conflicting vehicle phase, not as a separate scramble phase. |

## 3.2.3 Railway Pre-emption and Drain

This scenario highlights the critical intersection of traffic management and railway safety. Upon detecting an approaching train, the local railway controller (RLx) initiates a strict 45-second pre-emption sequence, prompting the adjacent intersection to immediately suppress traffic feeding toward the crossing. Following the train's departure, the system uses the advance queue sensor (`QD_x`) to trigger a bounded 60-second drain phase, clearing the accumulated connector-road backlog before resuming normal cyclical operation.

**Table 12: Use Case 03 Specification**

| Field | Description |
|---|---|
| **Use Case** | UC03: Railway Pre-emption and Drain |
| **Goal** | Protect the railway crossing and efficiently clear the post-closure traffic backlog. |
| **Primary Actors** | Train |
| **Secondary Actors** | None |
| **Description** | The system triggers a 45-second lead-time warning sequence for a train. Once the crossing is clear, the system executes a bounded drain phase to clear the queue. |
| **Pre-conditions** | The system operates normally, and the advance queue sensor (`QD_x`) is inactive. |
| **Trigger** | The train approach sensor (`TS_x`) activates. |
| **Basic Course of Events** | 1. The system executes a 45-second warning sequence, including flasher activation and gate closure.<br>2. The system immediately suppresses the toward-crossing green signal.<br>3. Once the 20-second train occupancy timer elapses, `QD_x` reports `QUEUE_WARNING`.<br>4. The system executes a bounded drain phase up to a maximum of 60 seconds. |
| **Alternative Paths** | **4a. The queue clears before the maximum cap:**<br>1. The `QD_x` sensor ceases reporting `QUEUE_WARNING` before 60 seconds.<br>2. The system terminates the drain phase early and resumes normal phase selection. |
| **Post-conditions** | The railway crossing is open. The traffic backlog is cleared. Normal phase selection resumes. |
| **Business Rules** | **Crossing-Closure Suppression (CC-02):** From pre-emption until the crossing reports `OPEN`, movements feeding toward the crossing receive no green.<br><br>**Post-Reopening Drain (CC-03):** The connector phase extends in 4-second increments while `QUEUE_WARNING` remains active, bounded by a strict 60-second cap.<br><br>**Train-Approach Warning Budget (RC-03):** A 45-second warning-to-arrival budget is enforced to cover flasher lead, gate movement, and adjacent-intersection clearances. |

## 3.2.4 Fail-Safe Hardware Fault

Robust distributed control requires immediate, localized responses to hardware failures. In this scenario, a boom gate fails to provide sensor confirmation that it has successfully closed. The local railway controller (RLx) bypasses any reliance on network connectivity or central acknowledgment, immediately forcing the train signal to `STOP` to protect the crossing. Simultaneously, a fire-and-forget fault report is dispatched to alert the Control Operator, demonstrating the system's fail-safe local autonomy.

**Table 13: Use Case 04 Specification**

| Field | Description |
|---|---|
| **Use Case** | UC04: Fail-Safe Hardware Fault |
| **Goal** | Contain a critical hardware failure locally to ensure physical safety while alerting the central control room. |
| **Primary Actors** | Hardware/Boom Gate |
| **Secondary Actors** | Control Operator |
| **Description** | A boom gate fails to confirm closure. The local system independently forces the train signal to `STOP` and reports the fault to the Control Operator's terminal. |
| **Pre-conditions** | The system has initiated a gate-closing sequence. |
| **Trigger** | The boom-gate position sensor (`GS_x`) fails to confirm `CLOSED` within the expected physical window. |
| **Basic Course of Events** | 1. The system detects the missing sensor confirmation.<br>2. The system independently forces the train signal (`RS_x`) to `STOP`.<br>3. The system transmits a fire-and-forget fault report for display to the Control Operator.<br>4. The system maintains the fail-safe state without awaiting acknowledgment. |
| **Alternative Paths** | **3a. The central link is offline:**<br>1. The communication link to the central control room is down after three consecutive missed heartbeats.<br>2. The system continues to apply its local safe state (`STOP`) independently.<br>3. The fault report to the Control Operator is buffered or dropped depending on the IPC implementation, but physical safety is maintained. |
| **Post-conditions** | The train signal remains at `STOP`. The Control Operator is notified of the fault when communication is available. The crossing remains physically protected. |
| **Business Rules** | **Gate-Confirmed Train Authority (RC-06):** A train signal may show `PROCEED` only when gates are sensor-confirmed `CLOSED`.<br><br>**Independent Fault Response (RC-10):** The railway controller independently forces the train signal to `STOP` and reports to the central controller (C1) without waiting for an acknowledgment.<br><br>**Local Fault Containment (PA-10):** A local failure affects only its own location and must not cascade through the distributed network. |

## 3.2.5 Central Link Failure and Autonomous Fallback

Distributed systems must gracefully handle network degradation without compromising local safety. In this scenario, the communication link between a local controller and the central control room drops. Upon missing three consecutive heartbeats, the system declares the link unavailable and enters a degraded autonomous mode. It retains its last validated timing parameters rather than freezing or reverting to arbitrary defaults and continues normal phase cycling using its local hardware clock.

**Table 14: Use Case 05 Specification**

| Field | Description |
|---|---|
| **Use Case** | UC05: Central Link Failure and Autonomous Fallback |
| **Goal** | Maintain safe, continuous local intersection operation when central communication is lost. |
| **Primary Actors** | Local Hardware Clock |
| **Secondary Actors** | Central Controller (as a disconnected monitoring entity) |
| **Description** | The heartbeat link fails. The system enters a degraded mode, retains its last known timing profile, and relies on its local clock for phase transitions. |
| **Pre-conditions** | The system operates normally with an active heartbeat connection to the central control room. |
| **Trigger** | The system registers three consecutive missed 1-second heartbeats from the central control room. |
| **Basic Course of Events** | 1. The system detects a heartbeat timeout and declares the communication link unavailable.<br>2. The system transitions into a `DEGRADED_LOCAL` operational state.<br>3. The system locks in the last validated timing parameters and disables external coordination.<br>4. The local hardware clock assumes full authority over Peak/Off-Peak mode switching and phase cycling. |
| **Alternative Paths** | **3a. Reconnection occurs:**<br>1. The system detects resumed heartbeats from the central control room.<br>2. The system pushes its complete current state to the central controller before accepting any new commands.<br>3. Normal coordinated operation resumes. |
| **Post-conditions** | The intersection continues safe, autonomous operation without central coordination. |
| **Business Rules** | **Central-Link Failure Protocol (PA-07):** After three consecutive missed 1-second heartbeats, the local controller retains the last validated timing parameters while its local clock continues selecting Peak/Off-Peak modes.<br><br>**Graceful Degradation (PA-12):** Central unreachability does not force an all-red or degraded output state; local autonomous cycling must safely continue. |

## 3.2.6 Central Profile Update

To support dynamic traffic coordination, the system allows central operators to push updated timing profiles to local nodes via IPC. This scenario demonstrates the system's strict adherence to local safety authority during a remote update. When a new `SET_TIMING_PROFILE` command is received, the system validates the request against its hardcoded safety invariants. Rather than applying the change immediately mid-phase, the system queues the update to take effect only at the next safe phase boundary.

**Table 15: Use Case 06 Specification**

| Field | Description |
|---|---|
| **Use Case** | UC06: Central Profile Update |
| **Goal** | Safely apply updated coordination timing parameters received from the central control room. |
| **Primary Actors** | Control Operator |
| **Secondary Actors** | None |
| **Description** | A Control Operator pushes a new timing profile. The local system validates the parameters and applies them at the next safe phase boundary. |
| **Pre-conditions** | The system operates normally with an active, synchronized link to the central control room. |
| **Trigger** | The system receives a `SET_TIMING_PROFILE` IPC command from the central control room. |
| **Basic Course of Events** | 1. The system extracts the proposed timing parameters (for example, a new green-wave offset) from the IPC message.<br>2. The system validates the parameters against local safety invariants, such as minimum-green and clearance limits.<br>3. The system returns an `ACK` message to the central control room confirming successful validation.<br>4. The system queues the mode or timing change and applies it at the next safe phase boundary. |
| **Alternative Paths** | **2a. The command violates safety invariants:**<br>1. The proposed profile attempts to bypass the 4-second yellow or 2-second all-red clearance intervals.<br>2. The system rejects the command and returns a `NACK` message to the central control room.<br>3. The system continues operating safely with its current timing profile. |
| **Post-conditions** | The intersection operates using the updated, validated timing parameters without interrupting an active phase. |
| **Business Rules** | **Unsafe Command Rejection (PA-09):** Any Central command that violates a safety invariant is rejected with a `NACK`.<br><br>**Safe Mode Transition (TL-04):** Any validated change requested by the central controller takes effect only at the next safe phase boundary. |
