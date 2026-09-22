# Test Plan — Traffic Control System

Manual test suite for the entire distributed traffic control system
(EEET2588). Split into 7 files by test type so multiple members can run
them in parallel; each file is self-contained (keymap, timing constants,
environment conventions) so you don't need to re-read the whole source
before testing.

## File list

| # | File | Scope | Test type | Tooling / method | # Cases |
|---|---|---|---|---|---|
| 1 | [01-usecase-functional.md](01-usecase-functional.md) | UC-01..10 (usecase.md) — end-to-end behavior per use case | Functional / black-box | Keyboard (`lx_sensor.c`/`rlx_sensor.c`/`c_operator.c`) + observe console/log | 43 |
| 2 | [02-state-machine-transition.md](02-state-machine-transition.md) | SC-01A/B/C, SC-02, SC-03A/B, SC-04A/B, SC-05 (STATE_CHARTS.md) — per transition | State-machine transition | Keyboard + observe state via console/`c_hmi`; some cases require waiting for exact timing milestones | 42 |
| 3 | [03-protocol-contract.md](03-protocol-contract.md) | 10 verbs in `ipc_msg.h` — every ACK/ACK_PENDING/NACK(reason)/ERROR outcome | Protocol / contract | Mostly via `c_operator.c` (keyboard); some malformed-payload cases need a **separate `test_client` tool (not yet built)** | 47 |
| 4 | [04-timing-assumptions.md](04-timing-assumptions.md) | Numeric/timing values in `system_assumptions_tables.md` (TC/TL/RC/PA/DP/CC) | Timing / performance | Manual stopwatch or reading timestamps in `central_log.txt` (no shared clock; system uses a 100ms tick) | 32 |
| 5 | [05-fault-safety.md](05-fault-safety.md) | Watchdog, FAULT_SAFE, RC-06/09/10 fault gating | Fault-injection / safety | Keyboard (demo keys `x`/`r`/`f`); some cases require a **debugger (gdb)** to simulate a hung thread or call functions directly with no UI path | 23 |
| 6 | [06-concurrency-race.md](06-concurrency-race.md) | Race conditions, input bursts, regression for fixed bugs | Concurrency / race | Very rapid keyboard input (script it if possible), **repeat multiple times (N≥5-10)** to increase odds of catching the bug | 21 |
| 7 | [07-cross-node-integration.md](07-cross-node-integration.md) | Cross-node behavior over real Qnet (multi-VM) | Integration / distributed | Multiple real QNX VMs over SSH + `TRAFFIC_NODE_MAP` env var, observing logs concurrently on multiple machines | 19 |
| | **Total** | | | | **227** |

## Environment conventions (shared across all files)

- **(A) Single QNX node** — runs only one binary (e.g. `lx_main 1`), no other node needed.
- **(B) Multiple nodes on the SAME QNX machine** — runs several binaries at once, Qnet same-node, no `TRAFFIC_NODE_MAP` needed.
- **(C) Multiple nodes on MULTIPLE real QNX machines/VMs over the network** — requires setting `TRAFFIC_NODE_MAP` (see `app/shared/README.md` section "Cross-node resolution" and `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`).
- **(D) Requires a separate `test_client` tool (not yet existing)** — some protocol-level cases (malformed payloads, sending directly to bypass `c_operator.c`'s validation) cannot be tested through the current UI. Proposed: a small executable reusing `qnet_utils.c` that builds an arbitrary `ipc_request_t` and sends it directly.

## Key findings during test design (code not yet fixed, decision needed)

Agents wrote tests independently of each other but repeatedly found the same
issue on their own while trying to reproduce test cases — a reliable signal:

| # | Finding | Severity | Note |
|---|---|---|---|
| 1 | **`RESULT_ACK` for `MSG_REQUEST_FAULT_CLEAR` cannot be reproduced via keyboard** — no path calls `rlx_gate_command_open()` while in state `RLX_FAULT`, so the "fault clear succeeds" branch can only be verified with a debugger | Confirmed **independently by 3 agents** (section 1, 3, 5) | Not a logic bug (validation is still correct), just missing a demo path to test/demonstrate this branch |
| 2 | ~~`lx_fsm_local_fault_clear()` exists but isn't wired to any trigger~~ — **FIXED**: `MSG_REQUEST_FAULT_CLEAR` is now handled by `lx_main.c` via `lx_fsm_on_request_fault_clear()`, and `c_operator.c`'s `f` key prompts for node type (0=Lx/1=RLx), so Lx now has a real `FAULT_SAFE` exit path via Central without restarting the process | Closed | The fix came with a safety re-audit: correctly resume `RAILWAY_PREEMPTION` (not always `NORMAL_OPERATION`) if the adjacent crossing is not yet `OPEN` at the time of clear (`fsm->last_crossing_state`) — see `05-fault-safety.md` TC-FAULT-14b/14c |
| 3 | **DP-02 (automatic Peak/Off-Peak switching by time of day) not wired into runtime** — `c_mode_eng_select_mode()` is never called with real time anywhere; mode is only changed manually via operator | Low | No safety impact, just incomplete automation |
| 4 | **`rlx_comm_broadcast_crossing_status_if_changed()` has no resend/backfill** — if a late-starting Lx misses one send, it won't resync until the next state change | Low | Known design limitation, document as expected behavior in the test case |
| 5 | `NACK_REASON_PEDESTRIAN_ACTIVE` is dead code (defined but never set) | Low | Known from a previous audit; actual behavior is `ACK_PENDING` |

## How to use this document

1. Build per `../../README.md`/root `Makefile`, or via QNX Momentics (`docs/QNX_MOMENTICS_INTEGRATION.md`).
2. Run tests in file order 1 → 7 (increasing environment complexity: 1-2 mostly environment A/B, 4-5 need close timing observation, 6 needs repeated runs, 7 requires environment C).
3. For each FAILed test case, cross-check the file:function cited in the "Related" section before reporting the bug.
