# F-10 — Demo Fault-Injection: Railway Gate Confirmation Failure

**Trigger:** keypress `x` (arm next gate motion to fail confirmation) / `r` (force confirmed-open, simulate repair) — both in `rlx_sensor.c : rlx_sensor_reader_thread()`
**Scope:** single-node — RLx only, **injection side only**; see `UC-06-respond-to-railway-equipment-fault.md` for the response side (from `enter_fault()` onward)
**Spec:** F-10 demo/testing feature — RC-06 (arm fault), RC-09/RC-10 (fault-clear demo)

```mermaid
---
title: F-10 — Demo Fault-Injection into rlx_gate.c (injection side only)
---
flowchart TD
    subgraph Path1["Path 1 - arm a confirmation failure ('x')"]
        X["keypress 'x'"] --> ARM["rlx_gate_arm_demo_fault()<br/>one-shot flag: g_demo_fault_armed = 1"]
        ARM -->|"idle until next motion command"| CMD["rlx_gate_command_close()/open()<br/>consumes flag:<br/>g_fail_this_motion = g_demo_fault_armed<br/>g_demo_fault_armed = 0"]
        CMD --> TICK["rlx_gate_on_tick()<br/>countdown → 0 with flag set:<br/>g_confirmed_closed/open left at 0"]
        TICK --> DEADLINE["rlx_fsm.c deadline check<br/>check_closing_or_reclosing_complete() /<br/>check_opening_complete()<br/>state_elapsed_ms ≥ 15000ms"]
        DEADLINE --> EF["enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING)<br/>rlx_fsm.c"]
    end

    subgraph Path2["Path 2 - force a 'repair' ('r')"]
        R["keypress 'r'"] --> FORCE["rlx_gate_force_confirmed_open()<br/>NOT a flag — immediate mutation:<br/>g_motion=IDLE, g_fail_this_motion=0,<br/>g_demo_fault_armed=0, g_confirmed_open=1"]
        FORCE -->|"read on next poll"| FC["rlx_fsm_on_fault_clear()<br/>gates_confirmed_open() now true"]
    end

    EF -.->|"see UC-06's doc from here"| UC6A[["UC-06: local safety actions<br/>+ independent fault report"]]
    FC -.->|"see UC-06's doc from here"| UC6B[["UC-06: RESULT_ACK,<br/>state = RLX_OPEN"]]
```

Deadlines (`rlx_timer.h`): `RLX_CLOSING_DEADLINE_MS=15000` · `RLX_OPENING_DEADLINE_MS=15000` · motion tick = `RLX_GATE_MOTION_MS=3000`.
Third key `f` (`rlx_sensor.c`) calls `rlx_fsm_on_fault_clear()` directly — a demo shortcut for UC-06's *clearance* half (skips the IPC round trip), not part of this injection feature; not traced here.

## Code map

| Step | File : Function |
|---|---|
| Arm keypress | `rlx_sensor.c : rlx_sensor_reader_thread()` (`x`) → `rlx_gate.c : rlx_gate_arm_demo_fault()` |
| Flag consumed | `rlx_gate.c : rlx_gate_command_close()` / `rlx_gate_command_open()` — copies `g_demo_fault_armed` into `g_fail_this_motion` |
| Countdown misses confirm | `rlx_gate.c : rlx_gate_on_tick()` — `g_confirmed_closed`/`g_confirmed_open` stay 0 |
| Deadline → real fault | `rlx_fsm.c : check_closing_or_reclosing_complete()` / `check_opening_complete()` → `enter_fault()` |
| Repair keypress | `rlx_sensor.c : rlx_sensor_reader_thread()` (`r`) → `rlx_gate.c : rlx_gate_force_confirmed_open()` |
| Repair consumed | `rlx_fsm.c : rlx_fsm_on_fault_clear()` — `gates_confirmed_open()` reads true → `RESULT_ACK` |

Both keys run on the RLx sensor thread with **no `fsm->lock` held** — `rlx_gate.c`'s own `g_gate_lock` is always the innermost/leaf lock, so there's no lock-ordering hazard with `rlx_fsm.c`. `rlx_gate_arm_demo_fault()` is a bare, idempotent latch; `rlx_gate_force_confirmed_open()` is the odd one out — it never touches `rlx_fsm_t` and mutates gate state immediately instead of waiting to be consumed.

## Cross-node view

Single-node by construction. Injection touches only `rlx_gate.c`'s process-local statics (`g_motion`, `g_remaining_ms`, `g_fail_this_motion`, `g_demo_fault_armed`, `g_confirmed_closed`, `g_confirmed_open`) under `g_gate_lock` — no `MsgSend()`/`ipc_client_post()` anywhere in either path. Cross-node wire traffic (`MSG_FAULT_REPORT`, `MSG_CROSSING_STATUS`, `MSG_REQUEST_FAULT_CLEAR`) only begins *after* the handoff into `enter_fault()` (Path 1) or `rlx_fsm_on_fault_clear()` (Path 2) — both covered in `UC-06-respond-to-railway-equipment-fault.md`'s own Cross-node view.
