# app/ — Traffic Control System source tree

QNX Neutrino distributed traffic-control system for the EEET2588 Real-Time
Systems project. Three independent executables, one shared IPC contract.

## Layout

```
app/
├── central/        Supervisory node   -> builds c_main
│   ├── includes/
│   └── src/
├── intersection/    Road-intersection node -> builds lx_main
│   ├── includes/
│   └── src/
├── railway/         Railway-crossing node -> builds rlx_main
│   ├── includes/
│   └── src/
└── shared/          Common IPC contract (sys_types.h, ipc_msg.h, qnet_utils)
    ├── includes/
    └── src/
```

- `c_main`   — one process, the Central controller (`C1`). Monitors all
  other controllers, logs events, and lets an operator issue commands
  (mode changes, timing-profile broadcasts, clear-route overrides, fault
  clears).
- `lx_main`  — one generic executable, run once per intersection with its
  identity selected by `argv[1]` (`1`-`6` → `L1`-`L6`). Owns local
  signal-phase timing, pedestrian requests, and railway pre-emption.
- `rlx_main` — one generic executable, run once per railway crossing with
  its identity selected by `argv[1]` (`1`-`3` → `RL1`-`RL3`). Owns gate
  control and train-approach handling.

Every node talks to every other node it needs to over Qnet, using the
message contract defined in `shared/includes/ipc_msg.h`/`sys_types.h` —
see `shared/README.md` for the full design rationale (threading pattern,
attach-point naming, wire-format rules).

## Building

From the repo root:

```bash
make            # build all three binaries (needs a QNX SDP toolchain, qcc on PATH)
make central    # build build/bin/c_main only
make intersection
make railway
make clean
```

No QNX toolchain on your machine? You can still catch plain C syntax
errors with:

```bash
make check-syntax   # host gcc/clang -fsyntax-only, NOT a real QNX build
```

See the root `Makefile`'s header comment and `docs/QNX_MOMENTICS_INTEGRATION.md`
if you'd rather build via the QNX Momentics IDE instead.

## Running

Each binary needs to `name_attach()` under a unique Qnet name before any
other node can reach it, so start `c_main` first, then the rest in any
order:

```bash
./build/bin/c_main &

./build/bin/rlx_main 1 &     # RL1
./build/bin/rlx_main 2 &     # RL2
./build/bin/rlx_main 3 &     # RL3

./build/bin/lx_main 1 &      # L1
./build/bin/lx_main 2 &      # L2
./build/bin/lx_main 3 &      # L3
./build/bin/lx_main 4 &      # L4
./build/bin/lx_main 5 &      # L5
./build/bin/lx_main 6 &      # L6
```

- `lx_main`/`rlx_main` read simulated sensor input from the keyboard —
  press `h`/`?` after starting one for its key map (vehicle demand,
  pedestrian requests, train approach, etc.).
- `c_main` prints a live status table and accepts operator commands from
  the keyboard — press `h`/`?` for its command list.
- To run the nodes across separate QNX machines/VMs instead of one box,
  see `shared/README.md`'s "Cross-node resolution" section and
  `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` for the `TRAFFIC_NODE_MAP`
  environment variable and full deployment walkthrough.

## More detail

- `shared/README.md` — IPC contract design rationale
- `docs/QNX_PROJECT_FILE_OVERVIEW.md` — file-by-file status
- `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` — multi-VM/multi-PC deployment
- `docs/QNX_MOMENTICS_INTEGRATION.md` — IDE-based build alternative
