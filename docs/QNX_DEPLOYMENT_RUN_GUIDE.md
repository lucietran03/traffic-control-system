# QNX Deployment Run Guide
This document outlines the execution sequence and networking configurations required to deploy the Traffic Control System binaries across QNX targets. It covers standalone, dual-host, and tri-host topologies.

> *CRITICAL QNET REQUIREMENT*
>> **QNET** is not automatically loaded on new QNX x86 VM Targets. You must configure the network adapters and startup scripts to enable transparent distributed processing before executing the binaries.  

## 1. Network & System Pre-requisites
**Goal**: Establish Qnet communication and SSH access across all target VMs.

Follow the `Guide to enable network services and SSH connnection to QNX VM targets.pdf` file provided on canvas Lab 2.

## 2. Binary Transfer & Execution Sequence
**Goal**: Move compiled binaries to the targets and execute them in the correct dependency order.

### 2.1 File Transfer
- 1. Open the Target File System Navigator in Momentics (`Window -> Show View -> QNX Target File System Navigator`).  
- 2. Drag and drop the compiled binaries (`c_main, lx_main, rlx_main`) from its respective project's build output directory into the target VM's `/tmp` directory.

### 2.2 Execution Order
Run the processes via SSH terminals in this strict sequence to ensure Qnet name attachment dependencies are met:
- 1. **Start Central (`C1`)**: Connect to the Central VM. Run `/tmp/c_main`.  
- 2. **Start Railways (`RL1-RL3`)**: Connect to the Railway VMs. Run `/tmp/rlx_main`.  
- 3. **Start Intersections (`L1-L6`)**: Connect to the Intersection VMs. Run `/tmp/lx_main`.

## 3. Deployment Topologies
Depending on your physical hardware availability, the distributed QNX nodes can be deployed in three different configurations.

### Case 1: Single Computer (Virtual Network)
All QNX nodes run as virtual machines hosted on one physical PC.
- **Setup**: Generate 2-3 VirtualBox VMs (`VM_x86_Target01, VM_x86_Target02, VM_x86_Target03`) via QNX Momentics.  
- **Networking**: Rely exclusively on VirtualBox's Internal Network (`qnet-la`b) for Adapter 2.  
- **Distribution**:
    - `VM_x86_Target01`: Runs `c_main` (Central).
    - `VM_x86_Target02`: Runs `lx_main` (`L1-L6` instances).
    - `VM_x86_Target03`: Runs `rlx_main` (`RL1-RL3` instances).

### Case 2: Two Computers (Bridged LAN)
The processing load is split across two physical PCs on the same Local Area Network.

- **Setup**: PC A hosts VMs for Central and Intersections. PC B hosts VMs for Railway Controllers.
- **Networking**: VirtualBox Adapter 2 must be set to Bridged Adapter (mapping to the physical Ethernet/Wi-Fi interface) instead of Internal Network, allowing Qnet MAC addresses to be broadcast across the physical LAN.
- **Distribution**:
    - PC A (VM 1): Runs `c_main` and `lx_main`.
    - PC B (VM 2): Runs `rlx_main`.

### Case 3: Three Computers (True Distributed Hardware)
A highly resilient, physically isolated distributed setup.

- **Setup**: Three distinct PCs, each running their own QNX VM target.
- **Networking**: VirtualBox Adapter 2 set to **Bridged Adapter** on all three PCs. Verify all PCs are on the same subnet and that `ls /net` on any VM successfully lists the hostnames of the VMs on the other two PCs. 
- **Distribution**:
    - PC 1 (VM 1): Runs `c_main` exclusively (Supervisory).
    - PC 2 (VM 2): Runs `lx_main` instances (Intersection Logic).
    - PC 3 (VM 3): Runs `rlx_main` instances (Railway Safety Logic).