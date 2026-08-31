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
| `sys_types.h` | `app/shared/includes` | Global enumerations and bitmasks | Defines traffic modes (`PEAK_FIXED`), signal aspects, crossing states (`WARNING, CLOSED`), and hardware fault flags. | TODO |
| `ipc_msg.h` | `app/shared/includes` | Qnet message structures | Standardizes payloads for status updates, `SET_OPERATION_MODE`, and `MSG_HEARTBEAT` exchanges. | TODO |
| `qnet_utils.h` | `app/shared/includes` | IPC utility prototypes | Exposes wrappers for QNX `name_attach()`, `MsgSend()`, and `MsgReceive()`. | TODO |
| `c_*.h` | `app/central/includes` | Cental node prototypes | Exposes supervisor mode engine limits, watchdog timeouts, and HMI display formatting functions. | TODO |
| `lx_*.h` | `app/intersection/includes` | Intersection prototypes | Defines FSM states, 90-second timer bounds, and collision interlock definitions. | TODO |
| `rlx_*.h` | `app/railway/includes` | Railway node prototypes | Defines the 45-second warning budget, overlapping occupancy window limits, and gate-confirmed-closed constraints. | TODO |

## 2. Central Supervisory Controller (`C1`)
*Base Path / Location*: `app/central/src`

These files govern network-wide monitoring, configuration distribution, and the operator's digital cockpit display.

| File Name | Description | Core Purpose | Process |
|-----------|-----------|--------------|----------|
| `c_main.c` | Central entry point | Initializes the `C1` node, attaches the Qnet name, and launches all supervisory threads. | TODO |
| `c_server.c` | IPC receive loop | Captures state transitions, fault reports, and heartbeats from the 9 local controllers | TODO |
| `c_mode_eng.c` | Coordination engine | Computes green-wave offsets and issues bounded `REQUEST_OVERRIDE` commands without directly actuating lights. | TODO |
| `c_watchdog_mon.c` | Network monitor | Tracks 1-second heartbeats and flags unreachable controllers as `DEGRADED_LOCAL`. | TODO |
| `c_hmi.c` | Terminal display interface | Renders the text-based view of network-wide traffic signals, pedestrian requests, and railway conditions. | TODO |
| `c_logger.c` | Blackbox persistent logger |Writes timestamped mode changes, commands, and fault events to the QNX `/fs` persistent file system. | TODO |

**Note**: `mon` = `monitor`.

## 3. Intersection Local Controller (`L1-L6`)
*Base Path / Location*: `app/intersection/src`

These files manage the autonomous traffic logic, pedestrian latching, and localized failsafe operation for physical intersections `I1` through `I6`.

| File Name | Description | Core Purpose | Process |
|-----------|-----------|--------------|----------|
| `lx_main.c` | Intersection entry point | Loads the specific ID configuration (e.g., `L1` vs `L2`) and spawns local control threads. | TODO |
| `lx_sensor.c` | Hardware input handler | Debounces keyboard-simulated inputs for vehicle presence, queue detection, and pedestrian push-buttons. | TODO |
| `lx_timer.c` | Phase timing manager | Maintains local timing for fixed cycles, 4-second extensions, and mandatory yellow/all-red clearance intervals. | TODO |
| `lx_fsm.c` | Traffic logic controller | Executes state machines for phase selection, railway pre-emption overlays, and queue drain rules. | TODO |
| `lx_signal.c` | Hardware output driver | Actuates signal states while enforcing the hardcoded collision interlock. | TODO |
| `lx_comm.c` | IPC and heartbeat task | Receives direct `CROSSING_STATUS` from railways, processes Central commands, and sends heartbeats. | TODO |
| `lx_watchdog.c` | Failsafe supervisor | Supervises the `Lx` process and forces a `FLASHING_RED` safe road state if the software hangs. | TODO |

## 5. Shared Logic & Project Documentation

## 4. Railway Level Crossing Controller (`RL1-RL3`)
*Base Path / Location*: `app/railway/src`

These files implement the safety-critical barrier control, flashing warnings, and overlapping train occupancy tracking for crossings `RC1` through `RC3`.

| File Name | Description | Core Purpose | Process |
|-----------|-----------|--------------|----------|
| `rlx_main.c` | Railway entry point | Initializes the crossing controller and its isolated safety threads. | TODO |
| `rlx_sensor.c` | Train approach handler | Captures directional binary train-approach events to trigger pre-emption. | TODO |
| `rlx_timer.c` | Safety timing manager | Tracks the 45-second warning budget, gate closure deadlines, and overlapping train occupancy windows. | TODO |
| `rlx_fsm.c` | Railway safety logic | Executes state machines and enforces the gate-confirmed-closed-before-proceed invariant. | TODO |
| `rlx_gate.c` | Boom-gate actuator | Commands dual gates and validates physical position sensor feedback independently of timers. | TODO |
| `rlx_signal.c` | Flasher & train signal | Actuates road-facing flashing red lights and train-facing `STOP`/`PROCEED` signals. | TODO |
| `rlx_comm.c` | Peer and central IPC | Broadcasts crossing status directly to adjacent intersections and handles Central fault-clear requests. | TODO |
| `rlx_watchdog.c` | Failsafe supervisor | Forces boom gates down and train signals to `STOP` if the primary railway process fails. | TODO |

## 5. Shared Logic
*Base Path / Location*: `app/shared/src`

These files provide the IPC infrastructure execution.

| File Name | Description | Core Purpose | Process |
|-----------|-----------|--------------|----------|
| `qnet_utils.c` | IPC implementation | Executes logic for setting up Qnet channels, binding ports, and safely handling message transmission errors. | TODO |
