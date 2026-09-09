# Getting started on QNX — end to end

One walkthrough from "I have this git repo, nothing else" to "10 controllers
are actually running and I can see them talking to each other." Existing
docs already cover build (`QNX_MOMENTICS_INTEGRATION.md`) and deploy/run
(`QNX_DEPLOYMENT_RUN_GUIDE.md`) in depth — this file is the missing bridge
between them, plus the one step neither covers: getting the code onto the
machine in the first place. Follow it top to bottom the first time; after
that, jump straight to whichever doc has the detail you need.

## 0. Prerequisites checklist

- A machine with **QNX Momentics IDE 8.0.3** (QNX SDP 7.1) installed —
  this is where you'll edit/build, not necessarily where the traffic
  controllers run.
- **VirtualBox** installed, for hosting the QNX target VM(s).
- Your team's `Guide to enable network services and SSH connection to QNX
  VM targets.pdf` (Canvas, Lab 2) — needed once, in step 4.
- Decide your topology now (affects how many VMs you create in step 3):
  **1 VM** (everything on one target, simplest, good for a first smoke
  test), **3 VMs on 1 PC**, **3 VMs across 2-3 PCs** — see
  `QNX_DEPLOYMENT_RUN_GUIDE.md` §3 for the exact three cases and their
  `TRAFFIC_NODE_MAP` values. This guide's steps assume the simplest case
  (1 VM, no cross-node networking) to get you to "it runs" fastest; once
  that works, layer on `TRAFFIC_NODE_MAP` for a real multi-VM topology.

## 1. Get the code onto your Momentics machine

This repo lives on GitHub; Momentics needs it on local disk.

```bash
git clone git@github.com-lucietran03:lucietran03/traffic-control-system.git
```

(or `git pull` if you already have a clone — just make sure you're on
`main` and up to date: `git status`, `git pull origin main`).

**Workspace path warning** (from `QNX_MOMENTICS_INTEGRATION.md`): make
sure the path has **no spaces** anywhere in it — e.g.
`C:\Users\you\qnx-workspace\traffic-control-system`, not `.../My
Documents/...`. A space in the path causes unintelligible compiler/
makefile errors later.

## 2. Build

Two options, same output (`c_main`, `lx_main`, `rlx_main`) — pick one:

**Option A — QNX Momentics IDE** (full detail in
`QNX_MOMENTICS_INTEGRATION.md` §1-4): launch IDE → create 3 `QNX
Executable` projects (`Central_Controller`/`Intersection_Controller`/
`Railway_Controller`, language `C`, CPU `x86_64`) → import each project's
`.c`/`.h` files from `app/<node>/src|includes` + `app/shared/src/
qnet_utils.c` + `app/shared/includes/*.h` → add `app/shared/includes` to
each project's include path (`Properties → C/C++ General → Paths and
Symbols`) → `Build Project`. Binaries land in `build/x86_64-debug/`.

**Option B — command line, from a QNX SDP shell/terminal** (faster, no
IDE project setup):

```bash
make            # builds build/bin/{c_main,lx_main,rlx_main}
```

If `qcc` isn't on `PATH` yet, source your SDP environment script first
(e.g. `qnxsdp-env.sh`), or launch a QNX SDP terminal/IDE terminal that
already has it. `make check-syntax` is a **separate, non-QNX** sanity
check (plain host gcc, no QNX headers) — useful on a machine with no SDP
at all, but it does **not** prove the real build works; use `make`/the
IDE for that.

**Confirm it actually built**: you should have three real files now
(`build/bin/c_main`/`lx_main`/`rlx_main`, or the IDE's `build/x86_64-
debug/` equivalents). If a build fails, check the `Problems` tab (IDE) or
the terminal output (`make`) — don't skip ahead with missing binaries.

## 3. Create your target VM(s)

Full detail: `QNX_MOMENTICS_INTEGRATION.md` §3. Quick version for the
simplest 1-VM topology:

1. In Momentics, launch-target dropdown → **New Launch Target** → **QNX
   Virtual Machine Target**.
2. Name it (e.g. `VM_x86_Target01`), platform `vbox`, CPU `x86_64`.
3. Finish — the IDE creates and boots the VM.

(For a 3-VM topology, repeat 3 times — see `QNX_DEPLOYMENT_RUN_GUIDE.md`
§3 for which controller goes on which VM under each case.)

## 4. Enable Qnet networking on the VM

**This step is easy to forget and the #1 reason "it builds but nothing
talks to anything."** Qnet is **not** on by default on a fresh QNX VM
target. Follow your team's `Guide to enable network services and SSH
connection to QNX VM targets.pdf` (Canvas, Lab 2) to configure the
network adapter and startup scripts. Confirm it worked before moving on:
SSH into the VM and run `ls /net` — you should see at least the VM's own
node name.

## 5. Transfer the binaries to the target

Full detail: `QNX_DEPLOYMENT_RUN_GUIDE.md` §2.1. Quickest path: `Window →
Show View → QNX Target File System Navigator` in Momentics, drag the
three binaries from your build output folder into the VM's `/tmp/`.

(If you built with `make` on a machine that already has direct
shell/SSH/SCP access to the VM, a plain `scp build/bin/* qnxuser@<vm>:/tmp/`
works too — same destination.)

## 6. Run it — simplest possible smoke test first

Before attempting a full 10-node topology, confirm ONE binary runs at
all. SSH into your VM:

```sh
/tmp/lx_main 1
```

You should immediately see `Lx 1: SIGNAL -> ARTERIAL GREEN` and, every
few seconds, phase transitions printing on their own (48s/4s/2s/30s/4s/2s
cycle — see `app/README.md`). Press `h` for the sensor key map, `q` to
stop. **If you see this, your build + deploy pipeline works end to end.**
Ctrl+C or `q` to stop before continuing.

## 7. Run the real thing — all 10 controllers

Still simplest on **one VM** first (no `TRAFFIC_NODE_MAP` needed — same-
node Qnet resolution just works): open enough SSH sessions/terminals on
that VM to run each binary in its own window (or background them with
`&`), in this order (`QNX_DEPLOYMENT_RUN_GUIDE.md` §2.3):

```sh
/tmp/c_main &
/tmp/rlx_main 1 &
/tmp/rlx_main 2 &
/tmp/rlx_main 3 &
/tmp/lx_main 1 &
/tmp/lx_main 2 &
/tmp/lx_main 3 &
/tmp/lx_main 4 &
/tmp/lx_main 5 &
/tmp/lx_main 6 &
```

(Order isn't actually load-bearing — `ipc_client_post()` fails gracefully
and retries are per-message, not a hard dependency — but starting Central
first means its status table has something to show from the start.)

**Confirm it's working**: `c_main`'s console prints a `---- C1 network
status ----` table once a second. Within ~3 seconds you should see all 9
rows (`L1`-`L6`, `RL1`-`RL3`) go to `AVAILABLE`. Press keys on any
`lx_main`/`rlx_main` window (`h` for its key map) and watch the effect
propagate — e.g. `0` on an `rlx_main` (train approaching) should make the
adjacent `lx_main`s stop cycling toward the crossing within ~1 second.

### Scaling to real multiple VMs

Once the single-VM version works, moving controllers to separate VMs
needs exactly one more thing: export `TRAFFIC_NODE_MAP` in each shell
**before** launching a binary that needs to reach a peer on another VM —
see `QNX_DEPLOYMENT_RUN_GUIDE.md` §2.2 for the exact format and §3 for
ready-made values per topology case. Forgetting this is the #2 most
common failure — the symptom is a controller that runs fine standalone
but never shows up as `AVAILABLE` on Central's table, because
`name_open()` silently treated the remote peer as "same node" and failed.

## 8. Now that it runs — verify it thoroughly / demo it

- **Manual test plan**: `docs/test-plan/` — 225 documented test cases
  across 7 categories (use cases, state machines, protocol contract,
  timing, faults, races, cross-node). Start with
  `docs/test-plan/01-usecase-functional.md`.
- **Automated version of most of it**: `tools/test-automation/` — run
  `python3 tools/test-automation/runner.py --binary-dir build/bin --cases
  tools/test-automation/cases/` from a machine that can reach the built
  binaries (182 of 227 cases are scripted; the rest are marked `skip`
  with a reason — see that tool's own `README.md`).
- **Live visual dashboard** (optional, for presenting): `tools/dashboard/`
  — reads `c_main`'s existing status output, no code changes, see its
  `README.md`. (As of this writing it hasn't been connected to a real run
  yet — do that after step 7 works.)

## Troubleshooting quick list

| Symptom | Likely cause | Fix |
|---|---|---|
| Compiler/makefile errors mentioning odd paths | Space in workspace path | Move the repo to a path with no spaces (§1) |
| Binaries never show up after "successful" build | Wrong output folder for your build method | IDE → `build/x86_64-debug/`; `make` → `build/bin/` |
| A binary runs alone but never appears on Central's table | Qnet not enabled on that VM, or wrong `TRAFFIC_NODE_MAP` | Re-check step 4; re-check `TRAFFIC_NODE_MAP` export (step 7) |
| Everything runs on one VM but not across VMs | Missing/wrong `TRAFFIC_NODE_MAP` | `QNX_DEPLOYMENT_RUN_GUIDE.md` §2.2/§3 |
| Not sure the binary is even the latest code | Stale build artifact | `make clean && make` (or rebuild the IDE project) before redeploying |
