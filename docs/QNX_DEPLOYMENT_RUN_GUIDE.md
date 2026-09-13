# QNX Deployment Run Guide

This document outlines the execution sequence and networking configurations required to deploy the Traffic Control System binaries across QNX targets. It covers standalone, dual-host, and tri-host topologies using the native IDE output naming.

> *CRITICAL QNET REQUIREMENT*
>> **QNET** is not automatically loaded on new QNX x86 VM Targets. You must configure the network adapters and startup scripts to enable transparent distributed processing before executing the binaries.

## 1. Network & System Pre-requisites
**Goal**: Establish Qnet communication and SSH access across all target VMs.

### 1.1 Hypervisor Networking
- Every VM target (`VM_x86_Target01` through `VM_x86_Target10`) must have **Network Adapter 2** enabled and attached to the same **Internal Network** (named `qnet-lab`) or configured as a **Bridged Adapter** on a physical LAN.
- **IP Allocation**: Ensure all virtual interfaces are up and assigned to the same subnet (e.g., `192.168.56.x`).

### 1.2 Initialize Qnet on Targets
Log into **every active QNX VM terminal** as `root` and execute the network packet driver along with the local name resolution server
```sh
# 1. Mount the transparent distributed processing network protocol driver
mount -T io-pkt lsm-qnet.so

# 2. Run the Global Name Server daemon in Local Namespace fallback mode
gns -l &
```
To verify that all nodes have discovered each other across the network, execute:
```sh
ls /net
```
**Success Criteria**: The terminal must output your current node hostname (and all other active VM hostnames if running on multiple VMs).

### 1.3 Execution Directory Constraint
**Do not execute binaries from the root (`/`) directory**. The root file system is often Read-Only, which causes the logger initialization to throw `could not open central_log.txt`. 

Always move your binaries into `/tmp` or `/data/var/tmp` before running:
```sh
cd /tmp
#chmod +x Central_Controller Intersection_Controller Railway_Controller
```

## 2. Binary Transfer & Execution Sequence
**Goal**: Move compiled binaries to the targets and execute them in the correct dependency order.

### 2.0 Build
Build the three binaries either via the QNX Momentics IDE (producing outputs in `build/x86_64-debug/`), or from the command line using `make` at the repo root (producing outputs in `build/bin/`):

*   **IDE Outputs**: `Central_Controller`, `Intersection_Controller`, `Railway_Controller`
*   **Command Line Outputs**: `build/bin/Central_Controller`, `build/bin/Intersection_Controller`, `build/bin/Railway_Controller`

### 2.1 File Transfer Mapping
Using the QNX Target File System Navigator inside Momentics, transfer the compiled binaries from your host machine into the target `/tmp` directories according to this exact node deployment matrix:

| Target Hostname | Role Identifier | File to Upload | Execution Subsystem |
|:---|:---|:---|:---|
| `VM_x86_Target01` | `C1` | `Central_Controller` | Central Supervisor Node |
| `VM_x86_Target02` | `L1` | `Intersection_Controller` | Intersection Node 1 |
| `VM_x86_Target03` | `L2` | `Intersection_Controller` | Intersection Node 2 |
| `VM_x86_Target04` | `L3` | `Intersection_Controller` | Intersection Node 3 |
| `VM_x86_Target05` | `L4` | `Intersection_Controller` | Intersection Node 4 |
| `VM_x86_Target06` | `L5` | `Intersection_Controller` | Intersection Node 5 |
| `VM_x86_Target07` | `L6` | `Intersection_Controller` | Intersection Node 6 |
| `VM_x86_Target08` | `RL1` | `Railway_Controller` | Railway Node 1 |
| `VM_x86_Target09` | `RL2` | `Railway_Controller` | Railway Node 2 |
| `VM_x86_Target10` | `RL3` | `Railway_Controller` | Railway Node 3 |

### 2.2 Cross-Node Addressing (`TRAFFIC_NODE_MAP`)
`qnet_utils.c` registers every controller's Qnet attach point globally (e.g., `traffic/c1` under `/dev/name/global/...`). Because every controller now resides on a separate VM target, **you must export the complete cluster map in EVERY active shell terminal before running its binary.**

Copy and paste this exact unified cluster map variable into your terminals:

```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target03,l3=VM_x86_Target04,l4=VM_x86_Target05,l5=VM_x86_Target06,l6=VM_x86_Target07,rl1=VM_x86_Target08,rl2=VM_x86_Target09,rl3=VM_x86_Target10"
```

### 2.3 Strict Execution Order & Runtime Arguments
Open 10 separate terminal windows and execute the nodes in this precise order to avoid initialization handshaking and name-service registration drops:

### Step 0: Clean Up Stale Deployments
Before triggering a new execution run, forcefully terminate any residual crashing, detached background tasks, or dangling IPC handles:
```sh
slay -f Central_Controller Intersection_Controller Railway_Controller 2>/dev/null
```

#### Step 1: Start Central Supervisor (`C1`)
Connect to `VM_x86_Target01`. Setup the network cluster environment map, grant executable permissions, and run. Central does not require any suffix arguments:

```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target03,l3=VM_x86_Target04,l4=VM_x86_Target05,l5=VM_x86_Target06,l6=VM_x86_Target07,rl1=VM_x86_Target08,rl2=VM_x86_Target09,rl3=VM_x86_Target10"
chmod +x /tmp/Central_Controller
/tmp/Central_Controller
```

#### Step 2: Start Railway Crossing Safety Nodes (`RL1-RL3`)
Connect to targets 8, 9, and 10. Pass the exact unique string identifier via `argv[1]` so each standalone node initializes its specific regional track bounds and fsm timers:

- **On `VM_x86_Target08` (Railway Node 1)**:
  ```sh
  export TRAFFIC_NODE_MAP="..." # Paste full map string from 2.2
  chmod +x /tmp/Railway_Controller
  /tmp/Railway_Controller 1
  ```
- **On `VM_x86_Target09` (Railway Node 2)**:
  ```sh
  export TRAFFIC_NODE_MAP="..."
  chmod +x /tmp/Railway_Controller
  /tmp/Railway_Controller 2
  ```
- **On `VM_x86_Target10` (Railway Node 3)**:
  ```sh
  export TRAFFIC_NODE_MAP="..."
  chmod +x /tmp/Railway_Controller
  /tmp/Railway_Controller 3
  ```

#### Step 3: Start Intersection Local Controllers (`L1-L6`)
Connect to targets 2 through 7. Pass the unique intersection instance code as a command argument so the thread loops latch onto the correct hardware input and pedestrian pin matrices:

- **On `VM_x86_Target02` (Intersection 1)**:
  ```sh
  export TRAFFIC_NODE_MAP="..." # Paste full map string from 2.2
  chmod +x /tmp/Intersection_Controller
  /tmp/Intersection_Controller 1
  ```
- **On `VM_x86_Target03` (Intersection 2)**:
  ```sh
  /tmp/Intersection_Controller 2
  ```
- **On `VM_x86_Target04` (Intersection 3)**:
  ```sh
  /tmp/Intersection_Controller 3
  ```
- **On `VM_x86_Target05` (Intersection 4)**:
  ```sh
  /tmp/Intersection_Controller 4
  ```
- **On `VM_x86_Target06` (Intersection 5)**:
  ```sh
  /tmp/Intersection_Controller 5
  ```
- **On `VM_x86_Target07` (Intersection 6)**:
  ```sh
  /tmp/Intersection_Controller 6
  ```

## 3. Deployment Topologies

Depending on your physical hardware availability and laboratory environment constraints, the 10-node distributed system can be deployed in two major distinct structural cluster topologies.

### Case 1: Single Workstation Sandbox (10 Independent VMs on One PC)
All 10 discrete QNX nodes run simultaneously as isolated virtual machine targets hosted within a single high-performance physical computer/workstation.

*   **Hypervisor Setup**: Create/clone and launch exactly 10 standalone virtual machines simultaneously (`VM_x86_Target01` through `VM_x86_Target10`) using VirtualBox managed via QNX Momentics IDE.
*   **Networking Isolation**: Configure **Network Adapter 2** on ALL 10 virtual machines strictly to **Internal Network** with the matching wire channel name **`qnet-lab`**. This creates an isolated virtual local broadcast domain inside the host memory.
*   **Cluster Mapping Strategy**: Because every system entity lives on its own dedicated VM node, you can copy and use the native hostname map uniformly across all 10 virtual machines:
    ```sh
    export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target03,l3=VM_x86_Target04,l4=VM_x86_Target05,l5=VM_x86_Target06,l6=VM_x86_Target07,rl1=VM_x86_Target08,rl2=VM_x86_Target09,rl3=VM_x86_Target10"
    ```

### Case 2: Multi-PC Laboratory LAN (True Distributed Hardware Grid)
To stress-test real-time network determinism under physical network load, the 10 virtual machines are distributed across multiple physical laboratory computers connected to the same Local Area Network (LAN) switch.

*   **Setup Example**: 
    *   **PC A**: Hosts `VM_x86_Target01` (Central Supervisor `C1`)
    *   **PC B**: Hosts `VM_x86_Target02` to `VM_x86_Target07` (Intersection Nodes `L1–L6`)
    *   **PC C**: Hosts `VM_x86_Target08` to `VM_x86_Target10` (Railway Nodes `RL1–RL3`)
*   **Networking Configuration**: You **MUST** change VirtualBox Network Adapter 2 from *Internal Network* to **Bridged Adapter** on all 10 virtual machines, binding it directly to the physical Ethernet/Wi-Fi interface of the host PCs. This allows Qnet token-ring MAC frames to propagate across the physical network hardware layer.
*   **Verification**: Execute `ls /net` on any of the target terminals. Every machine must be able to list the hostnames of the other 9 machines distributed across the room.
*   **Cluster Mapping Strategy**: Since the local target hostnames might automatically inherit unique laboratory suffixes depending on the PC workstation configuration, you must check the actual Qnet nodenames using `ls /net` and swap them into the map:
    ```sh
    # Replace the placeholders below with the real network hostnames visible from your terminal
    export TRAFFIC_NODE_MAP="c1=<Real_Name_Node01>,l1=<Real_Name_Node02>,l2=<Real_Name_Node03>,l3=<Real_Name_Node04>,l4=<Real_Name_Node05>,l5=<Real_Name_Node06>,l6=<Real_Name_Node07>,rl1=<Real_Name_Node08>,rl2=<Real_Name_Node09>,rl3=<Real_Name_Node10>"
    ```

### Case A: 1 VM Sandbox (All 10 Entities on One VM)
Ideal for quick standalone testing and functional verification on a single computer.
- **Mapping Strategy**: All logical components point back to the local device interface.
```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target01,l2=VM_x86_Target01,l3=VM_x86_Target01,l4=VM_x86_Target01,l5=VM_x86_Target01,l6=VM_x86_Target01,rl1=VM_x86_Target01,rl2=VM_x86_Target01,rl3=VM_x86_Target01"
```

### Case B: 2 VM Hybrid Split (Supervisor vs. Combined Workers)
Isolates the central supervisor and visual log telemetry engine from the background worker processes.
- **Mapping Strategy**: `VM_x86_Target01` hosts Central Supervisor. `VM_x86_Target02` hosts all 9 infrastructure worker processes.
```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target02,rl2=VM_x86_Target02,rl3=VM_x86_Target02"
```

### Case C: 3 VM Role Grouping (Functional Domain Topology)
Segments the grid logic by system roles: Supervisor, Road Operations, and Rail Safety Operations.
- **Mapping Strategy**: `VM_x86_Target01` (Central), `VM_x86_Target02` (Road Intersections L1–L6), `VM_x86_Target03` (Railway Crossings RL1–RL3).
```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"
```

### Case D: 4 VM to 9 VM Intermediate Clusters
When scaled across an arbitrary intermediate number of targets, consolidate processes sequentially. Group remaining intersection or railway components into the highest indexed targets available.
- *Example (5-VM Consolidated Grid Sample)*:
```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target03,l4=VM_x86_Target03,l5=VM_x86_Target04,l6=VM_x86_Target04,rl1=VM_x86_Target05,rl2=VM_x86_Target05,rl3=VM_x86_Target05"
```

### Case E: 10 VM Full Hardware Grid (Fully Distributed Top-Tier Cluster)
The target native setup. Each individual control subsystem sits completely isolated on its own independent hardware platform instance.
```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target03,l3=VM_x86_Target04,l4=VM_x86_Target05,l5=VM_x86_Target06,l6=VM_x86_Target07,rl1=VM_x86_Target08,rl2=VM_x86_Target09,rl3=VM_x86_Target10"
```