# Test Plan — Traffic Control System

Manual test-plan document set for the entire distributed traffic control
system (EEET2588). Split into 7 files by test type so they can be run in
parallel by multiple team members; each file is self-contained (keymaps,
timing constants, environment conventions) so there is no need to re-read
the whole source before testing.

## File list

| # | File | Scope | Test type | Tools / method | Case count |
|---|---|---|---|---|---|
| 1 | [01-usecase-functional.md](01-usecase-functional.md) | UC-01..10 (usecase.md) — end-to-end behavior per use case | Functional / black-box | Keyboard (`lx_sensor.c`/`rlx_sensor.c`/`c_operator.c`) + observing console/log | 45 |
| 2 | [02-state-machine-transition.md](02-state-machine-transition.md) | SC-01A/B/C, SC-02, SC-03A/B, SC-04A/B, SC-05 (STATE_CHARTS.md) — per transition | State-machine transition | Keyboard + observing state via console/`c_hmi`; some cases need to wait for the right timing mark | 43 |
| 3 | [03-protocol-contract.md](03-protocol-contract.md) | 10 verbs in `ipc_msg.h` — every ACK/ACK_PENDING/NACK(reason)/ERROR outcome | Protocol / contract | Mostly via `c_operator.c` (keyboard); some malformed-payload cases need a **separate `test_client` tool (not yet built)** | 48 |
| 4 | [04-timing-assumptions.md](04-timing-assumptions.md) | Numeric/timing values in `system_assumptions_tables.md` (TC/TL/RC/PA/DP/CC) | Timing / performance | Manual stopwatch or reading timestamps in `central_log.txt` (no shared clock, system uses a 100ms tick) | 32 |
| 5 | [05-fault-safety.md](05-fault-safety.md) | Watchdog, FAULT_SAFE, RC-06/09/10 fault gating | Fault-injection / safety | Keyboard (`x`/`r`/`f` demo keys); some cases need a **debugger (gdb)** to simulate a hung thread or call functions directly with no UI | 24 |
| 6 | [06-concurrency-race.md](06-concurrency-race.md) | Race conditions, rapid-fire input, regression for previously fixed bugs | Concurrency / race | Very fast keyboard operation (script if possible), **repeat many times (N≥5-10)** to increase the chance of catching the bug | 21 |
| 7 | [07-cross-node-integration.md](07-cross-node-integration.md) | Cross-node behavior over real Qnet (multi-VM) | Integration / distributed | Multiple real QNX VMs over SSH + the `TRAFFIC_NODE_MAP` environment variable, observing logs concurrently on multiple machines | 19 |
| | **Total** | | | | **232** |

## Environment conventions (shared across all files)

- **(A) Single QNX node** — only 1 binary running (e.g. `lx_main 1`), no other node needed.
- **(B) Multiple nodes on the SAME QNX machine** — multiple binaries running at once, Qnet same-node, no `TRAFFIC_NODE_MAP` needed.
- **(C) Multiple nodes on MULTIPLE real QNX machines/VMs over the network** — requires setting `TRAFFIC_NODE_MAP` (see `app/shared/README.md`, "Cross-node resolution" section, and `docs/QNX_BUILD_DEPLOY_RUN.md`).
- **(D) Requires a separate `test_client` tool (does not exist yet)** — some protocol-level cases (malformed payload, sending directly while bypassing `c_operator.c`'s validation) cannot be tested through the existing UI. Proposed: a small executable reusing `qnet_utils.c`, building an arbitrary `ipc_request_t` and sending it directly.

## Important findings during test design (code not yet fixed, needs a decision)

Agents wrote tests independently of each other but repeatedly discovered the
same issues on their own while trying to reproduce test cases — a reliable
signal:

| # | Finding | Severity | Notes |
|---|---|---|---|
| 1 | ~~`RESULT_ACK` for `MSG_REQUEST_FAULT_CLEAR` could not be reproduced via keyboard — no path calls `rlx_gate_command_open()` while in state `RLX_FAULT`, so the "fault clear succeeds" branch could only be verified with a debugger~~ — **FIXED**: `rlx_sensor.c`'s `r` key calls `rlx_gate_force_confirmed_open()` (`rlx_gate.c`) to force the gate into confirmed-open state while RLx is still in `RLX_FAULT`, after which `MSG_REQUEST_FAULT_CLEAR` via Central (C1's `f` key) produces a real `RESULT_ACK` — the whole branch is now testable purely via keyboard, no debugger needed | Closed | See the newest test case in `05-fault-safety.md` using the `r` key (`TC-FAULT-22`) |
| 2 | ~~`lx_fsm_local_fault_clear()` exists but is not wired to any trigger~~ — **FIXED**: `MSG_REQUEST_FAULT_CLEAR` is now handled by `lx_main.c` via `lx_fsm_on_request_fault_clear()`, and `c_operator.c`'s `f` key asks for node type (0=Lx/1=RLx), so Lx now has a real `FAULT_SAFE` exit path via Central without restarting the process | Closed | The fix came with a safety re-audit: correctly resumes `RAILWAY_PREEMPTION` (not always `NORMAL_OPERATION`) if the adjacent crossing is not yet `OPEN` at the time of clear (`fsm->last_crossing_state`) — see `05-fault-safety.md` TC-FAULT-14b/14c |
| 3 | ~~DP-02 (automatic Peak/Off-Peak switching by time of day) is not wired into the runtime — `c_mode_eng_select_mode()` is never called with the real clock anywhere, mode is only changed manually via the operator~~ — **FIXED**: `c_main.c`'s `on_pulse()` now calls `c_mode_eng_auto_check()` every 1Hz tick with the real time (or a demo time forced via `c_operator.c`'s `d`/`a` keys) and broadcasts `SET_MODE` to every Lx when a schedule boundary is crossed | Closed | Test with `c_operator.c`'s `d` key (force demo time) / `a` key (cancel forcing) to cross the 06:00/09:00 boundary without waiting for the real clock |
| 4 | **`rlx_comm_broadcast_crossing_status_if_changed()` has no resend/backfill** — an Lx that starts late and misses one send will not resync until the next state change | Low | Known design limitation, document it in the test case as expected behavior |
| 5 | `NACK_REASON_PEDESTRIAN_ACTIVE` is dead code (defined but never set) | Low | Known from a previous audit, actual behavior is `ACK_PENDING` |

## How to use this document set

1. Build per `../../README.md`/root `Makefile`, or via QNX Momentics (`docs/QNX_BUILD_DEPLOY_RUN.md`).
2. Run tests in file order 1 → 7 (increasing environment complexity: 1-2 are mostly environment A/B, 4-5 need close timing observation, 6 needs many repetitions, 7 requires environment C).
3. For each FAILing test case, cross-check the exact file:function cited in the "Related" section before reporting the bug.
