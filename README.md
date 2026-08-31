# QNX Traffic Control System

A real-time traffic light control system designed and implemented for the **EEET2588 Real-Time Systems** design project.

The system models a distributed road traffic network with multiple signalised intersections and railway crossings. The proof-of-concept is implemented using the **QNX Neutrino Real-Time Operating System** (specifically QNX 7.1) with **C**, QNX inter-process communication mechanisms (`Qnet`), and synchronisation primitives.

## Project Scope

The complete system design covers:

* Five roads: `R1–R5`
* Six signalised road intersections: `I1–I6`
* Multiple railway crossings across a double-track railway corridor
* Local intersection controllers
* Railway-crossing controllers
* A central traffic controller
* Vehicle sensing
* Pedestrian crossing requests
* Fixed-timing and sensor-driven traffic operation
* Railway-crossing interaction and fault handling
* Traffic coordination and congestion-management logic
* Communication-failure and degraded-operation scenarios

## High-Level Architecture

The system utilizes a strict distributed hierarchy to guarantee real-time safety. Each local controller remains responsible for its own intersection and must continue operating safely if communication with the central controller is unavailable.

```text
                        Central Controller
                               C1
                                |
            +-------------------+-------------------+
            |                                       |
   Road Intersection Controllers          Railway Controllers
      L1 L2 L3 L4 L5 L6                    RL1 RL2 RL3
       |  |  |  |  |  |                     |   |   |
      I1 I2 I3 I4 I5 I6                   RC1 RC2 RC3
```

### Critical Design Principles
- **Safety & Intersection Logic**: Modules managing the hardware conflict matrix, 90-second phase cycles, and 45-second railway warning budgets must never block waiting for Central communication.  
- **Supervisory Logic**: The Central controller is strictly for monitoring, configuring, and requesting bounded overrides; it must never directly actuate a physical signal or boom gate.

## Repository Structure

The codebase is organized strictly into independent QNX executables and shared IPC contracts.

```text
traffic-control-system/
├── app/
│   ├── central/          # Supervisory node (c_main, c_mode_eng, c_hmi)
│   ├── intersection/     # Autonomous road traffic logic (lx_main, lx_fsm, lx_timer)
│   ├── railway/          # Safety-critical barrier control (rlx_main, rlx_gate, rlx_fsm)
│   └── shared/           # Qnet IPC implementation, shared payloads, and sys_types.h
├── docs/                 # Official briefs, system diagrams, and integration guides
└── README.md             
```

For a complete, file-by-file breakdown of the `.c` and `.h` dependencies, see the [QNX Project File Overview](`docs/QNX_PROJECT_FILE_OVERVIEW.md`).  

## Development Environment

* QNX Neutrino RTOS
* QNX Momentics IDE
* C / C++
* Git for source control
* Multiple QNX nodes for distributed execution

Team members may use different host operating systems. The shared source code is maintained in this repository, while each member maintains their own local QNX development and target environment.

## Development Workflow

1. Clone the repository into the local QNX development environment.
2. Import or open the relevant project in QNX Momentics.
3. Create a feature branch for development.
4. Build and test changes locally.
5. Commit and push changes to the shared repository.
6. Merge completed work through the agreed team workflow.

Example:

```bash
git checkout -b feature/intersection-controller
git add .
git commit -m "Implement local intersection controller"
git push origin feature/intersection-controller
```

## Getting Started

The project relies on **QNX Momentics IDE 8.0.3** and **VirtualBox** for target deployment. Please follow our dedicated documentation guides in sequence to set up, build, and run the project:

### 1. IDE Setup & Compilation
- **Guide**: [QNX Momentics 8.0.3 Integration](docs/QNX_MOMENTICS_INTEGRATION.md).
- **Summary**: Instructions on setting up a clean workspace (ensuring no spaces in the directory path to prevent makefile errors), importing the `app/` folders into three distinct QNX Executable projects, linking shared headers, and building the `c_main`, `lx_main`, and `rlx_main` binaries.  

### 2. VM Network Configuration
- **Guide**: [QNX Deployment Run Guide](docs/QNX_DEPLOYMENT_RUN_GUIDE.md).
- **Summary**: Because QNET is not automatically loaded on new QNX x86 VM targets, this guide covers modifying the VirtualBox adapters (`qnet-lab` internal network or Bridged LAN) and editing the QNX `startup.sh` / `start_net.s`h scripts to enable transparent distributed processing.  

### 3. Execution Sequence
Once compiled and transferred to the `/tmp` directory of your target VMs, the processes must be executed via SSH in this strict sequence to ensure proper Qnet name attachment dependencies:  
- 1. **Start Central (`C1`)**: Connect to the Central VM and run `/tmp/c_main`.
- 2. **Start Railways (`RL1-RL3`)**: Connect to the Railway VMs and run `/tmp/rlx_main`.  
- 3. **Start Intersections (`L1-L6`)**: Connect to the Intersection VMs and run `/tmp/lx_main`.  

## Deployment Topologies

Depending on hardware availability, the system supports three deployment cases:  
1. **Single Computer (Virtual Network)**: 3 VMs (`VM_x86_Target01, 02, 03`) running locally via VirtualBox Internal Network (`qnet-lab`).  
2. **Two Computers (Bridged LAN)**: Split load where PC A hosts Central/Intersections, and PC B hosts the Railway nodes over a bridged physical network.  
3. **Three Computers (True Distributed)**: High-resiliency setup mapping one VM per physical PC across the same subnet.

## Team

EEET2588 Real-Time Systems Project Team

RMIT University
