# QNX Deployment Run Guide

This document outlines the execution sequence and networking configurations required to deploy the Traffic Control System binaries across QNX targets. It covers standalone, dual-host, and tri-host topologies using the native IDE output naming.

> *CRITICAL QNET REQUIREMENT*
>> **QNET** is not automatically loaded on new QNX x86 VM Targets. You must configure the network adapters and startup scripts to enable transparent distributed processing before executing the binaries.

## 1. Network & System Pre-requisites
**Goal**: Establish Qnet communication and SSH access across all target VMs.

Follow the `Guide to enable network services and SSH connection to QNX VM targets.pdf` file provided on canvas Lab 2.

## 2. Binary Transfer & Execution Sequence
**Goal**: Move compiled binaries to the targets and execute them in the correct dependency order.

### 2.0 Build
Build the three binaries either via the QNX Momentics IDE (producing outputs in `build/x86_64-debug/`), or from the command line using `make` at the repo root (producing outputs in `build/bin/`):

*   **IDE Outputs**: `Central_Controller`, `Intersection_Controller`, `Railway_Controller`
*   **Command Line Outputs**: `build/bin/Central_Controller`, `build/bin/Intersection_Controller`, `build/bin/Railway_Controller`

### 2.1 File Transfer
1. Open the Target File System Navigator in Momentics (`Window -> Show View -> QNX Target File System Navigator`).
2. Drag and drop the compiled binary files (`Central_Controller`, `Intersection_Controller`, `Railway_Controller`) from their respective project build directories into the target VM's `/tmp` directory.

### 2.2 Cross-Node Addressing (`TRAFFIC_NODE_MAP`)
`qnet_utils.c` registers every controller's Qnet attach point in the global namespace (`NAME_FLAG_ATTACH_GLOBAL`, e.g., `traffic/c1` under `/dev/name/global/...`). Nodes resolve their peers by consulting the `TRAFFIC_NODE_MAP` environment variable at startup.

**Whenever a controller needs to reach a peer residing on a different VM/target, you must export `TRAFFIC_NODE_MAP` in that SSH shell before launching the binary.**

Format: A comma-separated list of `<suffix>=<qnet-nodename>` pairs where `<suffix>` is the lowercase controller identifier (`c1`, `l1`-`l6`, `rl1`-`rl3`) and `<qnet-nodename>` is the target hostname visible via `ls /net`.

```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"
/tmp/Central_Controller
```

### 2.3 Execution Order
Run the processes via individual SSH terminals in this strict sequence to satisfy initialization dependencies:
1. **Start Central (`C1`)**: Connect to `VM_x86_Target01`. Export `TRAFFIC_NODE_MAP` if cross-node communication is required, then execute `/tmp/Central_Controller`.
2. **Start Railways (`RL1-RL3`)**: Connect to `VM_x86_Target03`. Export `TRAFFIC_NODE_MAP`, then execute `/tmp/Railway_Controller`.
3. **Start Intersections (`L1-L6`)**: Connect to `VM_x86_Target02`. Export `TRAFFIC_NODE_MAP`, then execute `/tmp/Intersection_Controller`.

## 3. Deployment Topologies

### Case 1: Single Computer (Virtual Network Setup)
All QNX nodes run as virtual machines hosted on one physical host PC.
- **Setup**: Generate 3 VirtualBox VMs (`VM_x86_Target01`, `VM_x86_Target02`, `VM_x86_Target03`) via QNX Momentics.
- **Networking**: Rely on VirtualBox's Internal Network (`qnet-lab`) for Network Adapter 2.
- **Distribution Mapping**:
    *   `VM_x86_Target01`: Runs `/tmp/Central_Controller`
    *   `VM_x86_Target02`: Runs `/tmp/Intersection_Controller` (Handles L1–L6 contexts)
    *   `VM_x86_Target03`: Runs `/tmp/Railway_Controller` (Handles RL1–RL3 contexts)
- **Environment Mapping**:
  `export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"`

### Case 2: Two Computers (Bridged LAN)
The distributed runtime processing load is split across two physical machines connected via a Local Area Network.
- **Setup**: PC A hosts VMs for Central and Intersections. PC B hosts VMs for Railway Controllers.
- **Networking**: VirtualBox Adapter 2 must be set to **Bridged Adapter** on both host machines to broadcast Qnet MAC frames across the physical LAN.
- **Distribution Mapping**:
    *   PC A (VM 1): Runs `/tmp/Central_Controller` and `/tmp/Intersection_Controller`.
    *   PC B (VM 2): Runs `/tmp/Railway_Controller`.
- **Environment Mapping (PC A)**:
  `export TRAFFIC_NODE_MAP="rl1=<PC_B_VM_hostname>,rl2=<PC_B_VM_hostname>,rl3=<PC_B_VM_hostname>"`
- **Environment Mapping (PC B)**:
  `export TRAFFIC_NODE_MAP="c1=<PC_A_VM_hostname>,l1=<PC_A_VM_hostname>,l2=<PC_A_VM_hostname>,l3=<PC_A_VM_hostname>,l4=<PC_A_VM_hostname>,l5=<PC_A_VM_hostname>,l6=<PC_A_VM_hostname>"`

### Case 3: Three Computers (True Distributed Hardware Environment)
A physically isolated distributed setup mapping each system component to standalone hardware.
- **Setup**: Three distinct physical PCs, each running their own QNX VM target.
- **Networking**: Network Adapter 2 set to **Bridged Adapter** on all machines. Verify connectivity by running `ls /net` on any target to ensure peer hostnames are correctly listed.
- **Distribution Mapping**:
    *   PC 1 (VM 1): Runs `/tmp/Central_Controller` exclusively.
    *   PC 2 (VM 2): Runs `/tmp/Intersection_Controller` exclusively.
    *   PC 3 (VM 3): Runs `/tmp/Railway_Controller` exclusively.
- **Environment Mapping**:
  `export TRAFFIC_NODE_MAP="c1=<PC1_hostname>,l1=<PC2_hostname>,l2=<PC2_hostname>,l3=<PC2_hostname>,l4=<PC2_hostname>,l5=<PC2_hostname>,l6=<PC2_hostname>,rl1=<PC3_hostname>,rl2=<PC3_hostname>,rl3=<PC3_hostname>"`