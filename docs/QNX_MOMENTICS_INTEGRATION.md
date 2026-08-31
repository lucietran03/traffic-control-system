# QNX MOMENTICS 8.0.3 Intergration
This document provides a clear, step-by-step guide for integrating the distributed Traffic Control System source code (`.c` and `.h` files) into the QNX Momentics IDE(version 8.0.3) for QNX Software Development Platform 7.1, building the project, and preparing it for target deployment.

The system consists of three independent executable applications:

- Central Controller (`c_main`)
- Intersection Controller (`lx_main`)
- Railway Controller {`rlx_main`}

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

| Project | Executable | Main Source File |
|--------|--------|--------|
| `Cebtral_Controller` | `c_main` | `app/central/src/c_main.c` |
| `Intersection_Controller` | `lx_main` | `app/intersection/src/lx_main.c` |
| `Railway_Controller` | `rlx_main` | `app/railway/src/rlx_main.c` |

| Step | Action | Description |
|------|--------|-------------|
| 2.1 | New Project | Navigate to `File -> New -> QNX Project` and select QNX Executable. |
| 2.2 | Project Setup | Name the project. Ensure the language is set to `C` and select the `x86_64` CPU variant, as required by QNX7.1. Click Finish. |
| 2.3 | Integrate Headers (`.h`) | In the Project Explorer, create the target `includes` folders. Import the following files so they are globally accessible: (`app/shared/includes`: `sys_types.h`, `ipc_msg.h`, `qnet_utils.h`, `app/central/includes`: `c_*.h`, `app/intersection/includes`: `lx_*.h`, `app/railway/includes`: `rlx_*.h`) |
| 2.4 | Integrate Source (`.c`) | Import the respective `.c` files into their `src` directories within the project: (*Central*: `c_main.c, c_server.c, c_mode_eng.c, c_watchdog_mon.c, c_hmi.c, c_logger.c`, *Intersection*: `lx_main.c, lx_sensor.c, lx_timer.c, lx_fsm.c, lx_signal.c, lx_comm.c, lx_watchdog.c`, *Railway*: `rlx_main.c, rlx_sensor.c, rlx_timer.c, rlx_fsm.c, rlx_gate.c, rlx_signal.c, rlx_comm.c, rlx_watchdog.c`, *Shared (All)*: `qne_utils.c`) |
| 2.5 | Configure includes Path | Right-click the project -> `Properties -> C/C++ General -> Paths and Symbols`. Add the `app/shared/includes` directory to the includes tab for each project so the compiler resolves cross-node dependencies and the project's on includes directory (`app/central/includes`: `c_*.h`, `app/intersection/includes`: `lx_*.h`, `app/railway/includes`: `rlx_*.h`).  |

After applying the changes, the compiler will correctly resolve all header dependencies.

## 3. Target VM Configuration
**Goal**: Generate exactly 3 Virtual Machine Targets (`VM_x86_Target01`, `VM_x86_Target03`, `VM_x86_Target03`) to fully simulate the distributed nodes (Central, Intersection, Railway) and connect the IDE to them for compilation and deployment.

Create the following lauch targets

| Target | Purpose |
|--------|--------|
| `VM_x86_Target01` | Cebtral Controller |
| `VM_x86_Target02` | Intersection Controller |
| `VM_x86_Target03` | Railway Controller |

| Step | Action | Description |
|------|--------|-------------|
| 3.1 | New Target | In the launch bar at the top, click dropdown and select New Launch Target. |
| 3.2 | VM Settings | Select QNX Virtual Machine Target. Set Target Name, VM Platform to `vbox`, and CPU Architecture to `x86_64`. |
| 3.3 | Finalise Target | Click Finish. The IDE generates the VM and displays it in the target list. Repeat Steps 3.1 - 3.3 twice more to create 2 other vm. |

## 4. Compilation & Build Process
**Goal**: Compile each distributed nodes into executable binaries.

| Step | Action | Description |
|------|--------|-------------|
| 4.1 | Build Project | Right-click the project name in the Project Explorer and select Build Project. |
| 4.2 | Monitor Output | Watch the `Console` view for the compiler output. If issues arise, check the `Problems` tab for readable error mapping linked directly to specific `.c` or `.h` files. |
| 4.3 | Locate Binaries | Upon success, executable binaries (`c_main, lx_main, rlx_main`) are generated in the Binaries folder under `build/x86_64-debug/`. |

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