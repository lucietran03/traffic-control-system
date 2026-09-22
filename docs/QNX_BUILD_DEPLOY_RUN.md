# QNX build, deploy, and run — end to end

One walkthrough from "I have this git repo, nothing else" to "10 controllers
are actually running and I can see them talking to each other." Aligned with
`TeamQNX_submission/TeamQNX_ImplementationNote.pdf` Section 2 (Build,
Deployment, and Operation) — that PDF is the authoritative, already-submitted
record of what was built and how it was run for grading; this file is the
working, teammate-facing version: more incremental (get one node running
before all ten), with the exact shell commands and a troubleshooting table
the PDF doesn't need. Follow it top to bottom the first time; after that,
jump straight to the section you need.

This file replaces `QNX_GETTING_STARTED.md`, `QNX_MOMENTICS_INTEGRATION.md`,
and `QNX_DEPLOYMENT_RUN_GUIDE.md`, which covered this same ground across
three separate documents.

## 0. Prerequisites

- A machine with **QNX Momentics IDE 8.0.3** (QNX SDP 7.1) installed — this
  is where you'll edit/build, not necessarily where the controllers run.
- **VirtualBox**, for hosting the QNX target VM(s).
- Your team's `Guide to enable network services and SSH connection to QNX VM
  targets.pdf` (Canvas, Lab 2) — needed once, in step 4.
- Decide your topology now (affects how many VMs you create in step 3): **1
  VM** (everything on one target, simplest, good for a first smoke test) up
  to **10 VMs** (one per controller — the configuration actually used for
  the submitted demo and documented in the Implementation Note). See
  §6 below for the exact `TRAFFIC_NODE_MAP` value per case. This guide's
  early steps assume the simplest case (1 VM, no cross-node networking) to
  get you to "it runs" fastest; scale up once that works.

## 1. Get the code

```bash
git clone git@github.com-lucietran03:lucietran03/traffic-control-system.git
```

(or `git pull` if you already have a clone — `git status`, then
`git pull origin main`.)

**Workspace path warning**: make sure the path has **no spaces** anywhere in
it — e.g. `C:\Users\you\qnx-workspace\traffic-control-system`, not `.../My
Documents/...`. A space in the path causes unintelligible compiler/makefile
errors later.

## 2. Build

Two build paths exist in this repo, producing **differently-named**
binaries — pick one, and don't mix instructions from the other:

### Option A — command line (`make`), fastest

From a QNX SDP shell/terminal (`qcc` on `PATH` — source your SDP environment
script, e.g. `qnxsdp-env.sh`, if it isn't already):

```bash
make            # builds build/bin/{c_main,lx_main,rlx_main}
```

`make check-syntax` is a **separate, non-QNX** sanity check (plain host
`gcc`, no QNX headers) — useful on a machine with no SDP at all, but it does
**not** prove the real build works; use `make` (or Option B) for that.

### Option B — QNX Momentics IDE

This is the path used for the submitted/graded build (Implementation Note
§2.2), and produces binaries named after the Eclipse project, **not**
`c_main`/`lx_main`/`rlx_main`:

1. `File → Import... → Projects from Folder or Archive` and select the
   submitted `TeamQNX_A2_ProjectSourceCode.zip`, or create three new **QNX
   Executable** projects by hand — `Central_Controller`,
   `Intersection_Controller`, `Railway_Controller` — language `C`, CPU
   `x86_64` (`gcc_ntox86_64`).
2. For each project's `includes/` folder: copy in the node-specific headers
   (`c_*.h`, `lx_*.h`, or `rlx_*.h`) plus the 3 shared headers
   (`sys_types.h`, `ipc_msg.h`, `qnet_utils.h`).
3. For each project's `src/` folder: import that node's `.c` files from
   `app/<node>/src` **plus** `app/shared/src/qnet_utils.c`.
4. `Properties → C/C++ General → Paths and Symbols → Includes → GNU C →
   Add... → Workspace...` and select that project's local `includes` folder.
   Tick **both** "Add to all configurations" and "Add to all languages".
5. Right-click each project → **Build Project**. Binaries land in
   `Binaries`/`build/x86_64-debug/`, named `Central_Controller`,
   `Intersection_Controller`, `Railway_Controller` (matching the project
   name, not the `make`-built binary names above).

**Confirm it actually built**: you should have three real files now,
whichever option you used. If a build fails, check the `Problems` tab (IDE)
or the terminal output (`make`) — don't skip ahead with missing binaries.

## 3. Create your target VM(s)

1. In Momentics, launch-target dropdown → **New Launch Target** → **QNX
   Virtual Machine Target**.
2. Name it (e.g. `VM_x86_Target01`), platform `vbox`, CPU `x86_64`.
3. Finish — the IDE creates and boots the VM.

For a full 10-VM topology (§6, Case E — the configuration used for the
submitted demo), repeat this 10 times, one target per controller:

| Target | Controller | Target | Controller |
|---|---|---|---|
| `VM_x86_Target01` | `C1` (Central) | `VM_x86_Target06` | `L5` |
| `VM_x86_Target02` | `L1` | `VM_x86_Target07` | `L6` |
| `VM_x86_Target03` | `L2` | `VM_x86_Target08` | `RL1` |
| `VM_x86_Target04` | `L3` | `VM_x86_Target09` | `RL2` |
| `VM_x86_Target05` | `L4` | `VM_x86_Target10` | `RL3` |

## 4. Enable Qnet networking on the VM(s)

**This step is easy to forget and the #1 reason "it builds but nothing talks
to anything."** Qnet is **not** on by default on a fresh QNX VM target.

- Follow your team's `Guide to enable network services and SSH connection to
  QNX VM targets.pdf` (Canvas, Lab 2) to configure the network adapter and
  startup scripts.
- On each active QNX VM (as `root`), start Qnet and the name server if the
  startup script doesn't already do it:
  ```sh
  mount -T io-pkt lsm-qnet.so   # transparent distributed processing driver
  gns -l &                      # global name server, local-namespace fallback
  ```
- **Confirm it worked** before moving on:
  ```sh
  ls /net
  ```
  You should see at least the VM's own node name (and every peer's hostname
  once more VMs are up and networked together).
- **Don't run binaries from `/`** — the root filesystem is often read-only,
  which makes `c_logger_init()` fail with `could not open central_log.txt`.
  Always `cd /tmp` (or `/data/var/tmp`) first.

## 5. Transfer the binaries to the target(s)

Quickest path: `Window → Show View → QNX Target File System Navigator` in
Momentics, drag the built binaries from your build output folder into each
target VM's `/tmp/`, then `chmod +x` them.

(If you built with `make` on a machine that already has direct shell/SSH/SCP
access to the VM, `scp build/bin/* qnxuser@<vm>:/tmp/` works too.)

| Target | Binary to upload |
|---|---|
| `VM_x86_Target01` (`C1`) | Central binary (`c_main` or `Central_Controller`) |
| `VM_x86_Target02`–`07` (`L1`–`L6`) | Intersection binary (`lx_main` or `Intersection_Controller`) |
| `VM_x86_Target08`–`10` (`RL1`–`RL3`) | Railway binary (`rlx_main` or `Railway_Controller`) |

## 6. Run it

### 6.1 Simplest possible smoke test first

Before attempting a full topology, confirm ONE binary runs at all. SSH into
your VM:

```sh
/tmp/lx_main 1        # or /tmp/Intersection_Controller 1
```

You should immediately see `Lx 1: SIGNAL -> ARTERIAL GREEN` and, every few
seconds, phase transitions printing on their own (48 s/4 s/2 s/30 s/4 s/2 s
cycle). Press `h` for the key map, `q` to stop. **If you see this, your
build + deploy pipeline works end to end.**

### 6.2 Cross-node addressing (`TRAFFIC_NODE_MAP`)

`qnet_utils.c` registers every controller's Qnet attach point globally
(e.g. `traffic/c1`). Once controllers live on separate VMs, **export the
complete cluster map in every shell before launching its binary**:

```sh
export TRAFFIC_NODE_MAP="c1=<node>,l1=<node>,l2=<node>,l3=<node>,l4=<node>,l5=<node>,l6=<node>,rl1=<node>,rl2=<node>,rl3=<node>"
```

It can be left unset only when the two communicating controllers run on the
same QNX node. Forgetting this is the #2 most common failure — the symptom
is a controller that runs fine standalone but never shows `AVAILABLE` on
Central's table, because `name_open()` silently treated the remote peer as
local and failed.

**Ready-made values per topology:**

| Case | Layout | `TRAFFIC_NODE_MAP` |
|---|---|---|
| A — 1 VM sandbox | everything on `VM_x86_Target01` | `c1=VM_x86_Target01,l1=VM_x86_Target01,l2=VM_x86_Target01,l3=VM_x86_Target01,l4=VM_x86_Target01,l5=VM_x86_Target01,l6=VM_x86_Target01,rl1=VM_x86_Target01,rl2=VM_x86_Target01,rl3=VM_x86_Target01` |
| B — 2 VM split | `Target01`=C1, `Target02`=everything else | `c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target02,rl2=VM_x86_Target02,rl3=VM_x86_Target02` |
| C — 3 VM by role | `Target01`=C1, `Target02`=L1-L6, `Target03`=RL1-RL3 | `c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03` |
| D — 5 VM intermediate | consolidate remaining nodes onto the highest-indexed targets | `c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target03,l4=VM_x86_Target03,l5=VM_x86_Target04,l6=VM_x86_Target04,rl1=VM_x86_Target05,rl2=VM_x86_Target05,rl3=VM_x86_Target05` |
| E — 10 VM full grid | one controller per VM (§3 table) — used for the submitted demo | `c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target03,l3=VM_x86_Target04,l4=VM_x86_Target05,l5=VM_x86_Target06,l6=VM_x86_Target07,rl1=VM_x86_Target08,rl2=VM_x86_Target09,rl3=VM_x86_Target10` |

Multi-PC LAN variant of Case E: swap the `VM_x86_TargetNN` values for the
real Qnet hostnames visible via `ls /net` on each machine, and use a
**Bridged Adapter** (not Internal Network) on every VM's network adapter 2.

For the submitted demo specifically, this map was baked directly into each
VM's `/proc/boot/startup.sh` (inserted just above `-d -t /dev/con1 ksh -l`)
so it's set automatically on boot rather than exported per shell — see the
Implementation Note §2.3 if you need to reproduce that exact setup.

### 6.3 Execution order

Start Central first (so its status table has something to show from the
start), then the rest — order isn't strictly load-bearing
(`ipc_client_post()` fails gracefully and retries per-message), but this
avoids initial connection-drop noise:

```sh
/tmp/c_main &                 # or /tmp/Central_Controller — no arguments
/tmp/rlx_main 1 &              # or /tmp/Railway_Controller 1
/tmp/rlx_main 2 &
/tmp/rlx_main 3 &
/tmp/lx_main 1 &                # or /tmp/Intersection_Controller 1
/tmp/lx_main 2 &
/tmp/lx_main 3 &
/tmp/lx_main 4 &
/tmp/lx_main 5 &
/tmp/lx_main 6 &
```

**Confirm it's working**: `c_main`'s console prints a `---- C1 network
status ----` table once a second. Within ~3 seconds all 9 rows (`L1`-`L6`,
`RL1`-`RL3`) should go `AVAILABLE`, with the internal link transitioning
`DEGRADED_LOCAL → CENTRAL_CONNECTED`. Press keys on any `lx_main`/`rlx_main`
window (`h` for its key map) and watch the effect propagate — e.g. `0` on an
`rlx_main` (train approaching) should make the adjacent `lx_main`s stop
cycling toward the crossing within ~1 second.

**Shutdown**: press `q` in each terminal to cleanly stop that controller's
operator/sensor-input thread, then `Ctrl+C` to terminate the process.

## 7. Operator and sensor key reference

Every controller uses a dedicated keyboard-reader thread, so blocking
terminal input never stalls its IPC server or state-machine processing.
Central's commands prompt for any required parameters after the key is
entered. **Keys are case-sensitive**: for Lx inputs, lowercase asserts a
sensor condition and the matching uppercase clears it.

**Central (`c_main`) operator commands:**

| Key | Action |
|---|---|
| `m` | `SET_MODE` (Peak/Off-Peak) for a selected Lx, applied at the next safe phase boundary |
| `t` | Broadcast a timing profile to arterial chain R1 (L1/L3/L5) or R2 (L2/L4/L6) |
| `o` | `REQUEST_OVERRIDE` (bounded clear-route) for a selected Lx and movement |
| `r` | Renew an active override for a selected Lx |
| `c` | Cancel an active or pending override |
| `f` | `REQUEST_FAULT_CLEAR` for a selected Lx or RLx |
| `d` | Force a simulated hour (0-23) for demo peak/off-peak switching |
| `a` | Resume automatic mode selection from the real clock |

**Intersection (`lx_main`) sensor keys:**

| Key | Simulated input |
|---|---|
| `a` / `A` | Assert / clear arterial vehicle presence |
| `c` / `C` | Assert / clear connector vehicle presence |
| `1`/`2`/`3`/`4` | Press the pedestrian button for side 0/1/2/3 |
| `w` / `W` | Assert / clear the advance queue-warning |

**Railway (`rlx_main`) sensor and demo keys:**

| Key | Simulated input |
|---|---|
| `0` / `1` | Report a train approaching from direction 0 / 1 |
| `x` | Arm the next gate motion to fail confirmation (demo fault injection) |
| `r` | Force the simulated gate feedback to confirmed OPEN (fault-clear demo aid) |
| `f` | Demo-only local fault-clear (bypasses Central; normal operation uses Central's `f`) |

**Common to all controllers:** `h` / `?` prints the current terminal's key
map; `q` stops only that controller's operator/sensor-input thread (its IPC
server, timers, watchdog, and state machine keep running).

## 8. Now that it runs — verify it thoroughly / demo it

- **Manual test plan**: `docs/test-plan/` — documented test cases across 7
  categories (use cases, state machines, protocol contract, timing, faults,
  races, cross-node). Start with `docs/test-plan/01-usecase-functional.md`.
- **Automated version of most of it**: `tools/test-automation/` — run
  `python3 tools/test-automation/runner.py --binary-dir build/bin --cases
  tools/test-automation/cases/` from a machine that can reach the built
  binaries — see that tool's own `README.md`.
- **Live visual dashboard** (optional, for presenting): `tools/dashboard/` —
  reads `c_main`'s existing status output, no code changes — see its
  `README.md`.
- **Implementation Note**: `TeamQNX_submission/TeamQNX_ImplementationNote.pdf`
  — the authoritative record of the architecture, IPC design, key decisions,
  and full verification/test results for the submitted build.

## Troubleshooting quick list

| Symptom | Likely cause | Fix |
|---|---|---|
| Compiler/makefile errors mentioning odd paths | Space in workspace path | Move the repo to a path with no spaces (§1) |
| Binaries never show up after "successful" build | Wrong output folder for your build method | IDE → `build/x86_64-debug/`; `make` → `build/bin/` |
| A binary runs alone but never appears on Central's table | Qnet not enabled on that VM, or wrong `TRAFFIC_NODE_MAP` | Re-check §4; re-check `TRAFFIC_NODE_MAP` export (§6.2) |
| Everything runs on one VM but not across VMs | Missing/wrong `TRAFFIC_NODE_MAP` | §6.2 |
| `could not open central_log.txt` | Running from a read-only directory (e.g. `/`) | `cd /tmp` first (§4) |
| Not sure the binary is even the latest code | Stale build artifact | `make clean && make` (or rebuild the IDE project) before redeploying |
