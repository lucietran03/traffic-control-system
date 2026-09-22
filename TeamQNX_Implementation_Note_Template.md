EEET2588 | EEET2687 - Real-Time Systems Engineering

IMPLEMENTATION NOTE

Traffic Light Control System

Team QNX

| Document information |  |
| --- | --- |
| Course | EEET2588 \| EEET2687 - Real-Time Systems Engineering |
| Team | Team QNX |
| Members | Tran Dong Nghi - s3914633<br>Le Hung - s4061665<br>Hoang Minh Thang - s3999925 |
| Lecturer | Dr Linh Tran |
| Semester | Semester 2, 2026 |
| Due date | 18/09/2026 |
| Repository / commit | https://github.com/lucietran03/traffic-control-system/ |

# Declaration and Statement of Authorship

I/we have not impersonated, or allowed myself to be impersonated by, any person for the purposes of this assessment.

This assessment is my/our original work and no part of it has been copied from any other source except where due acknowledgement is made.

No part of this assessment has been written for me/us by any other person except where such collaboration has been authorised by the lecturer/teacher concerned.

Where this work is being submitted for individual assessment, I declare that it is my original work and that no part has been contributed by, produced by or in conjunction with another student.

I/we give permission for my assessment response to be reproduced, communicated compared and archived for the purposes of detecting plagiarism.

I/we give permission for a copy of my assessment to be retained by the university for review and comparison, including review by external examiners.

I/we understand that:

Plagiarism is the presentation of the work, idea or creation of another person as though it is your own. It is a form of cheating and is a very serious academic offence that may lead to exclusion from the University. Plagiarised material can be drawn from, and presented in, written, graphic and visual form, including electronic data and oral presentations. Plagiarism occurs when the origin of the material used is not appropriately cited.

Plagiarism includes the act of assisting or allowing another person to plagiarise or to copy my work.

I/we agree and acknowledge that:

I/we have read and understood the Declaration and Statement of Authorship above.

If I/we do not agree to the Declaration and Statement of Authorship in this context and a signature is not included below, the assessment outcome is not valid for assessment purposes and will not be included in my final result for this course.

# 1. Implementation Overview

Target: approximately 0.5-1 page. Describe what was actually implemented, not the full aspirational design.

## 1.1 Purpose and Implemented Scope

<< Detail the boundaries of the implemented system. Mention the 5 road networks (R1-R5), the inclusion of pedestrian buttons (which targets the CR/HD grade boundary), and the fail-safe autonomy of local nodes.  >>

This implementation delivers a distributed, locally autonomous real-time traffic-control system operating on QNX RTOS. The proof-of-concept effectively manages a network of six signalized intersections (I1-I6) distributed across five two-way roads (R1-R5), which intersect with a double-track railway corridor managed by three crossing controllers (RC1-RC3).

The implemented scope encompasses decentralized intersection autonomy, active pedestrian crossing management, centralized command processing, and safety-critical railway preemption. Due to the simulation environment, physical hardware is completely abstracted: sensor inputs, such as vehicle presence, train approaches, and pedestrian push-buttons are simulated via concurrent, blocking stdin keyboard-reader threads, while physical signal actuations are rendered as formatted text messages to the terminal window.

## 1.2 Deployment Summary

*Table 2. Implemented deployment summary*

| Item | Implemented configuration |
| --- | --- |
| Physical QNX targets | 9 node QNX Virtual Machines connected via Qnet. |
| Logical controllers | C1 (Central); L1-L6 (Intersections); RL1-RL3 (Railway). |
| Build configuration | QNX Momentics IDE, standard C compiler, QNX SDP framework. |
| Operator interface | Standard text output to a terminal window. |
| Sensor simulation | Blocking keyboard input processed by dedicated stdin reader threads. |
| Persistent storage | Centralized system events appended to a local central_log.txt file. |

## 1.3 Relationship to the Initial Design

Reference the Initial Design Report rather than repeating it. Record only material implementation changes.

<< Focus strictly on engineering deviations (e.g., replacing physical GPIO with standard I/O threads, or simplifying the two boom gates into a single synchronized state variable).>>

*Table 3. Material design-to-implementation changes*

| Design item | Initial design | Implemented result | Reason / impact |
| --- | --- | --- | --- |
| Boom Gate State (RC-01) | Two independent boom gates and sensors per crossing. | Simplified to a single aggregate gate-motion and confirmation state for the entire crossing. | Reduces unnecessary IPC payload complexity while strictly maintaining the core confirmation-before-proceed safety invariant |
| Override Expiration (PA-11) | Absolute wall-clock expiry deadline comparisons. | Relative elapsed-millisecond countdown decremented per internal phase tick. | Mitigates the lack of a synchronized monotonic clock API across independent QNX nodes, ensuring reliable expiration independent of system time discrepancies. |
| Cross-Node IPC Resolution | Implicitly assumed local-only messaging routing. | Dynamic Qnet node resolution utilizing a TRAFFIC_NODE_MAP environment variable. | Allows the system to scale transparently across multiple isolated Virtual Machines without requiring source code recompilation. |

# 2. Build, Deployment, and Operation

An assessor should be able to reproduce the demonstrated system using this section and the submitted source code.

## 2.1 Prerequisites

<< List your QNX SDP version, QNX Momentics IDE version, and required multi-VM Qnet networking configurations.>>

IDE and Toolchain: QNX Momentics IDE 8.0.3 utilizing the standard QNX Software Development Platform 7.1 (SDP) C compiler and toolchain.

Operating Environment: Multiple QNX Virtual Machines with Qnet networking enabled to support distributed inter-process communication.

Environment Variables: The TRAFFIC_NODE_MAP environment variable must be defined in the execution shell before launch to resolve cross-node IPC paths. This variable maps logical controller suffixes (e.g., c1, l1, rl1) to physical QNX node hostnames. If testing entirely on a single machine, this variable can be safely left unset to default to local routing.

Dependencies: The system relies strictly on standard POSIX and QNX Neutrino system libraries and no external or third-party libraries are required.

## 2.2 Build Procedure

<< Details on how to build the Traffic_Control_System workspace. Note that the assessor must compile all three independent projects: central_controller, intersection_controller, and railway_controller.>>

- Step 1: Import Projects: Launch the QNX Momentics IDE. Navigate to File > Import... and choose Projects from Folder or Archive under General category. Browse and select the submitted .zip folder. In the import window, ensure you select just the 3 Eclipse projects (central_controller, intersection_controller, and railway_controller) from the list. Tick Add project to working sets for easier workspace management, and then click Finish.

- Note: Ensure you do not have any existing projects with these exact same names in your workspace before importing. Once finished, you should see all 3 projects clearly listed in your Project Explorer. In case, C/C++ Indexer database gets corrupted, try to restart the QNX Momentics IDE.

- Step 2: Compile: Highlight all three projects, right-click, and select Build Project to compile the distributed node binaries.

- Step 3: Verify Output: Confirm the build finishes without errors. The resulting executable binaries will be automatically generated and located within the Binaries or build directories inside each respective project folder.

- Step 4: Clean and Rebuild: If diagnosing a failed build or clearing stale artifacts, right-click the projects, select Clean Project to remove compiled object files, and perform the build step again.

## 2.3 Deployment and Startup Sequence

<< Explain the execution order. Typically: Start Qnet nodes -> Launch C1 -> Launch RL1-RL3 -> Launch L1-L6. Mention how the TRAFFIC_NODE_MAP environment variable routes the IPC.>>

Step 1: Network Initialization: Verify that Qnet is active across all QNX Virtual Machines and that each target node can successfully resolve its peers in the /net directory namespace.

Step 2: Binary Deployment: Transfer the compiled executable binaries from the host build environment to the /tmp directory on the respective target QNX Virtual Machines. Ensure the files have the necessary execution permissions (chmod +x)

Step 3: Environment Configuration: Before launching any binaries, define the TRAFFIC_NODE_MAP environment variable in the execution shell of every node. This ensures proper Qnet routing by mapping logical controller suffixes to their respective physical hostnames.

export TRAFFIC_NODE_MAP="c1=<hostname>,l1=<hostname>,l2=<hostname>,l3=<hostname>,l4=<hostname>,l5=<hostname>,l6=<hostname>,rl1=<hostname>,rl2=<hostname>,rl3=<hostname>”

Note: The <hostname> must be replaced with the real network hostnames visible from your terminal.

Step 4: Execution Order: Launch the compiled binaries in the following sequence to prevent initial communication drops:

Central Node: Start the Central controller first to establish the traffic/c1 attach-point and commence server loop monitoring.

Railway Nodes: Start the Railway controllers by passing the target ID (1-3) as a command-line argument (e.g., ./railway_controller 1 to initialize RL1).

Intersection Nodes: Start the Intersection controllers by passing the target ID (1-6) as a command-line argument (e.g., ./intersection_controller 1 to initialize L1).

Step 5: Verification: Observe the Central controller's terminal. The 1 Hz c_hmi_render() table should be updated to display all 9 connected controllers. Confirm that their availability status automatically transitions from UNAVAILABLE to AVAILABLE (and internal links transition from DEGRADED_LOCAL to CENTRAL_CONNECTED) as initial 1-second heartbeats are acknowledged.

Step 6: Shutdown Procedure: To safely halt the system, input the q command in each terminal to cleanly break the blocking stdin sensor and operator reader threads. Follow this with a standard OS interrupt (Ctrl+C) to terminate the underlying infinite server loops and exit the processes.

## 2.4 Operator and Sensor Controls

To facilitate testing and demonstration without physical hardware, the system utilizes dedicated stdin reader threads across all three controller types to capture keyboard inputs. The following tables define the specific command mappings and expected behaviors for the Central, Intersection, and Railway nodes.

*Table 4. Central Controller Operator Commands*

| Input / command | Purpose | Expected observable result |
| --- | --- | --- |
| m | Requests a mode change (PEAK_FIXED or OFF_PEAK_SENSOR) for a target Lx | SET_MODE command submitted; target updates mode at next safe boundary |
| t | Broadcasts a timing profile to an arterial chain (R1 or R2) | SET_TIMING_PROFILE submitted; coordinated offsets applied across the chain |
| o | Applies a clear-route override for a target intersection and movement <br> | REQUEST_OVERRIDE submitted; target movement receives green for the specified duration |
| r / c | Renews or cancels an active clear-route override on a target Lx | RENEW_OVERRIDE extends timer; CANCEL_OVERRIDE terminates override via safe clearance |
| f | Requests fault clearance for a Lx or RLx after a physical repair | REQUEST_FAULT_CLEAR submitted; target clears fault and resumes normal readiness if safe |
| d / a | Forces a simulated hour (d) or resumes real-time automatic switching (a) | Peak-hour mode is broadcasted based on the forced hour or restored local clock |
| q | Safely stops the operator console thread | Console thread terminates without stopping the main IPC server loop |

*Table 5. Intersection Sensor Controls*

| Input / command | Purpose | Expected observable result |
| --- | --- | --- |
| a / A | Asserts (a) or clears (A) arterial vehicle presence | Arterial demand triggers or extends green in OFF_PEAK_SENSOR mode. |
| c / C | Asserts (c) or clears (C) connector vehicle presence | Connector demand triggers or extends green in OFF_PEAK_SENSOR mode. |
| 1, 2, 3, 4 | Simulates pedestrian button presses for crossing sides 0 through 3 | Demand is latched and the WALK/FLASHING_DONT_WALK sequence initiates on the next compatible phase |
| w / W | Asserts (w) or clears (W) the advance queue-warning sensor | Triggers a 4-second connector drain extension after railway preemption concludes |
| q | Safely stops the keyboard input thread | Sensor reading terminates without stopping the intersection's phase sequencing loop |

*Table 6. Railway Sensor and Demo Controls*

| Input / command | Purpose | Expected observable result |
| --- | --- | --- |
| 0 / 1 | Simulates a train approaching from direction 0 or 1 | Crossing transitions to WARNING, activates flashers, and commands gates down |
| x | Arms the next gate motion to fail confirmation for demo purposes | Simulated fault triggers FAULT_GATE_CONFIRM_MISSING, forcing train signals to STOP |
| r | Simulates physical gate repair to forcibly set a confirmed OPEN state | Allows a subsequent fault-clear command to succeed |
| f | Demo-only local fault-clear trigger bypassing Central | Local fault clears directly if the gate is verified as OPEN |
| q | Safely stops the keyboard input thread | Sensor reading terminates while the railway safety FSM loop continues |

# 3. Implemented System Architecture

Describe actual executable units and ownership. Use the final implementation names, not design-only placeholders.

## 3.1 Runtime Architecture

[Explain how C1, L1-L6, and RL1-RL3 are deployed across the available QNX nodes. Summarise local autonomy and the direct RLx-to-adjacent-Lx safety path.]
[Continue here.]

[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]

*Figure 1. As-built task and deployment architecture*

## 3.2 Major Components and Processes

*Table 5. Implemented processes and responsibilities*

| Node / process | Purpose | Key inputs | Key outputs | Source location |
| --- | --- | --- | --- | --- |
| C1 / [process] | [Supervision, display, logging, dispatch] | [Messages/user input] | [Commands/display/logs] | [path] |
| L1-L6 / [process] | [Intersection control responsibility] | [Sensors/IPC/timers] | [Signals/status/replies] | [path] |
| RL1-RL3 / [process] | [Railway protection responsibility] | [Train/gate inputs] | [Gates/flashers/signals/status] | [path] |
| [Watchdog/input helper] | [Responsibility] | [Inputs] | [Outputs] | [path] |

## 3.3 Concurrency, Timing, and Synchronisation

[Identify threads/tasks, POSIX timers, pulses, mutexes, condition variables, atomic/shared state, scheduling choices, and watchdogs. Explain why each synchronisation primitive is needed and how races/deadlocks are avoided.]
[Continue here.]

*Table 6. Concurrency and synchronisation summary*

| Shared resource / event | Producer(s) | Consumer(s) | Synchronisation mechanism | Rationale |
| --- | --- | --- | --- | --- |
| [State/configuration] | [Task] | [Task] | [Mutex/message/pulse/timer] | [Why this is safe] |
| [Event/queue] | [Task] | [Task] | [Mechanism] | [Ordering/deadline guarantee] |

# 4. Inter-Process Communication

Show both the communication mechanism and the operational meaning. Do not duplicate every C structure if the source code is authoritative; identify the file and summarise fields that matter.

## 4.1 Communication Topology

[Explain Qnet channels, name attachment/discovery, direct peer paths, supervisory paths, synchronous request/reply behaviour, pulses, heartbeat, ACK/NACK, and reconnection synchronisation.]
[Continue here.]

[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]

*Figure 2. Implemented IPC topology*

## 4.2 Message Pathways

*Table 7. Implemented inter-process communication pathways*

| Message / pulse | Source | Destination | Mechanism | Purpose | Reply / timeout |
| --- | --- | --- | --- | --- | --- |
| [MSG_*] | [Process] | [Process] | [MsgSend/MsgReply/pulse] | [Operational purpose] | [ACK/NACK/deadline] |
| [MSG_*] | [Process] | [Process] | [Mechanism] | [Purpose] | [Outcome] |
| [Heartbeat] | [Local process] | [Central process] | [Mechanism] | [Link monitoring] | [Interval/missed count] |

## 4.3 Message Validation and Failure Handling

[Explain message version/size/type validation, controller IDs, sequence numbers, range checks, stale/duplicate handling, ACK/NACK reason codes, timeouts, loss of Central, reconnection, and invalid-command containment.]
[Continue here.]

# 5. Source-Code Organisation

This section is explicitly required by the project brief. Use repository-relative paths and match the submitted release exactly.

## 5.1 Repository Structure

<< Outline the Traffic_Control_System workspace. Show the three projects: central_controller, intersection_controller, and railway_controller.>>

## 5.2 Directory, File, and Module Responsibilities

*Table 8. Source-code organisation*

| Path | Purpose | Main contents / public interface | Used by |
| --- | --- | --- | --- |
| [src/common/] | [Shared definitions/utilities] | [Headers, message structs, helpers] | [Processes] |
| [src/central/] | [Central implementation] | [Main modules] | C1 |
| [src/intersection/] | [Intersection implementation] | [FSM, input, signals, IPC] | L1-L6 |
| [src/railway/] | [Railway implementation] | [Protection FSM, drivers, IPC] | RL1-RL3 |
| [tests/ or scripts/] | [Test/simulation support] | [Vectors, launch scripts] | [Developer/assessor] |
| [config/] | [Runtime configuration] | [Node IDs, timing profiles] | [Processes] |

# 6. Key Implementation Decisions

Select decisions that materially affect correctness, timing, safety, portability, or assessor understanding. Tie each decision to code and evidence.

*Table 9. Key implementation decisions*

| Decision | Reason | Implementation location | Consequence / trade-off | Evidence |
| --- | --- | --- | --- | --- |
| Local output authority | [Safety/autonomy rationale] | [path:function] | [Central cannot directly actuate outputs] | [Test ID/figure] |
| Direct railway pre-emption path | [Central-independent safety] | [path:function] | [Peer dependency and timeout behaviour] | [Test ID/figure] |
| Synchronous command replies | [Validation/feedback] | [path:function] | [Blocking/timeout handling] | [Test ID/figure] |
| [Additional decision] | [Reason] | [Location] | [Trade-off] | [Evidence] |

# 7. Implemented Features and Operational Evidence

Use a small number of representative screenshots. Each screenshot must have readable labels, a caption, and a short explanation of what it proves.

## 7.1 Normal Traffic and Pedestrian Operation

[Describe the demonstrated feature, the input sequence, and the expected observable behaviour. Reference relevant UC/assumption IDs where useful.]

[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]

*Figure 3. Normal traffic and pedestrian operation*

## 7.2 Railway Protection and Closure Traffic

[Describe approach warning, gate confirmation, train signal, direct crossing status, suppression, and queue drain evidence.]

[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]

*Figure 4. Railway protection and adjacent-intersection response*

## 7.3 Central Monitoring, Commands, and Local Autonomy

[Show status aggregation, validated command outcome, heartbeat loss, DEGRADED_LOCAL behaviour, and reconnection state synchronisation.]

[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]

*Figure 5. Central monitoring and degraded-local operation*

# 8. Verification and Test Results

Report tests that were actually executed. Include normal, boundary, conflict, failure, and recovery cases. Screenshots alone are not a pass criterion; state the expected result and observed result.

## 8.1 Test Environment and Method

<< Describe your local testing approach (e.g., running multiple QNX targets/VMs concurrently, manual keyboard injection). >>

## 8.2 Requirements and Scenario Coverage

*Table 10. Representative test results*

| Test ID | Requirement / UC | Input and precondition | Expected result | Observed result / evidence | Status |
| --- | --- | --- | --- | --- | --- |
| T-01 | [UC/assumption] | [Setup and stimulus] | [Observable criterion] | [Log/figure/path] | [Pass/Fail] |
| T-02 | [UC/assumption] | [Setup and stimulus] | [Criterion] | [Evidence] | [Pass/Fail] |
| T-03 | [Fault/recovery case] | [Setup and stimulus] | [Safe/recovery criterion] | [Evidence] | [Pass/Fail] |

## 8.3 Timing and Real-Time Behavior

*Table 11. Representative timing results*

| Measurement | Method | Required / configured value | Observed result | Interpretation |
| --- | --- | --- | --- | --- |
| [Command response] | [Timestamp/log method] | [Deadline] | [Min/max/typical] | [Pass and limitations] |
| [Heartbeat detection] | [Method] | [Interval/threshold] | [Observed] | [Pass and limitations] |
| [Signal/rail timing] | [Method] | [Configured value] | [Observed] | [Tolerance/explanation] |

## 8.4 Faults, Limitations, and Unresolved Results

*Table 12. Know issues and limitations*

| Issue / failed test | Impact | Current mitigation | Reproduction / evidence | Future correction |
| --- | --- | --- | --- | --- |
| [Issue or None identified] | [Functional/safety/demo impact] | [Mitigation] | [Steps/log] | [Planned fix] |
| [Known limitation] | [Impact] | [Accepted scope boundary] | [Evidence] | [Future work] |

# 9. Conclusion

Target: one concise paragraph. State what the implementation demonstrates and any material limitation; do not introduce new technical claims.

<< Summarize the successful implementation of the distributed QNX system. Reiterate that the core safety requirements (railway preemption, decentralized intersection autonomy) were achieved.>>

# References

Use RMIT Easy Cite IEEE format. Cite the project brief, QNX documentation, standards, libraries, or external code actually relied upon. Do not cite the team's Initial Design Report as external authority; cross-reference it internally by section/figure instead.

[1] RMIT University, "RTS_Final Project.pdf," EEET2588 Real-Time Systems design project brief, School of Engineering, Melbourne, VIC, Australia, 2026. [Online]. Available: [Canvas URL]. [Accessed: DD Mon. YYYY].
[2] [QNX documentation or other implementation source.]

# Appendix A. Complete Build and Launch Commands

Optional. Include only if the commands are too long for Section 2. Keep secrets, personal paths, and machine-specific credentials out of the report.

[Commands, expected output, and troubleshooting notes.]
[Continue here.]
[Continue here.]

# Appendix B. Additional Test Evidence

Optional. Store verbose logs, full test vectors, and secondary screenshots here; keep the representative evidence in Section 8.

[Additional evidence indexed by Test ID.]
[Continue here.]
[Continue here.]

Marking Rubric

| PROJECT DEMONSTRATION <br>- Clear, well-organised and professional demonstration of the implemented solution and its key features and scenarios.<br>- Demonstrates sound understanding of the system, including its operation, design decisions, implementation and testing.<br>- Effectively responds to Q&A, including explaining, justifying and re-demonstrating aspects of the solution when requested by the assessor.<br>- Individual Performance & Contribution, including demonstrated understanding, participation and ability to explain details in implementation. | 50 to >39.5 pts HD<br>Outstanding and unequivocally excellent demonstration, providing comprehensive and convincing evidence of achievement. Demonstrates exceptional technical understanding and responds confidently, accurately and effectively during Q&A. Individual performance and contribution are clearly demonstrated at an exceptional level. | 50 |
| --- | --- | --- |
| OVERALL TECHNICAL OUTCOME<br>- Overall correctness, completeness and quality of the implemented solution against the project requirements.<br>- Appropriate design and integration of system components, real-time behaviors, communication, synchronization and timing mechanisms.<br>- Reliability, robustness, testing and handling of relevant operating conditions or failures.<br>- Quality of the software implementation, including structure, maintainability and appropriate use of QNX/POSIX mechanisms.<br>- Technical justification, sophistication and ingenuity where appropriate.<br>- Individual Performance & Contribution | 50 to >39.5 pts HD<br>Outstanding and unequivocally excellent demonstration, providing comprehensive and convincing evidence of achievement. Demonstrates exceptional technical understanding and responds confidently, accurately and effectively during Q&A. Individual performance and contribution are clearly demonstrated at an exceptional level. | 50 |
