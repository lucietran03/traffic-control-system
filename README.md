# QNX Traffic Control System

A real-time traffic light control system designed and implemented for the **EEET2588 Real-Time Systems** design project.

The system models a distributed road traffic network with multiple signalised intersections and railway crossings. Each intersection is controlled locally, while a central controller provides monitoring, coordination, operating-mode selection, and exceptional override support.

The proof-of-concept is implemented using the **QNX Neutrino Real-Time Operating System** with **C/C++**, QNX interprocess communication mechanisms, and synchronisation primitives.

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

The central controller does not directly control individual traffic lights. Each local controller remains responsible for its own intersection and must continue operating safely if communication with the central controller is unavailable.

## Planned Repository Structure

```text
qnx-traffic-control-system/
├── central/
│   └── Central controller implementation
│
├── intersection/
│   └── Local road-intersection controller
│
├── railway/
│   └── Railway-crossing controller
│
├── common/
│   └── Shared message definitions, enums and utilities
│
├── include/
│   └── Shared header files
│
├── input/
│   └── Simulated sensor and operator input components
│
├── display/
│   └── Terminal-based system state display
│
├── tests/
│   └── Test programs and test scenarios
│
├── docs/
│   └── Design notes, diagrams and implementation documentation
│
└── README.md
```

The directory structure may evolve as the system architecture is refined during the Initial Design phase.

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

## Current Status

**Phase:** Initial Design

Current work focuses on defining:

* System assumptions
* Intersection layouts
* Traffic-light state behaviour
* Railway-crossing behaviour
* Central and local controller responsibilities
* Traffic demand and priority
* Timing requirements
* Traffic coordination
* Congestion control
* Inter-process and inter-node communication architecture
* Failure and fallback behaviour

Implementation details will be progressively added as the design is finalised.

## Team

EEET2588 Real-Time Systems Project Team

RMIT University
