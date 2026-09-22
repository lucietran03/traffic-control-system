# Source-Code Organisation

Repo-relative paths, matching the current submitted release (branch `documentation`, based on `main`). Authoritative source is `app/` — see the note at the end of Section 1 on `traffic_light/`.

---

## 1. Repo Structure

The workspace is one Git repository containing **three independent QNX projects** (each compiles to one standalone executable) plus a shared library all three depend on:

```
traffic-control-system/                    (workspace root)
├── app/                                    source workspace (builds via root Makefile / QNX Momentics)
│   ├── central/                            PROJECT 1 — Central Controller  ->  build/bin/c_main
│   │   ├── includes/                       public headers (.h)
│   │   └── src/                            implementation (.c)
│   ├── intersection/                       PROJECT 2 — Intersection Controller  ->  build/bin/lx_main
│   │   ├── includes/
│   │   └── src/
│   ├── railway/                            PROJECT 3 — Railway Crossing Controller  ->  build/bin/rlx_main
│   │   ├── includes/
│   │   └── src/
│   └── shared/                             common IPC contract — compiled into ALL THREE binaries
│       ├── includes/                       sys_types.h, ipc_msg.h, qnet_utils.h
│       └── src/                            qnet_utils.c
│
├── tools/                                  supporting tooling — NOT compiled into any QNX binary
│   ├── dashboard/                          read-only live web dashboard (Python, tails a running c_main's stdout)
│   └── test-automation/                    scripted-keystroke test harness (Python, drives build/bin/* as subprocesses)
│
├── docs/                                   build/deployment guides + function-level flow-diagram documentation
│
├── Makefile                                builds all 3 projects (`make`) or a host-only syntax check (`make check-syntax`)
│
└── *.md at repo root                       requirements/design documentation (not source code):
                                             usecase.md, SEQUENCE_DIAGRAMS.md, STATE_CHARTS.md,
                                             SYSTEM_DIAGRAMS.md, system_assumptions_tables.md,
                                             PROJECT_SPECIFICATION.vi.md
```

**Project → executable mapping** (each is a *generic* binary; which physical
controller it represents is chosen at launch, not at compile time):

| Project | Directory | Binary | Identity selection |
|---|---|---|---|
| 1 — Central Controller | `app/central/` | `build/bin/c_main` | none — always `C1`, exactly one instance |
| 2 — Intersection Controller | `app/intersection/` | `build/bin/lx_main` | `argv[1]` ∈ `{1..6}` → `L1..L6`, 6 instances |
| 3 — Railway Crossing Controller | `app/railway/` | `build/bin/rlx_main` | `argv[1]` ∈ `{1..3}` → `RL1..RL3`, 3 instances |

`app/shared/` is not a fourth project — it produces no executable of its own; `sys_types.h`/`ipc_msg.h`/`qnet_utils.h`/`.c` are compiled directly into all three binaries above so the wire format can never drift between them.

**Note on `traffic_light/`:** the repo also contains `traffic_light/{central,intersection,railway}_controller/`, an earlier QNX Momentics IDE project export merged in from a teammate's branch. It is **not** part of the submitted build — the Makefile and this document both target `app/` only. It is kept only for historical reference and is not further described here.

---

## 2. Directory, File and Module Responsibilities

One table per project, columns: **Path** (repo-relative) · **Purpose** · **Main Content / Public Interface** · **Used By**.

### 2.1 `app/central/` — Project 1: Central Controller

| Path | Purpose | Main Content / Public Interface | Used By |
|---|---|---|---|
| `app/central/src/c_main.c` | Process entry point; wires the server/client/operator threads; owns `central_context_t`, `mode_eng_lock`, `console_io_lock` | `main()`; static `on_request()`, `on_pulse()`, `log_reconnect_if_needed()` — no public header, nothing else calls into it | Build target `build/bin/c_main`; not included elsewhere |
| `app/central/includes/c_operator.h` / `src/c_operator.c` | Operator keyboard console — the entry point for every operator-originated command | `c_operator_reader_thread(void *arg)` (thread entry), `c_operator_args_t` | `c_main.c` (spawns the thread) |
| `app/central/includes/c_comm.h` / `src/c_comm.c` | Builds and posts every outgoing C1→Lx/RLx command | `c_comm_set_console_io_lock()`, `c_comm_send_set_mode()`, `c_comm_broadcast_set_mode()`, `c_comm_send_timing_profile()`, `c_comm_broadcast_timing_profile()`, `c_comm_send_request_override()`, `c_comm_send_renew_override()`, `c_comm_send_cancel_override()`, `c_comm_send_request_fault_clear()` | `c_operator.c` (all command handlers), `c_main.c` (auto peak-hour broadcast, lock wiring) |
| `app/central/includes/c_mode_eng.h` / `src/c_mode_eng.c` | Pure decision layer: mode/timing/override validation, peak-hour schedule + auto-check, arterial offset tables | `c_mode_eng_init()`, `c_mode_eng_controller_index()`, `c_mode_eng_select_mode()`, `c_mode_eng_auto_check()`, `c_mode_eng_mark_all_lx_commanded()`, `c_mode_eng_build_timing_profile()`, `c_mode_eng_next_profile_id()`, `c_mode_eng_get_chain()`, `c_mode_eng_validate_override_request()`; owns `c_mode_eng_t` | `c_main.c`, `c_operator.c`, `c_comm.c`, `c_server.c`, `c_watchdog_mon.c`, `c_hmi.c` (all read/write `c_mode_eng_t`) |
| `app/central/includes/c_server.h` / `src/c_server.c` | Records inbound STATUS/HEARTBEAT/CROSSING_STATUS into `c_mode_eng_t`; detects the PA-08 reconnect edge | `c_server_record_status()`, `c_server_record_fault_report()`, `c_server_record_crossing_status()` (first/third return the reconnect-edge flag) | `c_main.c` (`on_request()`) |
| `app/central/includes/c_watchdog_mon.h` / `src/c_watchdog_mon.c` | PA-07: marks a controller UNAVAILABLE after 3 missed heartbeats | `c_watchdog_mon_tick()` | `c_main.c` (`on_pulse()`, 1 Hz) |
| `app/central/includes/c_hmi.h` / `src/c_hmi.c` | UC-09: 1 Hz status-table print | `c_hmi_render()` | `c_main.c` (`on_pulse()`, under `console_io_lock`) |
| `app/central/includes/c_logger.h` / `src/c_logger.c` | Timestamped event log (stdout + `central_log.txt`) | `c_logger_init()`, `c_logger_log()` | `c_main.c`, `c_operator.c`, `c_comm.c` |

### 2.2 `app/intersection/` — Project 2: Intersection Controller

| Path | Purpose | Main Content / Public Interface | Used By |
|---|---|---|---|
| `app/intersection/src/lx_main.c` | Process entry point; parses `argv[1]` identity; wires server/client/sensor/watchdog threads | `main()`, static `parse_self_id()`, `on_request()`, `on_pulse()` | Build target `build/bin/lx_main`; not included elsewhere |
| `app/intersection/includes/lx_fsm.h` / `src/lx_fsm.c` | Largest module — phase/pedestrian/supervisory-authority FSM, plus PA-07 link tracking and DP-02 local-clock mode fallback | `lx_fsm_init()`, `lx_fsm_on_set_timing_profile()`, `lx_fsm_on_set_mode()`, `lx_fsm_on_request_override()`, `lx_fsm_on_renew_override()`, `lx_fsm_on_cancel_override()`, `lx_fsm_on_crossing_status()`, `lx_fsm_set_arterial_vehicle_demand()`, `lx_fsm_set_connector_vehicle_demand()`, `lx_fsm_latch_pedestrian_request()`, `lx_fsm_set_queue_warning()`, `lx_fsm_report_watchdog_trip()`, `lx_fsm_on_phase_timer()`, `lx_fsm_fill_status()`, `lx_fsm_on_heartbeat_result()`, `lx_fsm_local_clock_mode_check()`, `lx_fsm_on_request_fault_clear()`; owns `lx_fsm_t` | `lx_main.c`, `lx_comm.c`, `lx_sensor.c`, `lx_watchdog.c` |
| `app/intersection/includes/lx_timer.h` / `src/lx_timer.c` | Pure timing-policy functions — no state, no I/O | `lx_timer_peak_green_duration_ms()`, `lx_timer_should_exit_green()` | `lx_fsm.c` only |
| `app/intersection/includes/lx_comm.h` / `src/lx_comm.c` | Heartbeat send + reply inspection (PA-07 miss/ACK accounting) | `lx_comm_send_heartbeat()` | `lx_main.c` (`on_pulse()`, 1 Hz) |
| `app/intersection/includes/lx_sensor.h` / `src/lx_sensor.c` | Keyboard-simulated vehicle/pedestrian sensor input | `lx_sensor_reader_thread(void *arg)` (thread entry) | `lx_main.c` (spawns the thread) |
| `app/intersection/includes/lx_signal.h` / `src/lx_signal.c` | Printed signal-head / pedestrian-signal output (stand-in for GPIO/relay drive) | `lx_signal_show_phase()`, `lx_signal_apply_fault_safe()`, `lx_signal_show_override_clearance()`, `lx_signal_show_walk()`, `lx_signal_show_flashing_dont_walk()`, `lx_signal_show_dont_walk()` | `lx_fsm.c` |
| `app/intersection/includes/lx_watchdog.h` / `src/lx_watchdog.c` | PA-10: local hang detection (own thread, no network) | `lx_watchdog_thread(void *arg)` (thread entry) | `lx_main.c` (spawns the thread) |

### 2.3 `app/railway/` — Project 3: Railway Crossing Controller

| Path | Purpose | Main Content / Public Interface | Used By |
|---|---|---|---|
| `app/railway/src/rlx_main.c` | Process entry point; parses `argv[1]` identity; wires server/client/sensor/watchdog threads | `main()`, static `parse_self_id()`, `on_request()`, `on_pulse()` | Build target `build/bin/rlx_main`; not included elsewhere |
| `app/railway/includes/rlx_fsm.h` / `src/rlx_fsm.c` | Crossing lifecycle FSM (OPEN→WARNING→CLOSING→CLOSED→TRAIN_PRESENT→OPENING/RECLOSING→FAULT), occupancy-window tracking, plus PA-07 link tracking | `rlx_fsm_init()`, `rlx_fsm_simulate_train_approaching()`, `rlx_fsm_on_fault_clear()`, `rlx_fsm_on_tick()`, `rlx_fsm_get_crossing_state()`, `rlx_fsm_take_fault_report_pending()`, `rlx_fsm_fill_status()`, `rlx_fsm_on_heartbeat_result()`, `rlx_fsm_report_watchdog_trip()`; owns `rlx_fsm_t` | `rlx_main.c`, `rlx_comm.c`, `rlx_sensor.c`, `rlx_watchdog.c` |
| `app/railway/includes/rlx_gate.h` / `src/rlx_gate.c` | Simulated gate motion/confirmation — RC-06's safety invariant lives here; own internal lock | `rlx_gate_init()`, `rlx_gate_on_tick()`, `rlx_gate_command_close()`, `rlx_gate_command_open()`, `rlx_gate_poll_closed()`, `rlx_gate_poll_open()`, `rlx_gate_arm_demo_fault()`, `rlx_gate_force_confirmed_open()` | `rlx_fsm.c` (all state transitions), `rlx_sensor.c` (demo fault-injection keys) |
| `app/railway/includes/rlx_signal.h` / `src/rlx_signal.c` | Flasher / train-signal printed output | `rlx_signal_show_flashers_on()`, `rlx_signal_show_flashers_off()`, `rlx_signal_show_train_proceed()`, `rlx_signal_show_train_stop()`, `rlx_signal_show_reclosing()`, `rlx_signal_show_fault()` | `rlx_fsm.c` |
| `app/railway/includes/rlx_sensor.h` / `src/rlx_sensor.c` | Keyboard-simulated train-approach / fault-injection input | `rlx_sensor_reader_thread(void *arg)` (thread entry) | `rlx_main.c` (spawns the thread) |
| `app/railway/includes/rlx_comm.h` / `src/rlx_comm.c` | Heartbeat / fault-report / crossing-status sends (3-recipient fan-out for crossing status) | `rlx_comm_send_heartbeat()`, `rlx_comm_send_fault_report()`, `rlx_comm_broadcast_crossing_status_if_changed()` | `rlx_main.c` (`on_pulse()`) |
| `app/railway/includes/rlx_timer.h` / `src/rlx_timer.c` | Pure tick-window countdown helper — no state of its own | `rlx_timer_tick_window()` | `rlx_fsm.c` |
| `app/railway/includes/rlx_watchdog.h` / `src/rlx_watchdog.c` | PA-10: local hang detection (own thread, no network) | `rlx_watchdog_thread(void *arg)` (thread entry) | `rlx_main.c` (spawns the thread) |

### 2.4 `app/shared/` — common IPC contract (compiled into all 3 projects)

| Path | Purpose | Main Content / Public Interface | Used By |
|---|---|---|---|
| `app/shared/includes/sys_types.h` | Shared enums/bitmasks with no behaviour — controller IDs, operating mode, phase/crossing/supervisory state, connectivity state, fault flags, NACK reasons | Types only, no functions: `controller_id_t`, `operating_mode_t`, `signal_phase_t`, `crossing_state_t`, `supervisory_state_t`, `connectivity_state_t`, `fault_flags_t`, `sensor_status_t`, `nack_reason_t` | Every `.c`/`.h` in all 3 projects |
| `app/shared/includes/ipc_msg.h` | Wire envelope + one payload struct per cross-node verb | Types only: `msg_type_t`, `msg_result_t`, `ipc_request_t`, `ipc_reply_t`, and one payload struct per verb (`status_report_payload_t`, `heartbeat_payload_t`, `crossing_status_payload_t`, `set_mode_payload_t`, `set_timing_profile_payload_t`, `request_override_payload_t`, `renew_override_payload_t`, `fault_report_payload_t`) | `c_comm.h`, `c_mode_eng.h`, `c_server.h`, `c_main.c`, `lx_fsm.h`/`.c`, `lx_main.c`, `rlx_fsm.h`, `rlx_main.c`, `rlx_sensor.c`, `qnet_utils.h` |
| `app/shared/includes/qnet_utils.h` / `src/qnet_utils.c` | Qnet attach/open, the 2-thread (server/client) IPC pattern, timer/pulse arming, `TRAFFIC_NODE_MAP` cross-node name resolution | `ipc_attach()`, `ipc_attach_name()`, `ipc_timer_arm()`, `ipc_server_run()`, `ipc_client_queue_create()`, `ipc_client_queue_destroy()`, `ipc_client_post()`, `ipc_client_thread_main()` (thread entry) | `c_main.c`, `lx_main.c`, `rlx_main.c`, and every `*_comm.h`/`c_operator.h` (via `ipc_client_post()`) |

### 2.5 `tools/` — supporting tooling (not compiled into any QNX binary)

| Path | Purpose | Main Content / Public Interface | Used By |
|---|---|---|---|
| `tools/dashboard/server.py` | Tails a running `c_main`'s redirected stdout, serves the parsed state as JSON | `tail_log()`, `parse_line()` (`ROW_RE` regex), `Handler.do_GET()`, `main()` | Run standalone (`python3 tools/dashboard/server.py --log ...`); no QNX process ever imports it |
| `tools/dashboard/static/` (`index.html`, `app.js`, `style.css`) | Browser-side rendering of `/state.json` — network map, raw table, status-code legend | `poll()`, `render()`, `updateOverview()`, `renderRawTable()`, `buildCodeLegend()` | Served by `server.py`; loaded by any browser pointed at the dashboard |
| `tools/test-automation/runner.py` | Scripted-keystroke test harness — drives `build/bin/*` as ordinary subprocesses | `NodeProcess`, `run_case()`, `run_case_repeated()`, `wait_for_pattern()`, `main()` | Run standalone against a real build; reads case files under `tools/test-automation/cases/` |
| `tools/test-automation/mock_node.py` | Lightweight non-QNX stand-in for `lx_main`/`rlx_main`, used only to validate `runner.py`'s own mechanics | Minimal stdin/stdout loop mirroring the real binaries' keypress conventions | `runner.py` (as a substitute target when no real QNX build is available) |

---

*Source: derived directly from the current `app/` tree and each header's declared interface (verified via the files themselves, not copied from an earlier draft) — regenerate this table if a module's public functions change.*
