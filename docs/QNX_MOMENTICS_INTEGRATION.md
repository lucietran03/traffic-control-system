# QNX MOMENTICS 8.0.3 Integration Guide

This document provides a clear, step-by-step guide for integrating the distributed Traffic Control System source code (`.c` and `.h` files) into the QNX Momentics IDE (version 8.0.3) for QNX Software Development Platform 7.1, building the projects, and preparing them for target deployment.

> *ALTERNATIVE: COMMAND-LINE BUILD*
>> This repo also ships a versioned `Makefile` at the repo root, which builds all three binaries with a single `make` command from a QNX SDP shell/terminal (no IDE project setup required) - see section 4 below and `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`. The manual IDE setup described in this document still works and is not being replaced; use whichever workflow your team prefers.

The system consists of three independent executable applications:
- Central Controller (`Central_Controller`)
- Intersection Controller (`Intersection_Controller`)
- Railway Controller (`Railway_Controller`)

> *WORKSPACE WARNING*
>> Ensure your workspace directory path contains no spaces (e.g., use `C:\Users\v12010\ide-8.0.3-workspace`) to prevent unintelligent compiler and makefile errors.

## 1. Environment & Workspace Initialization
**Goal**: Prepare the QNX Momentics IDE to host the distributed project files.

| Step | Action | Description |
|------|--------|-------------|
| 1.1 | Lanch IDE | Open QNX Momentics IDE from the QNX Software Center. |
| 1.2 | Set Workspace | Select a dedicated workspace folder without spaces. |
| 1.3 | Check Defender | If prompted by Microsoft Defender, select "Exclude Momentics IDE from being scanned to improve performance". |

## 2. Project Creation & File Integration
**Goal**: Create the project container and link the distributed `.c` and `.h` file architecture.

| Project & Executable Name | Main Source File | Internal Includes Folder |
|:---|:---|:---|
| `Central_Controller` | `src/c_main.c` | `includes/` (contains `c_*.h`) |
| `Intersection_Controller` | `src/lx_main.c` | `includes/` (contains `lx_*.h`) |
| `Railway_Controller` | `src/rlx_main.c` | `includes/` (contains `rlx_*.h`) |


| Step | Action | Description |
|------|--------|-------------|
| 2.1 | New Project | Navigate to `File -> New -> QNX C/C++ Project` and select **QNX Executable**. |
| 2.2 | Project Setup | Name the project exactly as `Central_Controller`, `Intersection_Controller`, or `Railway_Controller`. Ensure the language is set to `C` and select the `x86_64` CPU variant (`gcc_ntox86_64`), as required by QNX 7.1. Click Finish. |
| 2.3 | Integrate Headers (`.h`) | In the Project Explorer, create or use the target `includes` folder within each project. Copy your node-specific headers (`c_*.h`, `lx_*.h`, or `rlx_*.h`) and the 3 shared headers (`sys_types.h`, `ipc_msg.h`, `qnet_utils.h`) directly into this folder. |
| 2.4 | Integrate Source (`.c`) | Import the respective `.c` files into the `src` directory within each project:<br>• **Central**: `c_main.c, c_server.c, c_mode_eng.c, c_watchdog_mon.c, c_hmi.c, c_logger.c, c_comm.c, c_operator.c, qnet_utils.c`<br>• **Intersection**: `lx_main.c, lx_sensor.c, lx_timer.c, lx_fsm.c, lx_signal.c, lx_comm.c, lx_watchdog.c, qnet_utils.c`<br>• **Railway**: `rlx_main.c, rlx_sensor.c, rlx_timer.c, rlx_fsm.c, rlx_gate.c, rlx_signal.c, rlx_comm.c, rlx_watchdog.c, qnet_utils.c` |
| 2.5 | Configure Include Paths | Right-click each project -> `Properties -> C/C++ General -> Paths and Symbols`. Under the **Includes** tab, select **GNU C** from the Languages list. Click **Add...**, select **Workspace...**, and choose the local `includes` folder. **Must check** both "Add to all configurations" and "Add to all languages". |

*Note: Update the `Makefile` inside each project folder by modifying the include flag (`INCLUDES += -Iincludes`) to match the local directory setup.*

## 3. Target VM Configuration
**Goal**: Generate exactly 3 Virtual Machine Targets (`VM_x86_Target01`, `VM_x86_Target03`, `VM_x86_Target03`) to fully simulate the distributed nodes (Central, Intersection, Railway) and connect the IDE to them for compilation and deployment.

Create the following lauch targets

| Target Name | Intended Executable Deployment |
|:---|:---|
| `VM_x86_Target01` | `Central_Controller` |
| `VM_x86_Target02` | `Intersection_Controller` |
| `VM_x86_Target03` | `Railway_Controller` |

| Step | Action | Description |
|------|--------|-------------|
| 3.1 | New Target | In the launch bar at the top, click dropdown and select New Launch Target. |
| 3.2 | VM Settings | Select QNX Virtual Machine Target. Set Target Name, VM Platform to `vbox`, and CPU Architecture to `x86_64`. |
| 3.3 | Finalise Target | Click Finish. The IDE generates the VM and displays it in the target list. Repeat Steps 3.1 - 3.3 twice more to create 2 other vm. |

## 4. Compilation & Build Process
**Goal**: Compile each distributed nodes into executable binaries.

| Step | Action | Description |
|------|--------|-------------|
| 4.1 | Build Project | Right-click the project name (`Central_Controller`, etc.) in the Project Explorer and select **Build Project**. |
| 4.2 | Monitor Output | Watch the `Console` view for compiler output. If issues arise, clear build artifacts via **Clean Project**, rebuild index via **Index -> Rebuild**, and build again. |
| 4.3 | Locate Binaries | Upon success, executable binaries (`Central_Controller`, `Intersection_Controller`, `Railway_Controller`) are generated under the `Binaries` virtual folder, or physically in `build/x86_64-debug/`. |

### 4.4 Command-line Alternative (`make`)
Instead of building through the IDE GUI, you can open a terminal with the QNX SDP environment sourced (`qcc` on `PATH`) and compile from the repository root:

```bash
make            # builds build/bin/Central_Controller, Intersection_Controller, Railway_Controller
make central    # or build just the Central Controller component
make clean      # remove build outputs
```

## 5. Result

After completing the above steps, the workspace in QNX Momentic should contain:

```
Workspace/
├── Central_Controller/
│   ├── includes/ (c_*.h, shared headers)
│   └── src/ (c_*.c, qnet_utils.c)
├── Intersection_Controller/
│   ├── includes/ (lx_*.h, shared headers)
│   └── src/ (lx_*.c, qnet_utils.c)
├── Railway_Controller/
│   ├── includes/ (rlx_*.h, shared headers)
│   └── src/ (rlx_*.c, qnet_utils.c)
└── VM Targets/
    ├── VM_x86_Target01
    ├── VM_x86_Target02
    └── VM_x86_Target03
```

Each project can now be deployed independently to its corresponding QNX Virtual Machine, enabling simulation of the complete distributed Traffic Control System.