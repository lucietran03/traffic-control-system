#  QNX RTOS Traffic Control System File Overview
This document defines the complete source file contract for the distributed QNX real-time traffic control system, encompassing the Central supervisory node, Local intersection controllers, and Railway crossing safety nodes. The structure is optimized to support bounded autonomous operation, safe hardware actuation, and Qnet-based inter-process communication (IPC).

> **CRITICAL WARNING FOR DISTRIBUTED DEPLOYMENT**
>> This document contains the complete implementation contract for all QNX nodes. To ensure safe operation and prevent IPC synchronization failures, please adhere strictly to the following division of responsibilities:
>> - **Safety & Intersection Logic**: Modules managing the hardware conflict matrix, 90-second phase cycles, and 45-second railway warning budgets must never block waiting for Central communication.
>> - **Supervisory Logic**: The Central controller is strictly for monitoring, configuring, and requesting bounded overrides; it must never directly actuate a physical signal or boom gate.

>> All nodes must immediately align their Qnet message payloads, IPC channels, and state enums with the shared header specifications before deploying to the physical targets.

## 1. System-Wide & Node-Specific Header Files (`.h`)
**Scope**: Cross-node data contracts, IPC message definitions, and task prototypes.

These headers guarantee that all distributed processes interpret Qnet payloads and state enums identically.

| File Name | Location | Description | Core Purpose | Process |
|-----------|---------|-----------|--------------|----------|
| `sys_types.h` | `app/shared/includes` | Global enumerations and bitmasks | Defines traffic modes (`PEAK_FIXED`), signal aspects, crossing states (`WARNING, CLOSED`), and hardware fault flags. | Done |
| `ipc_msg.h` | `app/shared/includes` | Qnet message structures | Standardizes payloads for status updates, `SET_MODE`, and `HEARTBEAT` exchanges. | Done |
| `qnet_utils.h` | `app/shared/includes` | IPC utility prototypes | Defines the `traffic/<id>` attach-point naming convention (`ipc_attach_name()`/`ipc_attach()`), the two-thread IPC pattern (`ipc_server_run()`, `ipc_client_post()`/`ipc_client_thread_main()`), and `SIGEV_PULSE` timer setup (`ipc_timer_arm()`). | Done |
| `c_*.h` | `app/central/includes` | Cental node prototypes | Exposes supervisor mode engine limits, watchdog timeouts, HMI display formatting functions, outgoing command-builder prototypes (`c_comm.h`), and the operator-console API (`c_operator.h`). | Done |
| `lx_*.h` | `app/intersection/includes` | Intersection prototypes | Defines FSM states (incl. pedestrian WALK/FDW/DONT_WALK, drain, and override sub-states), 90-second timer bounds, and collision interlock definitions. | Done |
| `rlx_*.h` | `app/railway/includes` | Railway node prototypes | Defines the 45-second warning budget, overlapping occupancy window limits, and gate-confirmed-closed constraints. | TODO |

## 2. Central Supervisory Controller (`C1`)
*Base Path / Location*: `app/central/src`

These files govern network-wide monitoring, configuration distribution, and the operator's digital cockpit display.

| File Name | Description | Core Purpose | Process |
|-----------|-----------|--------------|----------|
| `c_main.c` | Central entry point | Initializes the `C1` node, attaches the Qnet name, and launches all supervisory threads (server, client, and now the operator-console thread). Owns the `mode_eng_lock` mutex serialising server-thread and operator-thread access to the shared mode-engine view. | Done — two-thread IPC wiring plus the operator-console thread are wired in; business logic hooks (mode engine, watchdog, HMI, logger, comm) are all implemented |
| `c_server.c` | IPC receive loop | Captures state transitions, fault reports, and heartbeats from the 9 local controllers | TODO |
| `c_mode_eng.c` | Coordination engine | Computes green-wave offsets, validates override requests, and builds bounded `REQUEST_OVERRIDE`/`SET_TIMING_PROFILE` command payloads without directly actuating lights or performing any IPC itself. | Done |
| `c_watchdog_mon.c` | Network monitor | Ticks once/second (reuses the existing heartbeat pulse); marks a controller `unavailable` in C1's own view (`marked_unavailable`, not `DEGRADED_LOCAL` - that remains a local-only self-declared state per PA-07) after 3 consecutive missed proof-of-life reports. | Done |
| `c_hmi.c` | Terminal display interface | Renders a text table of all 9 controllers (mode/phase/crossing/supervisory/faults/override/availability) once/second. Known gap: no sensor-status column exists anywhere in the wire contract yet (UC-09 asks for it). | Done |
| `c_logger.c` | Blackbox persistent logger | Writes timestamped fault reports and unavailable-controller events to a local append-only text file (`central_log.txt`) plus stdout - uses portable `<time.h>`, not QNX `/fs` specifically, so it runs identically off-target for this PoC; swap the file path for `/fs/...` on real hardware. | Done |
| `c_comm.c` | Outgoing command builders | Builds and posts every C1 -> Lx/RLx command (`SET_MODE`, `SET_TIMING_PROFILE` incl. full arterial-chain broadcast, `REQUEST_OVERRIDE`, `RENEW_OVERRIDE`, `CANCEL_OVERRIDE`, `REQUEST_FAULT_CLEAR`) via `ipc_client_post()`, logging each eventual ACK/ACK_PENDING/NACK(reason)/ERROR/send-failure through `c_logger.c`. | Done |
| `c_operator.c` | Operator console | Dedicated blocking-stdin thread (mirrors `lx_sensor.c`'s pattern) turning Control Room Operator commands into `c_comm.c` calls - implements the operator-triggered halves of UC-03, UC-06 alt-flow 7.1, UC-07, and UC-08. | Done |

**Note**: `mon` = `monitor`.

## 3. Intersection Local Controller (`L1-L6`)
*Base Path / Location*: `app/intersection/src`

These files manage the autonomous traffic logic, pedestrian latching, and localized failsafe operation for physical intersections `I1` through `I6`.

| File Name | Description | Core Purpose | Process |
|-----------|-----------|--------------|----------|
| `lx_main.c` | Intersection entry point | Loads the specific ID configuration (e.g., `L1` vs `L2`) and spawns local control threads. | Partial — ID selection via argv + two-thread IPC wiring done; FSM/sensor/signal hooks are wired up |
| `lx_sensor.c` | Hardware input handler | Reads keyboard-simulated inputs for vehicle presence, queue detection, and the 4 pedestrian push-buttons (NU-03). | Done |
| `lx_timer.c` | Phase timing manager | Pure timing-policy constants (TL-01/02, CC-03's 60 s drain cap, TC-01/02's derived 90 s `LX_CYCLE_LENGTH_MS`) and the OFF_PEAK_SENSOR exit-guard formula (TL-03/DP-06), factored out of `lx_fsm.c` so phase-selection logic and timing policy are independently readable/testable. No mutex, no state. | Done |
| `lx_fsm.c` | Traffic logic controller | Executes state machines for phase selection, railway pre-emption overlays, pedestrian WALK/FLASHING_DONT_WALK/DONT_WALK sequencing (SC-02/TL-05/TL-06), the CC-03 post-closure connector drain phase, and TC-02/03 green-wave offset application. | Done |
| `lx_signal.c` | Hardware output driver | Printf-based stand-in for actual signal-head actuation: vehicle phase, pedestrian WALK/FLASHING_DONT_WALK/DONT_WALK per side, fault-safe and override-clearance outputs. | Done |
| `lx_comm.c` | IPC and heartbeat task | Receives direct `CROSSING_STATUS` from railways, processes Central commands, and sends heartbeats. | Done |
| `lx_watchdog.c` | Failsafe supervisor | Dedicated 4th thread; if the phase-timer tick counter stalls for 2s, forces `SUPERVISORY_FAULT_SAFE` and the safe-output display directly from the watchdog thread itself (not by waiting for the possibly-hung server thread to notice) - see app/intersection/src/lx_fsm.c's `lx_fsm_report_watchdog_trip()`. | Done |

## 4. Railway Level Crossing Controller (`RL1-RL3`)
*Base Path / Location*: `app/railway/src`

These files implement the safety-critical barrier control, flashing warnings, and overlapping train occupancy tracking for crossings `RC1` through `RC3`.

| File Name | Description | Core Purpose | Process |
|-----------|-----------|--------------|----------|
| `rlx_main.c` | Railway entry point | Initializes the crossing controller and its isolated safety threads. | Partial — ID selection via argv + two-thread IPC wiring done; FSM/gate/sensor hooks are TODO |
| `rlx_sensor.c` | Train approach handler | Captures directional binary train-approach events to trigger pre-emption. | TODO |
| `rlx_timer.c` | Safety timing manager | Pure timing-policy constants (RC-03/RC-04) and an underflow-safe countdown primitive, factored out of `rlx_fsm.c`. No mutex, no state. | Done |
| `rlx_fsm.c` | Railway safety logic | Executes state machines and enforces the gate-confirmed-closed-before-proceed invariant. | TODO |
| `rlx_gate.c` | Boom-gate actuator | Commands dual gates and validates physical position sensor feedback independently of timers. | TODO |
| `rlx_signal.c` | Flasher & train signal | Actuates road-facing flashing red lights and train-facing `STOP`/`PROCEED` signals. | TODO |
| `rlx_comm.c` | Peer and central IPC | Broadcasts crossing status directly to adjacent intersections and handles Central fault-clear requests. | TODO |
| `rlx_watchdog.c` | Failsafe supervisor | Dedicated 4th thread; if the tick counter stalls for 3s, forces the crossing directly into `RLX_FAULT` (via the existing `enter_fault()`) from the watchdog thread itself, under its own lock - independent of whether the server thread is still alive. | Done |

## 5. Shared Logic
*Base Path / Location*: `app/shared/src`

These files provide the IPC infrastructure execution.

| File Name | Description | Core Purpose | Process |
|-----------|-----------|--------------|----------|
| `qnet_utils.c` | IPC implementation | Executes logic for setting up Qnet channels, binding ports, and safely handling message transmission errors. | Done |
