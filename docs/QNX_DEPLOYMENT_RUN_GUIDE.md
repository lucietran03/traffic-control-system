# QNX Deployment Run Guide
This document outlines the execution sequence and networking configurations required to deploy the Traffic Control System binaries across QNX targets. It covers standalone, dual-host, and tri-host topologies.

> *CRITICAL QNET REQUIREMENT*
>> **QNET** is not automatically loaded on new QNX x86 VM Targets. You must configure the network adapters and startup scripts to enable transparent distributed processing before executing the binaries.  

## 1. Network & System Pre-requisites
**Goal**: Establish Qnet communication and SSH access across all target VMs.

Follow the `Guide to enable network services and SSH connnection to QNX VM targets.pdf` file provided on canvas Lab 2.

## 2. Binary Transfer & Execution Sequence
**Goal**: Move compiled binaries to the targets and execute them in the correct dependency order.

### 2.0 Build

Build the three binaries either via the QNX Momentics IDE (`docs/QNX_MOMENTICS_INTEGRATION.md` sections 2-4), or from the command line with `make` (from a QNX SDP shell/terminal, repo root):

```
make        # produces build/bin/c_main, build/bin/lx_main, build/bin/rlx_main
```

The IDE build and the `make` build produce the same three binaries with the same names (`c_main`, `lx_main`, `rlx_main`); only the output directory differs (`build/x86_64-debug/` for the IDE vs. `build/bin/` for `make`). Adjust the transfer step below accordingly depending on which build path you used.

### 2.1 File Transfer
- 1. Open the Target File System Navigator in Momentics (`Window -> Show View -> QNX Target File System Navigator`).  
- 2. Drag and drop the compiled binaries (`c_main, lx_main, rlx_main`) from its respective project's build output directory into the target VM's `/tmp` directory.

### 2.2 Cross-node addressing: `TRAFFIC_NODE_MAP` (required for any topology below Case 1's single-VM shortcut)

`app/shared/src/qnet_utils.c` registers every controller's Qnet attach
point in the **global** name-service namespace
(`NAME_FLAG_ATTACH_GLOBAL`, e.g. `traffic/c1` under
`/dev/name/global/...`), and each node resolves its peers by consulting
the `TRAFFIC_NODE_MAP` environment variable at startup. **Whenever a
controller you're about to run needs to reach a peer that lives on a
*different* VM/target than itself, export `TRAFFIC_NODE_MAP` in that
same SSH shell before launching the binary** — otherwise that peer is
treated as "same node" and the connection silently fails
(`name_open()` returns `-1`; handled gracefully, but the message never
arrives). See `app/shared/README.md`'s "Cross-node resolution" section
for exactly how the variable is parsed and how the resulting path is
built.

Format: a comma-separated list of `<suffix>=<qnet-nodename>` pairs,
where `<suffix>` is the lowercase controller suffix (`c1`, `l1`-`l6`,
`rl1`-`rl3`) and `<qnet-nodename>` is whatever `ls /net` shows for that
target from the node you're launching on. Set it once per shell, e.g.:

```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"
/tmp/c_main
```

If every controller you're deploying runs in the same process/VM (a
single-machine smoke test with just one binary running, or a topology
where a node never needs to reach a peer on another VM), you can leave
`TRAFFIC_NODE_MAP` unset — it then behaves exactly as a same-node
`name_open()` always has.

### 2.3 Execution Order
Run the processes via SSH terminals in this strict sequence to ensure Qnet name attachment dependencies are met:
- 1. **Start Central (`C1`)**: Connect to the Central VM. Export `TRAFFIC_NODE_MAP` (see 2.2) if `C1` needs to reach `Lx`/`RLx` peers on other VMs, then run `/tmp/c_main`.
- 2. **Start Railways (`RL1-RL3`)**: Connect to the Railway VMs. Export `TRAFFIC_NODE_MAP` if needed, then run `/tmp/rlx_main`.
- 3. **Start Intersections (`L1-L6`)**: Connect to the Intersection VMs. Export `TRAFFIC_NODE_MAP` if needed, then run `/tmp/lx_main`.

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
- **`TRAFFIC_NODE_MAP`**: every controller is on a different VM from every
  other role, so every node needs the full map (see 2.2):
  `export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"`

### Case 2: Two Computers (Bridged LAN)
The processing load is split across two physical PCs on the same Local Area Network.

- **Setup**: PC A hosts VMs for Central and Intersections. PC B hosts VMs for Railway Controllers.
- **Networking**: VirtualBox Adapter 2 must be set to Bridged Adapter (mapping to the physical Ethernet/Wi-Fi interface) instead of Internal Network, allowing Qnet MAC addresses to be broadcast across the physical LAN.
- **Distribution**:
    - PC A (VM 1): Runs `c_main` and `lx_main`.
    - PC B (VM 2): Runs `rlx_main`.
- **`TRAFFIC_NODE_MAP`**: `c_main` and `lx_main` share PC A's VM (same
  node — `C1 <-> Lx` traffic needs no map entry), only the RLx peers are
  remote. Substitute PC A/PC B's actual VM hostnames:
  `export TRAFFIC_NODE_MAP="rl1=<PC_B_VM_hostname>,rl2=<PC_B_VM_hostname>,rl3=<PC_B_VM_hostname>"`
  on PC A, and `export TRAFFIC_NODE_MAP="c1=<PC_A_VM_hostname>,l1=<PC_A_VM_hostname>,l2=<PC_A_VM_hostname>,l3=<PC_A_VM_hostname>,l4=<PC_A_VM_hostname>,l5=<PC_A_VM_hostname>,l6=<PC_A_VM_hostname>"`
  on PC B.

### Case 3: Three Computers (True Distributed Hardware)
A highly resilient, physically isolated distributed setup.

- **Setup**: Three distinct PCs, each running their own QNX VM target.
- **Networking**: VirtualBox Adapter 2 set to **Bridged Adapter** on all three PCs. Verify all PCs are on the same subnet and that `ls /net` on any VM successfully lists the hostnames of the VMs on the other two PCs. 
- **Distribution**:
    - PC 1 (VM 1): Runs `c_main` exclusively (Supervisory).
    - PC 2 (VM 2): Runs `lx_main` instances (Intersection Logic).
    - PC 3 (VM 3): Runs `rlx_main` instances (Railway Safety Logic).
- **`TRAFFIC_NODE_MAP`**: identical shape to Case 1 (one role per node),
  just with each VM's actual Qnet node name (from `ls /net`) substituted
  in place of `VM_x86_Target0N`:
  `export TRAFFIC_NODE_MAP="c1=<PC1_hostname>,l1=<PC2_hostname>,l2=<PC2_hostname>,l3=<PC2_hostname>,l4=<PC2_hostname>,l5=<PC2_hostname>,l6=<PC2_hostname>,rl1=<PC3_hostname>,rl2=<PC3_hostname>,rl3=<PC3_hostname>"`