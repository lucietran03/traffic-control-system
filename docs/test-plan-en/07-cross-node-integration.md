# 07. Cross-Node Integration Test Plan (Real multi-node Qnet)

## 0. Purpose & Scope

The other test-plan files (where applicable) verify the internal logic of a
**single FSM** (one node, one process, messages simulated within the same
process/unit test). This file is entirely different: every test case here
**must run on ≥ 2 real QNX machines/VMs, connected over real Qnet**
(no stand-ins, no mocked `name_open`/`MsgSend`). The goal is to prove the
system is genuinely **distributed** — each node is an independent process on
an independent QNX node, communicating cross-node through real
`name_attach`/`name_open`/`MsgSend` — not just 10 logically-correct FSMs
running on the same machine.

Technical background used throughout this file (code was read before
writing this, nothing here is guessed):

- `app/shared/src/qnet_utils.c` / `app/shared/includes/qnet_utils.h`: every
  node calls `name_attach()` once with `NAME_FLAG_ATTACH_GLOBAL` under
  `traffic/<suffix>`; the sending side (`ipc_client_thread_main()` →
  `build_open_path()`) looks up the `TRAFFIC_NODE_MAP` environment variable
  (a comma-separated list of `"<suffix>=<qnet-nodename>"` entries) to decide
  whether to wrap the path with `/net/<nodename>/dev/name/global/...`. A
  suffix absent from the map (including when the variable is unset entirely)
  means "same node as the caller" → `name_open()` is called with no `/net/`
  prefix.
- `app/shared/README.md`, section "Cross-node resolution", and
  `docs/QNX_DEPLOYMENT_RUN_GUIDE.md` sections 2.2/2.3 and section 3
  (Case 1/2/3): the authoritative source for `TRAFFIC_NODE_MAP` syntax and
  the topology examples reused in this file.
- Sending is always **non-blocking and fails gracefully**: `ipc_client_post()`
  only enqueues; `ipc_client_thread_main()` is the sole thread that calls
  `name_open()`/`MsgSend()`; if `name_open()` returns `-1` (peer doesn't
  exist / not running yet / wrong `TRAFFIC_NODE_MAP`), `send_ok = 0` is
  passed to the `on_reply` callback — **no exception, no crash, no hang of
  the process calling `ipc_client_post()`**. This is the mechanism every
  "negative" / "node not up yet" test case in this file exercises.
- `app/central/src/c_operator.c`: 6 real operator commands (`m`, `t`, `o`,
  `r`, `c`, `f`) — the only source of Central → Lx/RLx traffic in the system
  (no other API calls `c_comm_send_*`/`c_comm_broadcast_*`).
- `app/railway/src/rlx_comm.c` function
  `rlx_comm_broadcast_crossing_status_if_changed()`: every time
  `crossing_state_t` changes, it sends `MSG_CROSSING_STATUS` exactly 3
  times — the 2 adjacent Lx (fixed adjacency table: RL1→{L1,L2},
  RL2→{L3,L4}, RL3→{L5,L6}) + C1 — each call to `ipc_client_post()` is
  independent, no transaction, no rollback if 1 of the 3 sends fails.
- `app/intersection/src/lx_comm.c` / `app/railway/src/rlx_comm.c`: a 1-second
  heartbeat to C1 (`MSG_HEARTBEAT`). On the Central side,
  `app/central/src/c_watchdog_mon.c` (`c_watchdog_mon_tick()`, running every
  second via `IPC_PULSE_HEARTBEAT_TICK`,
  `ipc_timer_arm(chid, IPC_PULSE_HEARTBEAT_TICK, 1000, 1000, ...)` in
  `c_main.c`) increments `missed_heartbeat_ticks` on every tick; exactly when
  the counter reaches **3**, `marked_unavailable = 1` and the log
  `"Controller %d marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)"`
  is emitted. Conversely, `c_server.c` (`c_server_record_status()` /
  `c_server_record_crossing_status()`) resets both
  `missed_heartbeat_ticks = 0` and `marked_unavailable = 0` as soon as
  **any** valid message (HEARTBEAT, STATUS, or CROSSING_STATUS) is received
  from that controller — **there is no dedicated log line for transitioning
  back to AVAILABLE**; it can only be observed through the `AVAILABILITY`
  column of the status table (`c_hmi.c`, which prints the exact strings
  `AVAILABLE` / `UNAVAILABLE`).
- `app/intersection/src/lx_fsm.c` function
  `lx_fsm_apply_offset_locked()`: uses `clock_gettime(CLOCK_REALTIME, ...)`
  (wall-clock epoch time, **not** `CLOCK_MONOTONIC`) to compute the phase
  offset relative to `assigned_offset_ms` received from
  `SET_TIMING_PROFILE`, applying it exactly **once**, at the start of the
  **next** `PHASE_ARTERIAL_GREEN` (it does not cut into a green phase
  already in progress). This is why TC-02/TC-03 (arterial green offset)
  only carry **real verification value** when L1/L3/L5 (or L2/L4/L6) run on
  different VMs — each VM's independent system clock is the actual variable
  under test. **Important note**: the code has no NTP/time-sync mechanism
  whatsoever; if the VM clocks drift apart, the applied offset drifts with
  them — this is a known design limitation (see the comment in `lx_fsm.c`),
  not a bug, but the clock drift between VMs **must be checked and recorded
  before evaluating** results for offset-related tests.

## 1. Conventions Used in This File

- **Test case ID**: `TC-XNODE-NN` (NN = 01, 02, ...), distinct from the
  internal FSM `TC-xx` IDs used in the other test-plan files.
- **Type**: `Positive` (correct path) / `Negative` (wrong input/condition,
  system expected to reject or fail gracefully) / `Edge case` (boundary,
  rare but valid).
- **Related**: assumption ID (`PA-xx`, `TC-xx`, `RC-xx`, `CC-xx`, `UC-xx`,
  `SD-xx`) + the specific source file/function that was read.
- **Environment**: always **(C) multiple real QNX machines/VMs over a real
  network (Qnet)** — no exceptions in this file. If a group only has 1 VM,
  this category **cannot be properly tested** and must be recorded as
  "not yet verified" in the report, not treated as equivalent to running
  multiple processes on the same node.
- **`TRAFFIC_NODE_MAP` configuration**: the exact sample value (replace
  `VM1/VM2/VM3` with the real Qnet name obtained from `ls /net` run on that
  VM itself — see section 2.2 of `QNX_DEPLOYMENT_RUN_GUIDE.md`).
- **Setup**: which node runs on which VM, the specific startup order for
  that case (some cases in section 5.7 deliberately break the "recommended
  order" to prove the system doesn't depend on it).
- **Steps**: sequential actions, which may include operator console commands
  (`m`/`t`/`o`/`r`/`c`/`f`), shell commands (`kill`, `Ctrl+C`, restarting a
  binary), or log observation.
- **Expected Result**: an exact description of the log/console line/status
  column that will appear, with the real text string (not a generic
  paraphrase) so the tester can `grep` for it directly.

## 2. Standard Reference Topology (Baseline) Used Throughout This File

Unless a test case says otherwise, every test case in this file defaults to
the topology below — corresponding to **Case 2 "Two Computers"** in the root
`README.md` (2 physical machines) and section 3 "Case 2" of
`docs/QNX_DEPLOYMENT_RUN_GUIDE.md`, broken down further into 3 VMs (one role
per VM, easier to follow logs/consoles when writing test cases) — PC A runs
2 VMs (Central + Intersection), PC B runs 1 VM (Railway):

| VM (suggested name, replace with the real `ls /net` name) | Physical machine | Runs process | Note |
|---|---|---|---|
| `VM1` | PC A | `c_main` (no arguments) | Central — a single process |
| `VM2` | PC A | `lx_main 1`, `lx_main 2`, ..., `lx_main 6` (6 processes, 6 separate SSH shells or backgrounded) | Intersection L1-L6 |
| `VM3` | PC B | `rlx_main 1`, `rlx_main 2`, `rlx_main 3` (3 processes) | Railway RL1-RL3 |

Baseline `TRAFFIC_NODE_MAP` — export in **every SSH shell** before running
the corresponding binary (each process only needs the suffixes it actively
sends to via `ipc_client_post()`; extra entries are harmless, missing ones
just mean messages to that suffix silently never arrive):

```sh
# On VM1 (c_main) - C1 sends to both Lx (VM2) and RLx (VM3)
export TRAFFIC_NODE_MAP="l1=VM2,l2=VM2,l3=VM2,l4=VM2,l5=VM2,l6=VM2,rl1=VM3,rl2=VM3,rl3=VM3"
./c_main

# On VM2 (each lx_main N) - Lx only actively sends HEARTBEAT up to C1 (VM1)
export TRAFFIC_NODE_MAP="c1=VM1"
./lx_main 1        # repeat for 2..6 in other shells, same export

# On VM3 (each rlx_main N) - RLx sends HEARTBEAT/FAULT_REPORT up to C1 (VM1)
# and CROSSING_STATUS to the 2 adjacent Lx (VM2) + C1 (VM1)
export TRAFFIC_NODE_MAP="c1=VM1,l1=VM2,l2=VM2,l3=VM2,l4=VM2,l5=VM2,l6=VM2"
./rlx_main 1        # repeat for 2, 3 in other shells, same export
```

The "recommended" startup order per `README.md`/`QNX_DEPLOYMENT_RUN_GUIDE.md`
is C1 → RLx → Lx, but section 5.7 of this file verifies the system does
**not depend** on that order.

## 3. How to Run Each Node (Quick Reference)

- `./c_main` — no arguments (`app/central/src/c_main.c:126`, `main(void)`).
  Banner: `"C1 (Central Controller) starting..."` then
  `"C1: attached on traffic/c1, server loop starting."`.
- `./lx_main <1-6>` — selects L1..L6 (`parse_self_id()`,
  `app/intersection/src/lx_main.c`). Wrong/missing argument:
  `"usage: %s <1-6>   (selects L1..L6)"`.
- `./rlx_main <1-3>` — selects RL1..RL3 (`app/railway/src/rlx_main.c`).
  Wrong/missing argument: `"usage: %s <1-3>   (selects RL1..RL3)"`.

## 4. Where to Observe Results (Log/Console)

- **Central**: `c_logger_log()` (`app/central/src/c_logger.c`) writes
  simultaneously to **stdout** and to the file `central_log.txt` (opened
  with `fopen("central_log.txt", "a")` — path is **relative to the working
  directory when `c_main` was launched**, `fflush` right after every line).
  Every line has a `[YYYY-MM-DD HH:MM:SS]` timestamp. On VM1:
  `tail -f central_log.txt` or watch the `c_main` terminal directly. The
  status table (`c_hmi.c`) prints the exact strings `AVAILABLE` /
  `UNAVAILABLE` in the last column.
- **Lx/RLx**: **no log file** — only `printf`/`fprintf(stderr, ...)`
  straight to that process's own console. The tester must keep each
  Lx/RLx's SSH terminal open to observe directly (there is no way to review
  it after closing the terminal).
- Send-failure log strings to remember for `grep`/observation:
  - Central (via `on_command_reply()`, `app/central/src/c_comm.c`):
    `"C1: <VERB> to <id> send failed (peer unreachable or send error)"`
    (VERB e.g. `SET_TIMING_PROFILE`, `REQUEST_OVERRIDE`, ... per
    `verb_name()`).
  - Lx (`app/intersection/src/lx_comm.c`):
    `"Lx: HEARTBEAT to <id> failed to send"` or
    `"Lx: HEARTBEAT to C1 dropped - outgoing queue full or stopping"`.
  - RLx (`app/railway/src/rlx_comm.c`):
    `"RLx: message to <id> failed to send"`, or more specifically per verb:
    `"RLx: HEARTBEAT to C1 dropped - outgoing queue full or stopping"`,
    `"RLx: FAULT_REPORT to C1 dropped - outgoing queue full or stopping"`,
    `"RLx: CROSSING_STATUS to <id> dropped - outgoing queue full or stopping"`.
  - Actual light changes (simulated via `printf`,
    `app/intersection/src/lx_signal.c`):
    `"Lx <id>: SIGNAL -> <PHASE NAME>"` (`ARTERIAL GREEN`, `ARTERIAL YELLOW`,
    `ALL RED (A to B)`, `CONNECTOR GREEN`, `CONNECTOR YELLOW`,
    `ALL RED (B to A)`), and when an override ends:
    `"Lx <id>: override cleared/expired - running safe clearance sequence"`.

---

## 5. Test Case List

### 5.1 SET_TIMING_PROFILE cross-node (key `t`)

#### TC-XNODE-01: Broadcast profile R1 to L1/L3/L5 on 3 separate VMs
- **Type**: Positive
- **Related**: UC-03/SD-03, TC-01..TC-05,
  `c_mode_eng_get_chain(C_ARTERIAL_CHAIN_R1)`
  (`app/central/includes/c_mode_eng.h`: L1 offset=0ms, L3 offset=21000ms,
  L5 offset=45000ms, `LX_CYCLE_LENGTH_MS`=90000ms),
  `c_operator.c::handle_timing_profile()`, `lx_fsm_apply_offset_locked()`
  (`app/intersection/src/lx_fsm.c`).
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: extended topology — for a thorough
  demonstration (each Lx target on a different physical/virtual VM, unlike
  the baseline in section 2 where L1-L6 share VM2). If the group doesn't
  have enough VMs, the baseline may be used instead (L1, L3, L5 all on
  VM2) — still a valid cross-node case (C1 ↔ Lx on a different VM), just
  less thorough at proving the 3 Lx are independent of each other; note
  clearly in the report which variant was used.

  ```sh
  # On VM1 (c_main)
  export TRAFFIC_NODE_MAP="l1=VM2A,l3=VM2B,l5=VM2C,rl1=VM3,rl2=VM3,rl3=VM3"
  ./c_main

  # On VM2A (running L1)
  export TRAFFIC_NODE_MAP="c1=VM1"
  ./lx_main 1

  # On VM2B (running L3)
  export TRAFFIC_NODE_MAP="c1=VM1"
  ./lx_main 3

  # On VM2C (running L5)
  export TRAFFIC_NODE_MAP="c1=VM1"
  ./lx_main 5
  ```
- **Setup**: Before starting, run `date` on all 4 VMs (VM1, VM2A, VM2B,
  VM2C) and record the clock drift (if more than a few seconds, note it in
  the results since it directly affects
  `lx_fsm_apply_offset_locked()`). Start C1 first (VM1), then L1/L3/L5
  (VM2A/B/C), in any order among these 3 VMs. Wait for each Lx to print
  `"Lx <n>: attached on traffic/l<n>, server loop starting."` before moving
  to the next step. Put all 3 Lx into `MODE_PEAK_FIXED` first (use the `m`
  command if the default mode isn't PEAK_FIXED).
- **Steps**:
  1. On C1's operator console (VM1), press `t`.
  2. Enter `1` when prompted `"chain (1=R1 L1/L3/L5, 2=R2 L2/L4/L6): "`.
  3. Observe the Central log.
  4. Observe the console of L1 (VM2A), L3 (VM2B), L5 (VM2C).
  5. Record the wall-clock time (each VM's system clock, via `date`) at the
     moment each Lx prints `"SIGNAL -> ARTERIAL GREEN"` at the start of the
     **next** arterial green phase after receiving the profile (not the
     phase already in progress — by design, the offset is only applied at
     the boundary of a new phase).
- **Expected Result**:
  - `central_log.txt`/stdout on VM1 has the line
    `"Operator: SET_TIMING_PROFILE broadcast (chain=R1, profile_id=<N>) submitted"`.
  - All 3 ACK response lines appear, in the form
    `"C1: SET_TIMING_PROFILE to <id> -> ACK"` for id = L1, L3, L5 (numeric
    id per `controller_id_t`, not the letters "L1"/"L3"/"L5").
  - No `"send failed"` or `NACK` lines for these 3 targets.
  - L3's `ARTERIAL GREEN` start lags L1 by approximately 21 seconds, and
    L5 lags L1 by approximately 45 seconds (± the clock error recorded in
    the Setup step, ± up to one full 90s cycle if the Lx is mid-green-phase
    when it receives the profile — a known design limit, not a bug).

#### TC-XNODE-02: Broadcast profile R2 to L2/L4/L6 on 3 separate VMs
- **Type**: Positive
- **Related**: same as TC-XNODE-01 but chain R2
  (`C_ARTERIAL_CHAIN_R2`: L2 offset=0ms, L4 offset=19000ms,
  L6 offset=42000ms).
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: same as TC-XNODE-01, replace
  `l1/l3/l5` with `l2/l4/l6` and `./lx_main 1/3/5` with `./lx_main 2/4/6`.
- **Setup**: same as TC-XNODE-01, using L2/L4/L6 instead of L1/L3/L5.
- **Steps**: same as TC-XNODE-01, at step 2 enter `2` (chain R2).
- **Expected Result**: 3 ACKs for L2, L4, L6; L4 starts `ARTERIAL GREEN`
  about 19 seconds after L2; L6 lags L2 by about 42 seconds (same clock
  drift/one-cycle caveats as TC-XNODE-01).

#### TC-XNODE-03: Broadcast timing profile while 1 target Lx isn't started yet
- **Type**: Negative
- **Related**: `ipc_client_post()`/`ipc_client_thread_main()` graceful
  failure path (`app/shared/src/qnet_utils.c`),
  `c_comm.c::on_command_reply()`.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2): VM1=C1,
  VM2=L1-L6, VM3=RL1-RL3.
- **Setup**: Start C1 (VM1). On VM2, only start L1 and L3
  (`./lx_main 1`, `./lx_main 3`); **deliberately don't run**
  `./lx_main 5` yet.
- **Steps**:
  1. On C1, press `t`, choose chain `1` (R1: L1, L3, L5).
  2. Observe the Central log for a few seconds.
  3. Only then start `./lx_main 5` on VM2.
  4. Check whether L5 automatically receives the profile without C1
     resending it.
- **Expected Result**:
  - L1, L3 receive ACK normally: `"C1: SET_TIMING_PROFILE to <L1 id> -> ACK"`,
    `"C1: SET_TIMING_PROFILE to <L3 id> -> ACK"`.
  - For L5: `"C1: SET_TIMING_PROFILE to <L5 id> send failed (peer unreachable or send error)"`.
  - Central **does not crash, does not hang**, the other two requests are
    still handled normally (each `ipc_client_post()` is independent per
    target within the broadcast loop — one call per chain member).
  - After L5 starts in step 3, L5 **does not automatically receive** the
    previously-broadcast profile (no retry/resend mechanism in the code) —
    the operator must issue `t` again if L5 needs to be synced. This is
    correct behavior per the current design and should be clearly noted in
    the test report as a limitation (not a bug to fix within this test's
    scope).

### 5.2 REQUEST_OVERRIDE / RENEW_OVERRIDE / CANCEL_OVERRIDE cross-node (key `o`/`r`/`c`)

#### TC-XNODE-04: REQUEST_OVERRIDE arterial on an Lx on a different VM — real light change
- **Type**: Positive
- **Related**: UC-08/SD-07, `c_operator.c::handle_request_override()`,
  `lx_fsm.c::lx_fsm_on_request_override()`,
  `lx_fsm_advance_phase_locked()` (applies
  `PHASE_ARTERIAL_GREEN`/`PHASE_CONNECTOR_GREEN` at the next `ALL_RED`
  boundary), `lx_signal.c::lx_signal_show_phase()`. This is a feature that
  was **just fixed** per request — must be tested thoroughly, not just
  checking the ACK.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: Start C1 (VM1), then L2 on VM2 (`./lx_main 2`), make sure L2
  isn't in `SUPERVISORY_CENTRAL_OVERRIDE`/`SUPERVISORY_RAILWAY_PREEMPTION`/
  `SUPERVISORY_FAULT_SAFE` and has no `ped_clearance_active` (default mode,
  no simulated vehicle/pedestrian causing a ped phase to run — if the
  simulated `lx_sensor` has a pending ped request, cancel it first).
- **Steps**:
  1. On C1's console, press `o`.
  2. Enter `2` (Lx number).
  3. Enter `0` (target movement = arterial).
  4. Enter a valid `duration_ms`, e.g. `60000` (≤ 300000 per the
     `LX_OVERRIDE_DURATION_CAP_MS` limit).
  5. Observe the Central log immediately.
  6. Keep the L2 terminal (VM2) open, watch until it crosses the next
     `ALL_RED` boundary (up to one full light cycle).
- **Expected Result**:
  - Central: `"Operator: REQUEST_OVERRIDE(target=<L2 id>, movement=0, duration_ms=60000) submitted"`
    then `"C1: REQUEST_OVERRIDE to <L2 id> -> ACK"` (not `ACK_PENDING`,
    since no ped clearance is running).
  - On L2's console (VM2), the **actually observed order** of log lines is:
    L2 finishes its current phase normally, then at the next `ALL_RED`
    boundary, `"Lx 2: SIGNAL -> ALL RED (A to B)"` appears (or `(B to A)`,
    whichever boundary comes first), **immediately followed by**
    `"Lx 2: SIGNAL -> ARTERIAL GREEN"` — this is the actual evidence the
    light changed per the cross-node override command (not just an ACK at
    the IPC layer).
  - The ACK at the Central step happens almost instantly (< 1s), but the
    real light change at L2 may be delayed up to the rest of the current
    phase — record the observed delay in the report, do not treat it as a
    bug.

#### TC-XNODE-05: REQUEST_OVERRIDE connector on an Lx on a different VM
- **Type**: Positive
- **Related**: same as TC-XNODE-04,
  `target_movement = OVERRIDE_MOVEMENT_CONNECTOR (1)`.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: same as TC-XNODE-04, may use L4 (VM2) instead of L2 to keep it
  separate from TC-XNODE-04 if run back-to-back in the same test session.
- **Steps**: same as TC-XNODE-04 but enter `1` (connector) at step 3.
- **Expected Result**: L4's console prints
  `"Lx 4: SIGNAL -> ALL RED (...)"` then `"Lx 4: SIGNAL -> CONNECTOR GREEN"`
  (a different phase from TC-XNODE-04) — confirms `target_movement` is
  transmitted correctly cross-node, and Lx doesn't misinterpret/default to
  arterial.

#### TC-XNODE-06: REQUEST_OVERRIDE NACKed due to active railway preemption — light doesn't change
- **Type**: Negative
- **Related**: `lx_fsm_on_request_override()` branch
  `fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION` → `RESULT_NACK`,
  `NACK_REASON_RAILWAY_CONFLICT`; RC-02/CC-02.
- **Environment**: (C) multiple real QNX machines/VMs over a real network
  (needs a real RLx to create the preemption condition via Qnet, not
  simulated in-place).
- **`TRAFFIC_NODE_MAP` configuration**: full 3-VM baseline (section 2) —
  needs RL1 (VM3) to actually send `CROSSING_STATUS` to L1 (VM2) so L1
  enters `SUPERVISORY_RAILWAY_PREEMPTION`.
- **Setup**: Start C1 (VM1), L1+L2 (VM2), RL1 (VM3). Trigger RL1's
  simulated sensor to switch to a train-approaching/occupied state (see
  the internal RLx test-plan file for how to trigger it) so that L1
  receives `CROSSING_STATUS` and its `fsm->supervisory` becomes
  `SUPERVISORY_RAILWAY_PREEMPTION`. Confirm via L1's console that
  preemption has taken effect before proceeding.
- **Steps**:
  1. On C1, press `o`, choose Lx `1`, any movement (`0`), a valid duration
     (`60000`).
  2. Observe the Central log.
  3. Watch L1's console — confirm no new `SIGNAL ->` line results from this
     override command.
- **Expected Result**:
  - Central: `"C1: REQUEST_OVERRIDE to <L1 id> -> NACK reason=RAILWAY_CONFLICT"`.
  - L1's console prints no additional `SIGNAL ->` lines beyond those
    already caused by the earlier railway preemption — the light stays in
    the safe state dictated by railway preemption; Central's override is
    fully rejected (no partial effect).

#### TC-XNODE-07: REQUEST_OVERRIDE during active ped clearance — ACK_PENDING, then the light changes later
- **Type**: Edge case
- **Related**: `lx_fsm_on_request_override()` branch
  `fsm->ped_clearance_active` → `OVR_PENDING_CLEARANCE`,
  `RESULT_ACK_PENDING`; the light only changes after
  `lx_fsm_on_phase_timer()` re-validates and transitions to `OVR_ACTIVE`.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: Start C1 (VM1), L3 (VM2). Trigger a pedestrian request on L3
  right before sending the override, so `ped_clearance_active = 1` at the
  moment C1 sends the command (time it against L3's ped phase cycle).
- **Steps**:
  1. Right while L3 is in the ped clearance phase (watch L3's console print
     `PED SIGNAL ... FLASHING_DONT_WALK`/`DONT_WALK`), on C1 press `o`,
     choose Lx `3`, movement `1` (connector), duration `60000`.
  2. Observe the Central log immediately.
  3. Keep L3's console open until ped clearance finishes and it crosses the
     next ALL_RED boundary.
- **Expected Result**:
  - Central: `"C1: REQUEST_OVERRIDE to <L3 id> -> ACK_PENDING"` (distinct
    from the plain `ACK` in TC-XNODE-04/05) — confirms the pending state
    is transmitted correctly over Qnet, and Central/the console doesn't
    mistake it for a plain ACK.
  - L3's light **doesn't change right away** on receiving `ACK_PENDING` —
    it still completes the ped clearance phase as normal.
  - After ped clearance ends, the override automatically moves to
    `OVR_ACTIVE` and L3's light switches to `CONNECTOR GREEN` at the next
    ALL_RED boundary — observe the line
    `"Lx 3: SIGNAL -> CONNECTOR GREEN"` appearing **after** ped clearance
    has completed, not right after ACK_PENDING.

#### TC-XNODE-08: RENEW_OVERRIDE then CANCEL_OVERRIDE cross-node
- **Type**: Positive
- **Related**: UC-08/SD-07/BR-7,
  `c_operator.c::handle_renew_override()`/`handle_cancel_override()`,
  `lx_fsm.c::lx_fsm_terminate_override_locked()`,
  `lx_signal.c::lx_signal_show_override_clearance()`.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: Run TC-XNODE-04 first (L2 currently overriding arterial, with
  a short duration e.g. `15000` ms for easy renew/cancel observation
  during the session).
- **Steps**:
  1. Before the 15s expires, on C1 press `r`, choose Lx `2`,
     `extend_duration_ms` = `30000`.
  2. Observe the Central log to confirm the renewal was sent.
  3. Then press `c`, choose Lx `2` to cancel the override early.
  4. Watch L2's console.
- **Expected Result**:
  - Step 2: `"C1: RENEW_OVERRIDE to <L2 id> -> ACK"` (not NACKed since the
    override is currently active and valid).
  - Step 3: `"Operator: CANCEL_OVERRIDE(target=<L2 id>) submitted"` then
    `"C1: CANCEL_OVERRIDE to <L2 id> -> ACK"`.
  - L2's console: `"Lx 2: override cleared/expired - running safe clearance sequence"`
    appears right after receiving the cross-node CANCEL_OVERRIDE — confirms
    the cancel command from another VM has a real effect on the device, not
    just at the ACK layer.

### 5.3 SET_MODE and REQUEST_FAULT_CLEAR cross-node (key `m`/`f`)

#### TC-XNODE-09: SET_MODE cross-node, reflected in the next HEARTBEAT
- **Type**: Positive
- **Related**: UC-07/SD-03, `c_operator.c::handle_set_mode()`,
  `lx_comm.c::lx_comm_send_heartbeat()` (heartbeat carries the current
  `mode` via `lx_fsm_fill_status()`), status table `c_hmi.c`.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: Start C1 (VM1), L6 (VM2). Confirm L6's current mode on
  Central's status table (usually defaults to `OFF_PEAK_SENSOR`).
- **Steps**:
  1. On C1, press `m`, choose Lx `6`, mode `0` (PEAK_FIXED).
  2. Wait at least 1-2 seconds (one heartbeat cycle) for L6 to send its
     next HEARTBEAT carrying the new mode.
  3. Check Central's status table again (`c_hmi.c` render).
- **Expected Result**:
  - Central: `"Operator: SET_MODE(target=<L6 id>, mode=PEAK_FIXED) submitted"`
    then `"C1: SET_MODE to <L6 id> -> ACK"`.
  - L6's mode column on Central's status table switches to reflect
    `PEAK_FIXED` after the next heartbeat arrives — confirms the mode
    actually changed on the node on the other VM, not just at the command
    layer.

#### TC-XNODE-10: REQUEST_FAULT_CLEAR cross-node to an RLx
- **Type**: Positive
- **Related**: UC-06 alt 7.1/SD-06, RC-09,
  `c_operator.c::handle_request_fault_clear()`.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: Start C1 (VM1), RL2 (VM3). Put RL2 into a fault state (trigger
  a simulated sensor fault per the internal RLx test documentation) so
  there's something to clear.
- **Steps**:
  1. On C1, press `f`, choose RLx `2`.
  2. Observe the Central log.
  3. Watch RL2's console (VM3).
- **Expected Result**:
  - Central: `"Operator: REQUEST_FAULT_CLEAR(target=<RL2 id>) submitted"`
    then `"C1: REQUEST_FAULT_CLEAR to <RL2 id> -> ACK"` (or `NACK` if the
    clear condition isn't yet valid — record the reason if that happens).
  - If ACKed: RL2's fault flag is cleared, observable via RL2's next
    `FAULT_REPORT` to Central (fault_code decreases/returns to 0) —
    confirms the cross-node clear command has a real effect on the RLx,
    consistent with RC-09 ("Central may request a fault clear but must
    not directly operate railway equipment" — RLx decides and executes the
    clear itself).

### 5.4 CROSSING_STATUS broadcast to 3 destinations (RLx → 2 adjacent Lx + C1)

#### TC-XNODE-11: RL1 reports CROSSING_STATUS — L1, L2, and C1 all receive it
- **Type**: Positive
- **Related**: RC-01/RC-02/SD-04/Diagram 4,
  `rlx_comm.c::rlx_comm_broadcast_crossing_status_if_changed()`, adjacency
  RL1→{L1,L2}.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2), make sure
  RL1 on VM3 has `l1=VM2,l2=VM2` (already in the baseline map).
- **Setup**: Start C1 (VM1), L1+L2 (VM2), RL1 (VM3), in any order, but wait
  for all 3 to print the "attached" banner before starting.
- **Steps**:
  1. Trigger RL1's simulated sensor to change `crossing_state_t` (e.g. from
     CLEAR to WARNING) — per RLx sensor console's triggering method.
  2. Immediately watch all 3 consoles: RL1 (VM3), L1 (VM2), L2 (VM2), and
     the Central log (VM1).
- **Expected Result**:
  - RL1's console shows no send failure (`"RLx: message to ... failed to send"`
    or `"CROSSING_STATUS to ... dropped"` do **not** appear).
  - The Central log has an entry acknowledging CROSSING_STATUS from RL1
    (via `c_server_record_crossing_status()` resetting the heartbeat,
    observable indirectly through RL1 remaining `AVAILABLE`/its
    `missed_heartbeat_ticks` returning to 0).
  - Both L1 and L2 (2 independent processes on VM2) react to the new
    state (e.g. `supervisory` switches to `SUPERVISORY_RAILWAY_PREEMPTION`
    if the state is WARNING/OCCUPIED — see the internal Lx test-plan for
    the exact observable signs). **All 3 destinations must receive it — if
    only 1 or 2 of the 3 react, the test FAILS** (this is exactly what this
    category is meant to prove more strictly than a single-node test).

#### TC-XNODE-12: RL1 reports CROSSING_STATUS while L2 isn't started yet — graceful, 2/3 still receive it
- **Type**: Negative / Edge case
- **Related**: same as TC-XNODE-11, plus the graceful-failure path of
  `ipc_client_post()`/`ipc_client_thread_main()`.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: Start C1 (VM1), RL1 (VM3), and **only** L1 on VM2
  (`./lx_main 1`) — deliberately **don't** run `./lx_main 2` yet.
- **Steps**:
  1. Trigger RL1's sensor to change the crossing state (like TC-XNODE-11
     step 1).
  2. Watch RL1's console, L1's console, the Central log.
  3. Only then start `./lx_main 2` on VM2.
  4. Trigger another state change on RL1 (e.g. WARNING → CLEAR) to create
     a new event.
- **Expected Result**:
  - Step 2: RL1's console prints exactly
    `"RLx: CROSSING_STATUS to <L2 id> dropped - outgoing queue full or stopping"`
    **ONLY WHEN** the queue is actually full — in the normal case (L2 not
    running, Qnet name doesn't exist) it actually goes through the
    `name_open()` failure branch in `ipc_client_thread_main()`, leading to
    `on_reply_log_failure()` with `send_ok = 0`, printing
    **`"RLx: message to <L2 id> failed to send"`** (this is the log line
    to actually look for — call this out clearly to avoid confusing the
    two failure forms). L1 and C1 still receive it normally, nothing
    unusual at those 2 destinations.
  - RL1 **doesn't hang, doesn't crash**, and keeps running its normal loop
    (thanks to its dedicated, non-blocking client thread with
    `ipc_client_post()`).
  - Steps 3-4: after L2 starts, it will receive the CROSSING_STATUS of the
    **next** state change — it will **not** receive the state that was
    missed earlier, since
    `rlx_comm_broadcast_crossing_status_if_changed()` only sends when the
    state actually differs from `last_broadcast_state` (no
    resend/backfill). Record this limitation clearly in the report: if L2
    starts late right before a dangerous window, it may not know the
    current crossing state until the next state change — a design risk to
    flag, not something for this test to fix.

### 5.5 Node down / watchdog PA-07 (kill and restart)

#### TC-XNODE-13: Abruptly kill 1 Lx — Central marks it UNAVAILABLE after exactly 3 seconds
- **Type**: Positive
- **Related**: PA-07, `c_watchdog_mon.c::c_watchdog_mon_tick()` (1-second
  tick via `IPC_PULSE_HEARTBEAT_TICK`), `c_hmi.c` (the `AVAILABILITY`
  column).
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: Start C1 (VM1), all of L1-L6 (VM2), RL1-RL3 (VM3). Wait for
  Central's status table to show all 9 controllers `AVAILABLE`.
- **Steps**:
  1. On VM2, pick 1 process, e.g. `lx_main 4` (L4), press `Ctrl+C` (or
     `kill <pid>` from another shell) to kill it abruptly, bypassing any
     clean shutdown sequence.
  2. Note the exact time of the kill.
  3. Watch `central_log.txt`/stdout on VM1 continuously for the next 5
     seconds.
  4. At the same time, watch the remaining Lx/RLx (other processes on VM2,
     and VM3) — confirm they keep operating normally (regular heartbeats,
     no unexpected error logs).
- **Expected Result**:
  - Exactly ~3 seconds after L4's last heartbeat (i.e. ~3 ticks of
    `IPC_PULSE_HEARTBEAT_TICK`), the Central log shows:
    `"Controller <L4 id> marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)"`.
  - Central's status table: L4's row switches its `AVAILABILITY` column
    from `AVAILABLE` to `UNAVAILABLE`; the other rows (L1,L2,L3,L5,L6,RL1,
    RL2,RL3) remain `AVAILABLE`.
  - The remaining processes **don't** hang/crash/log unexpected errors
    because of this event — proving that one dead node doesn't bring down
    the distributed system.
  - If L4 is one of the 2 adjacent Lx of an RLx (e.g. RL2 is adjacent to
    L3/L4), that RLx sending CROSSING_STATUS to L4 while L4 is dead will
    log `"RLx: message to <L4 id> failed to send"` — this is correct
    graceful-failure behavior by design, not a new bug.

#### TC-XNODE-14: Abruptly kill 1 RLx while its crossing is WARNING — state freezes, no crash
- **Type**: Edge case
- **Related**: PA-07, RC-02/CC-02 (Lx depends on the last crossing state it
  received to decide which movements to suppress).
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: Start the full system. Trigger RL2 to switch to WARNING
  (affecting L3, L4 per adjacency).
- **Steps**:
  1. Confirm L3, L4 are now in `SUPERVISORY_RAILWAY_PREEMPTION` (due to
     RL2's WARNING).
  2. On VM3, `kill` the `rlx_main 2` (RL2) process abruptly.
  3. Watch Central for the next 5 seconds.
  4. Watch L3, L4's consoles — do they automatically exit preemption or
     keep their last known safe state?
- **Expected Result**:
  - After ~3 seconds, Central logs
    `"Controller <RL2 id> marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)"`,
    RL2's status table row switches to `UNAVAILABLE`.
  - L3, L4 **keep** their last known preemption state (they don't
    spontaneously treat RL2's lost connection as "safe to reopen
    traffic") — because Lx has no direct link to RLx's
    AVAILABLE/UNAVAILABLE status on Central; it relies solely on the last
    `CROSSING_STATUS` it received. Note this as sound fail-safe behavior
    (losing signal → hold the last restrictive state instead of reopening)
    rather than a bug, but also record any auto-clear timeout found in the
    code (if none is found, state "no auto-expiry mechanism — RL2 must
    restart and send a new state to release it").
  - Central and the other Lx/RLx don't crash/hang because RL2 disappeared.

#### TC-XNODE-15: Restart a killed node — it automatically becomes AVAILABLE on the next heartbeat
- **Type**: Positive
- **Related**: PA-07, `c_server.c::c_server_record_status()` (resets
  `missed_heartbeat_ticks = 0`, `marked_unavailable = 0` as soon as any
  valid message is received — HEARTBEAT/STATUS/CROSSING_STATUS).
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2) — **reuse the
  exact environment variable already exported in the same shell earlier**,
  or export it identically again if opening a new SSH shell (don't skip
  this step when restarting, otherwise you'll accidentally fall into the
  TC-XNODE-17 scenario).
- **Setup**: Continues directly from TC-XNODE-13 (L4 was just killed and
  marked UNAVAILABLE).
- **Steps**:
  1. On VM2, rerun `export TRAFFIC_NODE_MAP="c1=VM1"` (if a new shell) then
     `./lx_main 4`.
  2. Wait for L4 to print the successful attach banner.
  3. Watch Central's status table for up to 2 seconds after L4's first
     heartbeat.
- **Expected Result**:
  - Central's status table: L4's row switches from `UNAVAILABLE` back to
    `AVAILABLE` right at the first valid heartbeat received from L4 —
    **no need to wait for 3 consecutive ticks** (unlike marking it
    unavailable, which needs 3 ticks; recovery only needs 1 heartbeat
    since the reset logic lives in `c_server_record_status()`, which runs
    immediately on message receipt, not on a tick).
  - **There is no dedicated log line announcing "back to AVAILABLE"** —
    this is correct per the current code (a log only exists for the
    transition to UNAVAILABLE); the tester must confirm via the
    `AVAILABILITY` column of the status table, not via log text. Note this
    clearly in the test report to avoid it being mistaken for "missing
    log".

### 5.6 Incorrect `TRAFFIC_NODE_MAP` configuration

#### TC-XNODE-16: TRAFFIC_NODE_MAP points to a Qnet node name that doesn't exist
- **Type**: Negative
- **Related**: `build_open_path()`/`name_open()` returning `-1`, the
  `send_ok = 0` path in `ipc_client_thread_main()`
  (`app/shared/src/qnet_utils.c`).
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: **deliberately wrong** on VM1:
  ```sh
  # On VM1 - "l4" points to a Qnet node name that doesn't actually exist on the network
  export TRAFFIC_NODE_MAP="l1=VM2,l2=VM2,l3=VM2,l4=VM_KHONG_TON_TAI,l5=VM2,l6=VM2,rl1=VM3,rl2=VM3,rl3=VM3"
  ./c_main
  ```
  The other VMs use the normal baseline configuration (section 2).
- **Setup**: Start the full system, including a real L4 on VM2
  (`./lx_main 4`) — i.e. L4 **exists and is running correctly**, only
  VM1's (C1's) `TRAFFIC_NODE_MAP` has the wrong node name for it.
- **Steps**:
  1. On C1, press `m`, choose Lx `4`, any mode.
  2. Observe the Central log.
  3. Right after, try another command to a correctly-configured Lx (e.g.
     `m` to Lx `1`).
- **Expected Result**:
  - Central **doesn't crash, doesn't hang the operator process or the
    client thread**.
  - Central log: `"C1: SET_MODE to <L4 id> send failed (peer unreachable or send error)"`
    — because `name_open("/net/VM_KHONG_TON_TAI/dev/name/global/traffic/l4", 0)`
    returns `-1` (Qnet can't find that node), landing directly in the
    `send_ok = 0` branch.
  - The command to L1 in step 3 (correctly configured) still succeeds
    normally (`"C1: SET_MODE to <L1 id> -> ACK"`) — confirms one bad entry
    in the map doesn't affect other entries, per `node_map_load()`'s
    design of parsing each entry independently.

#### TC-XNODE-17: TRAFFIC_NODE_MAP missing an entry for a peer that's actually remote
- **Type**: Negative
- **Related**: same as TC-XNODE-16 but through a different path —
  `resolve_node()` returns `NULL` (suffix absent), causing
  `build_open_path()` to use the same-node path (`"traffic/l4"`, no
  `/net/` prefix), while L4 is actually on a different VM — `name_open()`
  also returns `-1` but for a different reason (it looks in VM1's own
  local namespace, not found there because that name only exists globally
  on VM2).
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: **deliberately missing** the `l4`
  entry on VM1:
  ```sh
  # On VM1 - "l4=..." is entirely omitted from the list
  export TRAFFIC_NODE_MAP="l1=VM2,l2=VM2,l3=VM2,l5=VM2,l6=VM2,rl1=VM3,rl2=VM3,rl3=VM3"
  ./c_main
  ```
  The other VMs use the normal baseline.
- **Setup**: same as TC-XNODE-16 (real L4, running correctly on VM2).
- **Steps**: same as TC-XNODE-16 (the `m` command to Lx `4`, then a correct
  command to Lx `1`).
- **Expected Result**:
  - Same end-behavior result as TC-XNODE-16 (send fails, log
    `"send failed (peer unreachable or send error)"`, system doesn't
    crash, other commands unaffected) — but a different root cause
    (missing entry → treated as same-node, not a wrong node name). Clearly
    note in the test report that **both different configuration errors
    lead to the same kind of graceful failure at the application layer** —
    this is a strength worth highlighting (neither class of configuration
    error crashes the system).

### 5.7 Starting nodes in an arbitrary order

#### TC-XNODE-18: Start in reverse of the recommended order (Lx first, RLx next, C1 last)
- **Type**: Positive
- **Related**: confirms that the "C1 → RLx → Lx" order stated in
  `README.md`/`docs/QNX_DEPLOYMENT_RUN_GUIDE.md` section 2.3 is only an
  operational recommendation, **not a technical requirement** — each node
  calls `name_attach()` independently and doesn't wait on the others at
  startup, and `ipc_client_post()` is always graceful if the peer isn't
  ready yet (the same mechanism already verified in sections 5.4/5.6).
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2) — no change
  from the standard configuration, export as usual on all 3 VMs **before**
  running the binaries in the new order below.
- **Setup**: Make sure no `c_main`/`lx_main`/`rlx_main` process is running
  on any of the 3 VMs (clean stop from any previous test).
- **Steps**:
  1. On VM2: start all of `./lx_main 1` .. `./lx_main 6` (Lx starts
     **first**, before both C1 and RLx).
  2. Wait 5 seconds (to make sure each Lx's first heartbeat attempts fail
     gracefully since C1 doesn't exist yet) — check each Lx console prints
     `"Lx: HEARTBEAT to C1 dropped - outgoing queue full or stopping"` or
     `"Lx: HEARTBEAT to <C1 id> failed to send"` **without** crashing.
  3. On VM3: start `./rlx_main 1`, `./rlx_main 2`, `./rlx_main 3`
     (RLx starts **second**).
  4. Wait 5 seconds, confirm RLx also doesn't crash even though C1 doesn't
     exist yet (RLx's heartbeat fails gracefully the same way).
  5. Finally, on VM1: start `./c_main`.
  6. Watch Central's status table for the 5 seconds after C1 starts.
- **Expected Result**:
  - No process (Lx, RLx) crashes or hangs while waiting for C1 to not yet
    exist — every failed heartbeat send logs gracefully as described in
    steps 2/4, and the process keeps looping on its 1-second heartbeat
    cycle.
  - Right after C1 starts and finishes attaching
    (`"C1: attached on traffic/c1, server loop starting."`), the next
    valid heartbeat from **every** node (within about ~1 second/node per
    the heartbeat cycle) fills Central's status table with all 9
    `AVAILABLE` rows — **no need to restart** any Lx/RLx that happened to
    start before C1.
  - Send a test operator command (e.g. `m` to Lx `1`) right after the
    status table is fully populated — confirms the system operates
    completely normally, exactly as if started in the recommended order.

#### TC-XNODE-19: Start in another random order (RLx first, Lx next, C1 last; interleaved)
- **Type**: Positive
- **Related**: same as TC-XNODE-18, verifying an additional permutation to
  rule out TC-XNODE-18 "happening to work" due to that specific order.
- **Environment**: (C) multiple real QNX machines/VMs over a real network.
- **`TRAFFIC_NODE_MAP` configuration**: baseline (section 2).
- **Setup**: cleanly stop all processes from the previous test.
- **Steps**:
  1. Start `./rlx_main 2` (RL2 only) on VM3.
  2. Start `./lx_main 3`, `./lx_main 4` (L3, L4 — the 2 Lx adjacent to RL2)
     on VM2.
  3. Start `./c_main` on VM1.
  4. Start the rest: `./rlx_main 1`, `./rlx_main 3` (VM3),
     `./lx_main 1`, `./lx_main 2`, `./lx_main 5`, `./lx_main 6` (VM2) — in
     any interleaved order, no need to follow role groupings.
  5. Once all 9 processes + C1 are running, check Central's status table
     and try a `t` broadcast (chain R1) to confirm the whole system is
     fully operational.
- **Expected Result**:
  - No process crashes at any step, regardless of order.
  - Central's final status table shows all 9/9 `AVAILABLE`, independent of
    the specific startup order in steps 1-4.
  - The `t` command (chain R1) in step 5 receives all 3 ACKs from L1, L3,
    L5 — confirms the system converges to a fully working state regardless
    of startup sequence, as long as every node eventually comes up and each
    side's `TRAFFIC_NODE_MAP` is correct.

---

## 6. Quick Traceability Summary Table

| TC | Flow | Type | Nodes required on different VMs |
|---|---|---|---|
| TC-XNODE-01 | SET_TIMING_PROFILE R1 | Positive | C1, L1, L3, L5 |
| TC-XNODE-02 | SET_TIMING_PROFILE R2 | Positive | C1, L2, L4, L6 |
| TC-XNODE-03 | SET_TIMING_PROFILE, 1 target not up yet | Negative | C1, L1, L3, (L5 late) |
| TC-XNODE-04 | REQUEST_OVERRIDE arterial | Positive | C1, L2 |
| TC-XNODE-05 | REQUEST_OVERRIDE connector | Positive | C1, L4 |
| TC-XNODE-06 | REQUEST_OVERRIDE NACKed (railway conflict) | Negative | C1, L1, RL1 |
| TC-XNODE-07 | REQUEST_OVERRIDE during ped clearance | Edge case | C1, L3 |
| TC-XNODE-08 | RENEW/CANCEL_OVERRIDE | Positive | C1, L2 |
| TC-XNODE-09 | SET_MODE | Positive | C1, L6 |
| TC-XNODE-10 | REQUEST_FAULT_CLEAR | Positive | C1, RL2 |
| TC-XNODE-11 | CROSSING_STATUS to 3 destinations (all up) | Positive | RL1, L1, L2, C1 |
| TC-XNODE-12 | CROSSING_STATUS, 1 target not up yet | Negative/Edge | RL1, L1, (L2 late), C1 |
| TC-XNODE-13 | Kill 1 Lx, 3s watchdog | Positive | C1, all Lx/RLx |
| TC-XNODE-14 | Kill 1 RLx during WARNING | Edge case | C1, L3, L4, RL2 |
| TC-XNODE-15 | Restart a killed node | Positive | C1, L4 |
| TC-XNODE-16 | TRAFFIC_NODE_MAP wrong node name | Negative | C1, L4 (real) |
| TC-XNODE-17 | TRAFFIC_NODE_MAP missing entry | Negative | C1, L4 (real) |
| TC-XNODE-18 | Start in reverse order | Positive | all 10 nodes |
| TC-XNODE-19 | Start in a different interleaved order | Positive | all 10 nodes |
