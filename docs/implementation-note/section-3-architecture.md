# 3. Implemented System Architecture

## 3.1 Runtime Architecture

The three Eclipse projects (`central_controller`, `intersection_controller`,
`railway_controller`) compile down to three binaries but ten runtime
processes: one `c_main` (C1), six instances of `lx_main` (L1-L6), and three
instances of `rlx_main` (RL1-RL3) — the same binary is deployed multiple
times with `argv[1]` selecting the logical identity (`lx_main 3` becomes
`L3`, `rlx_main 2` becomes `RL2`), per `parse_self_id()` in
`app/intersection/src/lx_main.c` and `app/railway/src/rlx_main.c`; C1 takes
no `argv` and is a compile-time constant (`app/central/src/c_main.c`). This
matches `docs/flow-diagrams/00-ARCHITECTURE-OVERVIEW.md` Diagram 1 (3
source trees -> 3 binaries -> 10 processes).

`docs/QNX_DEPLOYMENT_RUN_GUIDE.md` documents the as-built deployment matrix
as one QNX VM target per logical controller process — `VM_x86_Target01`
through `VM_x86_Target10`, one each for C1, L1-L6, and RL1-RL3 (Section
2.1's file-transfer table) — under two topology cases: **Case 1**, all 10
VMs hosted on a single workstation with Network Adapter 2 on an internal
`qnet-lab` network; and **Case 2**, the same 10 VMs spread across multiple
physical lab PCs on a bridged LAN (example split: PC A hosts C1, PC B
hosts L1-L6, PC C hosts RL1-RL3). Every process resolves its peers through
the `TRAFFIC_NODE_MAP` environment variable, which maps each logical
controller suffix (`c1`, `l1`..`l6`, `rl1`..`rl3`) to a physical Qnet node
name; because names are registered globally (`NAME_FLAG_ATTACH_GLOBAL` in
`qnet_utils.c`, see `app/shared/README.md`), the same source binaries run
unmodified whether a suffix maps to its own dedicated VM or to a VM shared
with other controllers. `app/shared/README.md` gives a concrete example of
the latter: a 3-VM `TRAFFIC_NODE_MAP` in which one VM runs `c_main`, a
second VM runs all six `lx_main` instances (`L1`-`L6`), and a third runs
all three `rlx_main` instances (`RL1`-`RL3`) — i.e. node count and
physical-machine count are independent, consistent with the lecturer's
clarification in `lecture_clarification.md` that "multiple QNX nodes with
separate process on each node" does not require one physical computer per
controller. (Note: Section 1's Table 2 records the demonstrated
configuration as 9 QNX VMs; the deployment guide's Section 2.1 matrix
enumerates 10 targets, one per controller process across C1 + L1-L6 +
RL1-RL3 — both the guide's 10-target matrix and the shared README's
lower-count example are legitimate `TRAFFIC_NODE_MAP` configurations of
the same binaries, and either can be used to produce Figure 1 below.)

**Local autonomy.** Each Lx/RLx owns and actuates its own outputs directly
from its local FSM (`lx_fsm.c` / `rlx_fsm.c`) and never blocks on a reply
from C1 to do so. Loss of the Central link does not stop local control:
per PA-07, an Lx/RLx that fails to get a `RESULT_ACK` on three consecutive
1 s `MSG_HEARTBEAT` sends transitions `link_state` from
`LINK_CENTRAL_CONNECTED` to `LINK_DEGRADED_LOCAL`
(`lx_fsm_on_heartbeat_result()` / `rlx_fsm_on_heartbeat_result()`), while
its phase/protection FSM keeps running off its own timer pulses regardless.
While degraded, an Lx additionally falls back to a local wall-clock
peak/off-peak schedule (DP-02, `lx_fsm_local_clock_mode_check()`, called
every `IPC_PULSE_HEARTBEAT_TICK` from `lx_main.c`'s `on_pulse()`) so mode
selection keeps tracking time-of-day without C1's broadcast; RLx has no
equivalent fallback because RC-10 already gives its core safety logic full
autonomy independent of Central at all times, degraded or not. On the next
successfully ACKed heartbeat, `link_state` jumps straight back to
`LINK_CENTRAL_CONNECTED` (PA-08) carrying the full `status_report_payload_t`
in that same heartbeat, which is enough for Central to treat the node as
resynchronised without a dedicated resync verb. Central detects the same
outage/recovery independently on its own 1 Hz `c_watchdog_mon_tick()`
(`app/central/src/c_watchdog_mon.c`) and logs the reconnect edge via
`log_reconnect_if_needed()` in `c_main.c` — the two sides share no state and
each side's detection is self-contained (see
`docs/flow-diagrams/UC-10-continue-local-operation-during-link-loss.md`).

**Direct RLx-to-adjacent-Lx safety path.** When a railway crossing's
wire-visible state changes, `rlx_comm_broadcast_crossing_status_if_changed()`
(`app/railway/src/rlx_comm.c`) enqueues exactly three `MSG_CROSSING_STATUS`
sends — to its two adjacent Lx controllers (compile-time adjacency
`RL1->{L1,L2}`, `RL2->{L3,L4}`, `RL3->{L5,L6}`) and to C1 — onto the same
outbound queue, drained in parallel by RLx's own client thread. Central's
copy is for display/logging only; it is not a relay, and the two adjacent
Lx controllers apply `SUPERVISORY_RAILWAY_PREEMPTION` (or resume normal
operation once the crossing reopens) the moment their own copy of the
message arrives via `lx_fsm_on_crossing_status()`
(`app/intersection/src/lx_fsm.c`) — Lx never waits on C1 to learn about or
forward a railway closure. This matches RC-10 ("`RLx` never waits for `C1`
before applying its local safe state") and RC-01/RC-02 ("the owning `RLx`
exclusively controls the gates, flashers, and train signals; other
controllers receive status only" / "no `Lx` may command railway
equipment") in `system_assumptions_tables.md`, and is traced end-to-end in
`docs/flow-diagrams/UC-04-protect-railway-crossing.md`.

A good Figure 1 for this section would be either a screenshot of the QNX
Momentics IDE Project Explorer showing the three imported projects
(`central_controller`, `intersection_controller`, `railway_controller`)
each successfully built, or a terminal/VM layout diagram enumerating which
physical VM (per `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`'s Section 2.1 matrix)
runs which of the ten controller processes. No such screenshot has been
captured in this environment (no QNX SDP toolchain or running VM cluster
is available here) — this placeholder must be filled in from an actual
build/deployment session before submission.

[INSERT CROPPED, LEGIBLE SCREENSHOT HERE]

*Figure 1. As-built task and deployment architecture*

## 3.2 Major Components and Processes

*Table 5. Implemented processes and responsibilities*

| Node / process | Purpose | Key inputs | Key outputs | Source location |
| --- | --- | --- | --- | --- |
| C1 / `c_main` | Central supervision: aggregates status/fault/heartbeat reports, dispatches operator commands, auto-switches peak/off-peak mode by wall clock, renders the 1 Hz status table, persists events | `MSG_STATUS` / `MSG_HEARTBEAT` / `MSG_FAULT_REPORT` / `MSG_CROSSING_STATUS` from L1-L6 and RL1-RL3; operator keyboard commands (`m,t,o,r,c,f,d,a,q`) | `SET_MODE` / `SET_TIMING_PROFILE` / `REQUEST_OVERRIDE` / `RENEW_OVERRIDE` / `CANCEL_OVERRIDE` / `REQUEST_FAULT_CLEAR` to Lx; `REQUEST_FAULT_CLEAR` to RLx; printed status table; `central_log.txt` entries | `app/central/src/c_main.c` (with `c_mode_eng.c`, `c_server.c`, `c_watchdog_mon.c`, `c_hmi.c`, `c_logger.c`, `c_comm.c`) |
| L1-L6 / `lx_main` | Intersection phase sequencing FSM, pedestrian WALK/flashing-don't-walk sequencing, railway pre-emption overlay (`SUPERVISORY_RAILWAY_PREEMPTION`), clear-route override handling, mode/timing-profile application | 100 ms `IPC_PULSE_PHASE_TIMER`; 1 s `IPC_PULSE_HEARTBEAT_TICK`; inbound C1 commands; `MSG_CROSSING_STATUS` from the owning RLx; simulated vehicle/pedestrian/queue-warning keypresses | Printed signal-head/pedestrian-head state; `MSG_HEARTBEAT` (full `status_report_payload_t`) to C1; `RESULT_ACK`/`RESULT_ERROR` replies to inbound commands | `app/intersection/src/lx_main.c` (with `lx_fsm.c`, `lx_timer.c`, `lx_sensor.c`, `lx_signal.c`, `lx_comm.c`) |
| RL1-RL3 / `rlx_main` | Railway crossing protection FSM (warning -> closing -> closed -> train-present -> opening), gate/flasher/train-signal actuation, RC-06 gate-confirmation enforcement, local fault detection and reporting | 1 s `IPC_PULSE_RAILWAY_WARNING` (doubles as FSM tick and RC-04 occupancy countdown); 1 s `IPC_PULSE_HEARTBEAT_TICK`; simulated train-approach/gate-fault/repair keypresses; inbound `MSG_REQUEST_FAULT_CLEAR` from C1 | `MSG_CROSSING_STATUS` to its 2 adjacent Lx and to C1; `MSG_FAULT_REPORT` to C1; `MSG_HEARTBEAT` to C1; printed gate/flasher/train-signal state | `app/railway/src/rlx_main.c` (with `rlx_fsm.c`, `rlx_gate.c`, `rlx_signal.c`, `rlx_timer.c`, `rlx_comm.c`) |
| Lx/RLx watchdog threads (`lx_watchdog_thread`, `rlx_watchdog_thread`) | PA-10 local hang self-detection, entirely node-local (no IPC) | Polled `volatile uint32_t` tick counter (`phase_tick_counter` / `tick_counter`) bumped by the server thread on every timer pulse | Lx: sets `FAULT_WATCHDOG_TRIP` + `SUPERVISORY_FAULT_SAFE` for the next FSM check to notice; RLx: forces `state = RLX_FAULT` and immediately commands the gates closed | `app/intersection/src/lx_watchdog.c`; `app/railway/src/rlx_watchdog.c` |
| Sensor/operator reader threads (`c_operator_reader_thread`, `lx_sensor_reader_thread`, `rlx_sensor_reader_thread`) | Dedicated blocking-stdin threads simulating physical inputs (vehicle sensors, pedestrian buttons, train sensors, operator console) without blocking the server or client IPC threads | Keyboard keystrokes (see Tables 4-6 in Section 2.4) | C1: enqueues outgoing commands via `c_comm.c` senders; Lx/RLx: direct FSM setter calls (`lx_fsm_*`, `rlx_fsm_*`) | `app/central/src/c_operator.c`; `app/intersection/src/lx_sensor.c`; `app/railway/src/rlx_sensor.c` |

## 3.3 Concurrency, Timing, and Synchronisation

Every node follows the same two-mandatory-thread IPC pattern documented in
`app/shared/README.md` ("Threading pattern"): a **server thread** that only
ever calls `MsgReceive()`/`MsgReply()` (`ipc_server_run()`, never blocks
sending), and a **client thread** that only ever calls `MsgSend()`
(`ipc_client_thread_main()`, draining an outbound queue). This split exists
because C1/Lx/RLx have mixed client/server roles with the same peers (e.g.
C1 is a server for inbound `HEARTBEAT` but a client for outbound
`SET_MODE`); collapsing both roles onto one thread would let a blocking
outgoing send stall that same thread's ability to receive, which is a
genuine safety hazard for Lx (it could not suppress a movement toward a
crossing on an incoming `MSG_CROSSING_STATUS` if its one thread were
blocked sending its own heartbeat). On top of these two, each node type
adds its own dedicated threads, confirmed against each `main()`
(`app/central/src/c_main.c`, `app/intersection/src/lx_main.c`,
`app/railway/src/rlx_main.c`, cross-checked against
`docs/flow-diagrams/F-01-node-startup-and-self-identification.md`):

- **C1 — 3 threads**: server, client, operator (`c_operator_reader_thread`).
- **Lx / RLx — 4 threads each**: server, client, sensor
  (`lx_sensor_reader_thread` / `rlx_sensor_reader_thread`), watchdog
  (`lx_watchdog_thread` / `rlx_watchdog_thread`).

**Timers and pulses** (`app/shared/includes/qnet_utils.h`, `_PULSE_CODE_MINAVAIL`-based
so they never collide with kernel-reserved pulse codes): every node arms
its timers with `SIGEV_PULSE` back onto its own channel, dispatched inside
`ipc_server_run()`'s single `MsgReceive()` loop by `switch(msg.hdr.code)`
(`docs/flow-diagrams/F-03-qnet-transport-internals.md`):

| Pulse | Period | Armed on | Effect |
| --- | --- | --- | --- |
| `IPC_PULSE_PHASE_TIMER` | 100 ms | Lx (`lx_main.c:185`) | `lx_fsm_on_phase_timer()` — single-steps phase sequencing, green-extension recheck, override expiry; bumps `phase_tick_counter` |
| `IPC_PULSE_HEARTBEAT_TICK` | 1000 ms | every node (`c_main.c:296`, `lx_main.c:190`, `rlx_main.c:148`) | C1: missed-heartbeat bookkeeping + 1 Hz HMI render; Lx/RLx: sends `MSG_HEARTBEAT`, Lx also runs `lx_fsm_local_clock_mode_check()` |
| `IPC_PULSE_RAILWAY_WARNING` | 1000 ms | RLx (`rlx_main.c:156`) | `rlx_fsm_on_tick()` — reused as the FSM's single recurring tick, driving both the RC-03 warning/closing chain and the RC-04 occupancy countdown; bumps `tick_counter`; triggers pending fault reports and `MSG_CROSSING_STATUS` broadcasts |

(`IPC_PULSE_RAILWAY_OCCUPANCY` is declared in `qnet_utils.h` but never armed
by `rlx_main.c` — the `IPC_PULSE_RAILWAY_WARNING` tick already covers the
occupancy countdown, per that file's `on_pulse()` comment.)

**Mutexes and condition variables**, and why each is needed:

- **`lx_fsm_t.lock` / `rlx_fsm_t.lock`** (`app/intersection/includes/lx_fsm.h`,
  `app/railway/includes/rlx_fsm.h`) — one mutex per FSM instance, taken by
  every `lx_fsm_*()`/`rlx_fsm_*()` function. Needed because each FSM struct
  is touched from at least three threads per node: the sensor thread
  (keypress setters), the server thread (`on_request()`/`on_pulse()`), and
  — for Lx — the client thread's heartbeat-reply callback
  (`on_heartbeat_reply()`). A single struct-wide lock avoids torn reads of
  multi-field FSM state (phase, supervisory overlay, link state, fault
  flags) without needing per-field atomics.
- **`central_context_t.mode_eng_lock`** (`app/central/src/c_main.c`) —
  protects `c_mode_eng_t` (per-controller status/mode records,
  `controllers[]`), which is written by the server thread
  (`c_server_record_*()`, `c_watchdog_mon_tick()`) and by the operator
  thread's `handle_*()` commands. It was added specifically once the
  operator thread was introduced — before that, only the server thread ever
  touched `mode_eng`, so the lock was aspirational; it is now load-bearing.
- **`central_context_t.console_io_lock`** (`app/central/src/c_main.c`) —
  serialises this process's stdout/stdin: `c_hmi_render()`'s 1 Hz table,
  the operator thread's interactive prompts, and `MSG_FAULT_REPORT` log
  lines all write to the same terminal from different threads. Before this
  lock existed the render loop could (and did, in a live demo) splice
  itself into the middle of an operator prompt; this was the concrete bug
  found and fixed this session — commit `a1bacb0` ("fix a console_io_lock
  gap in the peak-hour broadcast") closed a path in `on_pulse()`'s
  `IPC_PULSE_HEARTBEAT_TICK` handler where the auto peak-hour `SET_MODE`
  broadcast logged and sent under `mode_eng_lock` alone, without
  `console_io_lock`, defeating the serialisation the lock exists for.
  `docs/flow-diagrams/F-05-operator-console-dispatch.md` traces the fixed
  design: `console_io_lock` is held across a handler's whole *output*
  sequence but released around the blocking wait for a digit
  (`read_long()`'s inner `scanf()`), so a paused operator cannot starve the
  server thread's 1 Hz render.
- **Internal lock in `app/railway/src/rlx_gate.c`
  (`g_gate_lock`)** — the gate-simulation module is called both from
  `rlx_fsm.c` (already holding `fsm->lock`) and from the sensor/keyboard
  thread's fault-injection commands with no FSM lock held, so it keeps its
  own internal mutex around every public function rather than relying on a
  caller-held lock. It is documented as always the innermost/leaf lock — it
  never calls back into anything that takes `fsm->lock` — so it introduces
  no lock-ordering hazard.
- **Internal mutex + condition variable in `app/shared/src/qnet_utils.c`'s
  `ipc_client_queue_t`** (`q->lock` / `q->not_empty`) — the fixed 16-slot
  outbound ring buffer that decouples any producer thread's
  `ipc_client_post()` (server thread, FSM thread, sensor thread, operator
  thread — all non-blocking, return `-1` immediately if the queue is full
  or stopping) from the client thread's actual `name_open()`/`MsgSend()`
  work. The condition variable lets the client thread block efficiently
  when idle instead of busy-polling, and the lock is only ever held for the
  enqueue/dequeue bookkeeping, never across the blocking Qnet I/O itself.

**Deadlock avoidance.** The one documented multi-lock ordering rule in the
system is Central's: `console_io_lock` is always acquired **before**
`mode_eng_lock`, never the reverse — stated identically in
`c_operator.h`, `c_operator.c`, and `c_main.c`, and enforced everywhere
`c_main.c`'s `on_pulse()` and `c_operator.c`'s reader-thread switch both
touch the two locks. Every other lock in the system (`fsm->lock`,
`g_gate_lock`, the client-queue's `q->lock`) is a single, independent
mutex that is never nested with another lock, so no other AB-BA ordering
exists to violate. Timing-sensitive serialisation on the sensor/watchdog
tick counters (`phase_tick_counter`, `tick_counter`) is deliberately
lock-free: both are `volatile uint32_t` values with a single writer (the
server thread) and a single reader (the watchdog thread), so a plain
unlocked read/increment is sufficient and a mutex would be unnecessary
overhead on a poll-only path (`docs/flow-diagrams/F-04-local-watchdog-self-detection.md`).

*Table 6. Concurrency and synchronisation summary*

| Shared resource / event | Producer(s) | Consumer(s) | Synchronisation mechanism | Rationale |
| --- | --- | --- | --- | --- |
| `lx_fsm_t` / `rlx_fsm_t` struct (phase, supervisory overlay, link state, fault flags, gate state) | Sensor thread (keypress setters), client thread (heartbeat-reply callback, Lx only) | Server thread (`on_request()`/`on_pulse()`), watchdog thread (fault-forcing calls only) | `fsm->lock` (`pthread_mutex_t`), taken by every `lx_fsm_*()`/`rlx_fsm_*()` entry point | One struct, many fields, touched from 3+ threads per node — a single struct-wide lock prevents torn multi-field reads without per-field atomics |
| `central_context_t.mode_eng` (per-controller status/mode records) | Server thread (`c_server_record_*()`, `c_watchdog_mon_tick()`) | Operator thread (`handle_*()` command validation/dispatch), server thread's own `c_hmi_render()` | `mode_eng_lock` (`pthread_mutex_t`), always acquired *inner* to `console_io_lock` | Became load-bearing once the operator thread started reading/writing the same `controllers[]` slots as the server thread; short, uncontended critical sections around each record/tick call |
| C1 terminal stdout/stdin (status table, operator prompts, fault-report log lines) | Server thread (`on_pulse()`'s `c_hmi_render()`, fault-report logging), operator thread (`handle_*()` prompts) | Terminal (human operator) | `console_io_lock` (`pthread_mutex_t`), always acquired *outer* to `mode_eng_lock`; released around `read_long()`'s blocking inner `scanf()` | Prevents the 1 Hz status table from splicing into an in-progress operator prompt (the concrete bug fixed in commit `a1bacb0`); the release-around-blocking-read step stops an operator mid-keystroke from starving the render tick |
| Outgoing IPC job queue (`ipc_client_queue_t`, 16-slot ring buffer) | Any thread calling `ipc_client_post()` — server, FSM/sensor, operator threads | Client thread (`ipc_client_thread_main()`), which dequeues, calls `name_open()`/`MsgSend()`, and invokes the reply callback | Internal mutex + condition variable (`q->lock` / `q->not_empty`) in `qnet_utils.c` | Decouples non-blocking producers from the one thread allowed to block on Qnet I/O; full queue or `stopping` returns `-1` immediately rather than blocking a producer, preserving the "server/FSM threads never block on send" invariant |
