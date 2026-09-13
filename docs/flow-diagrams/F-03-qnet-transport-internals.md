# F-03 — Qnet Transport Internals

**Trigger:** any thread calling `ipc_client_post()` (outbound request) · each node's `main()` calling `ipc_timer_arm()` at startup (periodic pulse)
**Scope:** infrastructure shared by every node (`c_main`, `lx_main`, `rlx_main`) — not itself a use case; every `MSG_*` send and every tick in UC-01..UC-10 goes through this
**Spec:** `qnet_utils.h` / `qnet_utils.c` — F-03 infrastructure feature (not named in `usecase.md`)

## (a) Outgoing queue: producer enqueues → consumer thread dequeues → send → reply

```mermaid
sequenceDiagram
    participant P as Producer thread<br/>(on_pulse/on_request,<br/>or FSM/sensor/operator thread)
    participant Q as ring buffer<br/>16 slots, q->lock, q->not_empty
    participant C as Client thread<br/>ipc_client_thread_main()
    participant N as Target node (Qnet)

    P->>Q: ipc_client_post(): lock, enqueue, cond_signal, unlock
    Note over P: returns immediately - never blocks on I/O
    Q-->>C: wakes from cond_wait(not_empty)
    C->>C: dequeue job, unlock, build_open_path()
    C->>N: name_open(path)
    alt name_open fails on /global/ path
        C->>N: retry name_open() on /local/ path
    end
    C->>N: MsgSend(coid, &req, &reply)
    N-->>C: reply, or send_ok=0 on failure
    C->>N: name_close(coid)
    C->>P: job.on_reply(target_id, req, reply, send_ok, ctx)
    Note over C: reply callback runs ON THE CLIENT THREAD
```

`IPC_CLIENT_QUEUE_CAPACITY=16` (fixed array, not a growable list). Full queue or `stopping` ⇒ `ipc_client_post()` returns `-1` immediately, never blocks. `qnet_utils.c:384-385` zeroes `req.hdr.type`/`subtype` before enqueueing so it can never collide with the reserved `_IO_*` ranges handled below.

## (b) Timer/pulse dispatch: N `ipc_timer_arm()` calls → one channel → code-based fan-out in `ipc_server_run()`

```mermaid
flowchart LR
    T1["ipc_timer_arm()\nIPC_PULSE_PHASE_TIMER\n(Lx, 100ms)"] -->|SIGEV_PULSE via ConnectAttach| CH
    T2["ipc_timer_arm()\nIPC_PULSE_HEARTBEAT_TICK\n(every node, 1s)"] -->|SIGEV_PULSE via ConnectAttach| CH
    T3["ipc_timer_arm()\nIPC_PULSE_RAILWAY_WARNING\n(RLx, 1s)"] -->|SIGEV_PULSE via ConnectAttach| CH
    K["kernel _PULSE_CODE_DISCONNECT"] --> CH
    CH(["one chid<br/>ipc_attach()"]) --> MR["MsgReceive()<br/>ipc_server_run(), server thread"]
    MR -->|"rcvid == 0 (pulse)"| SW{"switch(msg.hdr.code)"}
    MR -->|"rcvid != 0 (message)"| ONREQ["on_request() -> MsgReply()"]
    SW -->|PHASE_TIMER| P1["on_pulse(): lx_fsm_on_phase_timer()"]
    SW -->|HEARTBEAT_TICK| P2["on_pulse(): heartbeat send / watchdog tick"]
    SW -->|RAILWAY_WARNING| P3["on_pulse(): rlx_fsm_on_tick()"]
    SW -->|"default (incl. DISCONNECT)"| P4["on_pulse(): no-op, continue loop"]
```

Application pulse codes start at `_PULSE_CODE_MINAVAIL` (`qnet_utils.h:80-85`), so they never collide with the kernel's own reserved codes (e.g. `_PULSE_CODE_DISCONNECT`). A pulse (`rcvid==0`) is never `MsgReply()`'d; a real message always is.

## Code map

| Step | File : Function |
|---|---|
| Enqueue job | `qnet_utils.c : ipc_client_post()` (365-393) — ring buffer, never blocks |
| Dequeue + send | `qnet_utils.c : ipc_client_thread_main()` (401-449) — `name_open`/`MsgSend`/`name_close`, lock released before I/O |
| Reply callback | runs on client thread — `rlx_comm.c : on_reply_log_failure()`, `lx_comm.c`'s variant (also checks `RESULT_ACK`) |
| Arm one timer | `qnet_utils.c : ipc_timer_arm()` (240-276) — `timer_create`/`timer_settime` + `SIGEV_PULSE` via `ConnectAttach(ND_LOCAL_NODE, ..., chid, ...)` |
| Receive + dispatch | `qnet_utils.c : ipc_server_run()` (278-320) — `MsgReceive()`; `rcvid==0` → `on_pulse()`; else → `on_request()` → `MsgReply()` |
| Pulse handling | each node's own `on_pulse()` — `lx_main.c`, `rlx_main.c`, `c_main.c` |

## Cross-node view

| Node | `ipc_timer_arm()` calls | Codes armed |
|---|---|---|
| **C1** (`c_main.c:289`) | 1 | `IPC_PULSE_HEARTBEAT_TICK` (1000ms) |
| **Lx** (`lx_main.c:185,190`) | 2 | `IPC_PULSE_PHASE_TIMER` (100ms), `IPC_PULSE_HEARTBEAT_TICK` (1000ms) |
| **RLx** (`rlx_main.c:148,156`) | 2 | `IPC_PULSE_HEARTBEAT_TICK` (1000ms), `IPC_PULSE_RAILWAY_WARNING` (1000ms, doubles as FSM tick) |

`IPC_PULSE_RAILWAY_OCCUPANCY` exists but is never armed — `rlx_main.c`'s `on_pulse()` treats it as a no-op since `IPC_PULSE_RAILWAY_WARNING`'s tick already drives both RC-03 and RC-04 internally. Every node shares one `chid` across all its timers and one `ipc_client_queue_t` across all its outbound sends — both mechanisms are per-node singletons that every other feature builds on, never around.
