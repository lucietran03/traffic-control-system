# Architecture Overview — System → Layer → Component → Feature

This is the entry point for `docs/flow-diagrams/`. It goes top-down (system
overview → layers inside one node → components inside one layer), then
bottom-out into an index of per-feature function-level flow diagrams (the
files alongside this one). Read this file first; each linked file then
zooms all the way into real function-call chains for one specific feature.

Companion documents this overview does NOT duplicate:
- `SYSTEM_DIAGRAMS.md` — physical network topology (roads, intersections, rail crossings)
- `SEQUENCE_DIAGRAMS.md` — message-level behavior per use case (SD-01..SD-08)
- `STATE_CHARTS.md` — per-controller state machines (SC-01..SC-05)
- `app/shared/README.md` — IPC contract design rationale (threading pattern, Qnet naming, timers)
- `usecase.md` — the 10 formally-specified use cases (UC-01..UC-10)

---

## Level 0 — System overview

Three executables, ten running processes, one shared wire contract:

```text
                              CENTRAL CONTROLLER (C1)
                         one process: build/bin/c_main
                                     |
                         Qnet (name_attach/name_open,
                         MsgSend/MsgReceive/MsgReply)
                                     |
       +-----------------+----------+----------+-----------------+
       |                 |                     |                 |
  INTERSECTIONS (Lx)                                      RAILWAY CROSSINGS (RLx)
  6 processes, one binary:                                3 processes, one binary:
  build/bin/lx_main <1-6>                                 build/bin/rlx_main <1-3>
  identity chosen by argv[1]                              identity chosen by argv[1]
  (L1..L6)                                                 (RL1..RL3)
       |                                                          |
       +---- RC1/RC2/RC3 CROSSING_STATUS -------------------------+
             (RLx -> its 2 adjacent Lx, read-only, never the reverse)
```

- **3 source trees, 3 binaries, 10 processes.** `app/central`, `app/intersection`,
  `app/railway` each compile to ONE generic executable; which physical
  location a process represents is selected by `argv[1]` at launch (`lx_main 3`
  → `L3`), not by a separate source file per intersection (`RC-07`: "one
  generic railway-controller implementation is deployed three times" —
  the same applies to `lx_main`).
- **`app/shared`** is not a fourth executable — it's the header/impl pair
  (`sys_types.h`, `ipc_msg.h`, `qnet_utils.h`/`.c`) all three binaries
  compile against, so the wire format can never drift between them.
- Every arrow above is a Qnet `MsgSend()`/`MsgReceive()`/`MsgReply()` round
  trip. There is no shared memory, no filesystem IPC, no sockets opened
  directly by application code.

---

## Level 1 — Layers inside one node

Every node (`c_main`, `lx_main`, `rlx_main`) is internally organised into
the same five layers, though C1's layer 3 looks different from Lx/RLx's
(C1 has an operator console instead of field sensors/actuators):

| # | Layer | Responsibility | Never does |
|---|---|---|---|
| **1** | **IPC / Transport** | Own the Qnet channel, thread split, wire envelope | Never touches business-logic state directly |
| **2** | **Business logic / FSM** | Pure state + decisions (modes, phases, supervisory authority) | Never calls `MsgSend()`/`ipc_client_post()` itself, never touches stdout |
| **3** | **Actuation & Sensing** | Turn FSM decisions into printed/simulated output; turn keyboard input into FSM setter calls | Never owns state itself — only reads/writes through the FSM's own functions |
| **4** | **Resilience** | Detect local hangs (PA-10) and Central-link loss (PA-07/PA-08) independently of layer 2's normal logic | Never substitutes for FSM logic — only reports faults/link-state into it |
| **5** | **Observability / tooling** | Persist and expose what's happening, outside the safety-critical path | Never influences control decisions (read-only consumers) |

### Layer 1 — IPC / Transport

Threading pattern (see `app/shared/README.md` for the full rationale):
every node splits into a **server thread** (`ipc_server_run()`, only ever
calls `MsgReceive()`/`MsgReply()`, never `MsgSend()`), a **client thread**
(`ipc_client_thread_main()`, drains a queue fed by `ipc_client_post()`,
the only code allowed to call `MsgSend()`), and the node's own
**main/FSM-owning thread** (server thread callbacks run inline, so this is
really the server thread wearing a second hat via `on_request()`/`on_pulse()`).
C1 additionally has a fourth thread: the **operator console** (blocking
`stdin` reads), since that's a second source of outgoing commands distinct
from anything IPC-driven.

Shared implementation: `app/shared/includes/qnet_utils.h` + `app/shared/src/qnet_utils.c`
(`ipc_attach()`, `ipc_server_run()`, `ipc_client_post()`, `ipc_client_queue_create()`,
`ipc_timer_arm()`, the `TRAFFIC_NODE_MAP` cross-node name resolution).
Wire format: `app/shared/includes/ipc_msg.h` (`ipc_request_t`/`ipc_reply_t`,
one payload struct per verb) + `app/shared/includes/sys_types.h` (shared
enums/bitmasks with no behaviour).

### Layer 2 — Business logic / FSM

- **C1**: `app/central/includes/c_mode_eng.h` + `.c` (mode/timing/override
  decision layer — the peak-hour schedule, arterial offset tables, override
  surface validation) plus `app/central/src/c_server.c` (records inbound
  STATUS/HEARTBEAT/CROSSING_STATUS into `c_mode_eng_t`) and
  `app/central/src/c_watchdog_mon.c` (missed-heartbeat staleness detection
  for the other 9 controllers — this is C1 watching *them*, distinct from
  layer 4's "watching itself/its own link to them").
- **Lx**: `app/intersection/includes/lx_fsm.h` + `app/intersection/src/lx_fsm.c`
  — the single largest file in the repo: phase sequencing (`SC-01A/B/C`),
  pedestrian sequencing (`SC-02`), supervisory authority (`SC-03A/B`), and
  now the local-clock DP-02 fallback and PA-07 link tracking too (added
  this session — see `UC-10`'s flow doc).
- **RLx**: `app/railway/includes/rlx_fsm.h` + `app/railway/src/rlx_fsm.c`
  — crossing lifecycle (`SC-04A/B`), occupancy-window tracking, fault
  containment.

### Layer 3 — Actuation & Sensing

- **C1**: `app/central/src/c_operator.c` (keyboard **input** — operator
  commands) and `app/central/src/c_hmi.c` (terminal **output** — the 1 Hz
  status table).
- **Lx**: `app/intersection/src/lx_sensor.c` (keyboard-simulated vehicle/
  pedestrian sensors — **input**) and `app/intersection/src/lx_signal.c`
  (printed signal-head state — **output**).
- **RLx**: `app/railway/src/rlx_sensor.c` (keyboard-simulated train
  approach/faults — **input**), `app/railway/src/rlx_gate.c` (simulated
  gate motion/confirmation — a stateful actuator with its own internal
  lock, RC-06's safety invariant lives here), `app/railway/src/rlx_signal.c`
  (flasher/train-signal printed output).

### Layer 4 — Resilience

- **Local hang detection (PA-10)**: `app/intersection/src/lx_watchdog.c` /
  `app/railway/src/rlx_watchdog.c` — a dedicated thread polling a tick
  counter the server thread bumps; reports into the FSM's fault flags if
  the counter stalls. Entirely local — no network involved.
- **Central-link loss detection (PA-07/PA-08/DP-02, added this session)**:
  `lx_comm.c`'s/`rlx_comm.c`'s heartbeat reply callbacks feed
  `lx_fsm_on_heartbeat_result()`/`rlx_fsm_on_heartbeat_result()`, which
  drive `connectivity_state_t` for real (previously a hardcoded
  placeholder — see `UC-10`'s flow doc for the full trace). `c_server.c`'s
  `was_unavailable` capture is C1's side of the same mechanism.

### Layer 5 — Observability / tooling

- `app/central/src/c_logger.c` — timestamped event log (stdout).
- `tools/dashboard/` — a Python process that tails `c_main`'s stdout and
  serves a live JSON+HTML view; entirely outside the QNX process
  boundary, strictly read-only.
- `tools/test-automation/` — a Python scripted-keystroke test runner that
  drives the real binaries as ordinary subprocesses.

---

## Level 2 — Component table

| File | Layer | One-line responsibility |
|---|---|---|
| `app/shared/includes/sys_types.h` | 1 | Shared enums/bitmasks — controller IDs, modes, states, fault flags |
| `app/shared/includes/ipc_msg.h` | 1 | Wire envelope + one payload struct per cross-node verb |
| `app/shared/includes/qnet_utils.h` / `src/qnet_utils.c` | 1 | Qnet attach/open, server/client thread machinery, timers, `TRAFFIC_NODE_MAP` |
| `app/central/src/c_main.c` | 1/2 glue | C1's `main()`, `on_request()`/`on_pulse()` dispatch, `console_io_lock`/`mode_eng_lock` ownership |
| `app/central/includes/c_mode_eng.h` / `src/c_mode_eng.c` | 2 | Mode/timing/override decision layer, peak-hour schedule + auto-check |
| `app/central/src/c_server.c` | 2 | Records inbound STATUS/HEARTBEAT/CROSSING_STATUS; reconnect-edge detection |
| `app/central/src/c_watchdog_mon.c` | 2 | Marks a controller UNAVAILABLE after 3 missed heartbeats |
| `app/central/src/c_comm.c` | 1/2 glue | Builds and posts every C1→Lx/RLx command; async reply logging |
| `app/central/src/c_operator.c` | 3 | Operator keyboard console — every outgoing command's entry point |
| `app/central/src/c_hmi.c` | 3 | 1 Hz status-table print |
| `app/central/src/c_logger.c` | 5 | Timestamped event log |
| `app/intersection/src/lx_main.c` | 1/2 glue | Lx's `main()`, `on_request()`/`on_pulse()` dispatch |
| `app/intersection/includes/lx_fsm.h` / `src/lx_fsm.c` | 2 | Phase/pedestrian/supervisory FSM; now also link-state + local-clock fallback |
| `app/intersection/includes/lx_timer.h` / `src/lx_timer.c` | 2 | Pure timing-policy functions (no state) |
| `app/intersection/src/lx_comm.c` | 1/2 glue | Heartbeat send + reply inspection (PA-07) |
| `app/intersection/src/lx_sensor.c` | 3 | Keyboard-simulated vehicle/pedestrian input |
| `app/intersection/src/lx_signal.c` | 3 | Printed signal-head output |
| `app/intersection/src/lx_watchdog.c` | 4 | Local hang detection |
| `app/railway/src/rlx_main.c` | 1/2 glue | RLx's `main()`, `on_request()`/`on_pulse()` dispatch |
| `app/railway/includes/rlx_fsm.h` / `src/rlx_fsm.c` | 2 | Crossing lifecycle FSM; now also link-state tracking |
| `app/railway/src/rlx_gate.c` | 3 | Simulated gate motion/confirmation (RC-06 safety invariant) |
| `app/railway/src/rlx_signal.c` | 3 | Flasher/train-signal printed output |
| `app/railway/src/rlx_sensor.c` | 3 | Keyboard-simulated train-approach/fault input |
| `app/railway/src/rlx_comm.c` | 1/2 glue | Heartbeat/fault-report/crossing-status sends |
| `app/railway/src/rlx_watchdog.c` | 4 | Local hang detection |
| `tools/dashboard/server.py` + `static/*` | 5 | Live web dashboard (tails `c_main`'s stdout) |
| `tools/test-automation/runner.py` | 5 | Scripted-keystroke test harness |

---

## Level 3 — Feature index

### Primary use cases (formally specified — `usecase.md` UC-01..UC-10)

| Use case | Flow doc |
|---|---|
| UC-01 — Serve Vehicle Demand | [`UC-01-serve-vehicle-demand.md`](UC-01-serve-vehicle-demand.md) |
| UC-02 — Serve Pedestrian Crossing Request | [`UC-02-serve-pedestrian-crossing-request.md`](UC-02-serve-pedestrian-crossing-request.md) |
| UC-03 — Coordinate Arterial Traffic Progression | [`UC-03-coordinate-arterial-traffic-progression.md`](UC-03-coordinate-arterial-traffic-progression.md) |
| UC-04 — Protect a Railway Crossing | [`UC-04-protect-railway-crossing.md`](UC-04-protect-railway-crossing.md) |
| UC-05 — Manage Traffic During/After Railway Closure | [`UC-05-manage-traffic-during-railway-closure.md`](UC-05-manage-traffic-during-railway-closure.md) |
| UC-06 — Respond to a Railway Equipment Fault | [`UC-06-respond-to-railway-equipment-fault.md`](UC-06-respond-to-railway-equipment-fault.md) |
| UC-07 — Configure Traffic Operating Parameters (SET_MODE) | [`UC-07-configure-mode.md`](UC-07-configure-mode.md) |
| UC-08 — Apply a Bounded Clear-Route Override | [`UC-08-clear-route-override.md`](UC-08-clear-route-override.md) |
| UC-09 — Monitor Network Status and Faults | [`UC-09-monitor-network-status.md`](UC-09-monitor-network-status.md) |
| UC-10 — Continue Local Operation During Central Link Loss | [`UC-10-continue-local-operation-during-link-loss.md`](UC-10-continue-local-operation-during-link-loss.md) |

### Infrastructure / cross-cutting features (not individually named in `usecase.md`, discovered by tracing the codebase)

A dedicated full-codebase sweep (every file in `app/`, `tools/dashboard/`, `tools/test-automation/`, plus the root `Makefile`) found 18 distinct non-UC mechanisms. Two are intentionally NOT given their own file below to avoid duplicating existing documentation: the demo peak-hour clock override (`'d'`/`'a'` operator keys) is already fully traced inside `UC-07`'s flow doc, and the shared IPC wire-format/envelope design rationale (`msg_header_t` mirroring `struct _pulse`, fixed-width wire fields) is already thoroughly covered in `app/shared/README.md` rather than re-explained here. The remaining 12 (with 2 pairs merged where the mechanism is identical across Lx/RLx or across two closely-related tools) each got their own flow doc:

| # | Feature | Flow doc |
|---|---|---|
| F-01 | Node Startup, Self-Identification & Thread Bring-up | [`F-01-node-startup-and-self-identification.md`](F-01-node-startup-and-self-identification.md) |
| F-02 | Cross-Node Qnet Name Resolution (`TRAFFIC_NODE_MAP`) | [`F-02-cross-node-name-resolution.md`](F-02-cross-node-name-resolution.md) |
| F-03 | Qnet Transport Internals: Outgoing Queue + Timer/Pulse Dispatch | [`F-03-qnet-transport-internals.md`](F-03-qnet-transport-internals.md) |
| F-04 | PA-10 Local Watchdog Self-Detection (Lx + RLx) | [`F-04-local-watchdog-self-detection.md`](F-04-local-watchdog-self-detection.md) |
| F-05 | Central Operator Console Dispatch & `console_io_lock` Serialization | [`F-05-operator-console-dispatch.md`](F-05-operator-console-dispatch.md) |
| F-06 | Persistent Event Logging | [`F-06-persistent-event-logging.md`](F-06-persistent-event-logging.md) |
| F-07 | Live Dashboard (Python, outside the QNX process boundary) | [`F-07-live-dashboard.md`](F-07-live-dashboard.md) |
| F-08 | Scripted Test-Automation Tooling (runner + mock-node) | [`F-08-test-automation-tooling.md`](F-08-test-automation-tooling.md) |
| F-09 | Build & Host-Syntax-Check Tooling (`qcc` vs `check-syntax`) | [`F-09-build-and-syntax-check-tooling.md`](F-09-build-and-syntax-check-tooling.md) |
| F-10 | Demo Fault-Injection: Railway Gate Confirmation Failure | [`F-10-railway-gate-fault-injection.md`](F-10-railway-gate-fault-injection.md) |
| F-11 | Green-Wave Phase-Realignment Math (`lx_fsm_apply_offset_locked()`) | [`F-11-green-wave-offset-math.md`](F-11-green-wave-offset-math.md) |
| F-12 | Intersection Local Fault-Safe Supervisory Override (SC-03A) | [`F-12-fault-safe-supervisory-override.md`](F-12-fault-safe-supervisory-override.md) |

### How to use this document set

- Start here (`00-ARCHITECTURE-OVERVIEW.md`) for the system → layer → component shape.
- Read a `UC-xx` file when you need "how does the system behave for this formally-specified use case, from a real keypress/message down to the actuator/reply."
- Read an `F-xx` file when you need "how does this specific piece of infrastructure/tooling/algorithm actually work," independent of any single use case.
- Every file cites real `file:function`/`around line N` references grounded in the current source — if the code has moved on since a doc was written, trust the code and treat the doc as a map that may need a quick re-check, not a second source of truth.
