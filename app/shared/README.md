# Shared IPC Contract — `sys_types.h` / `ipc_msg.h`

This directory holds the one header pair all three executables (`c_main`,
`lx_main`, `rlx_main`) compile against. Before this change both files were
empty stubs (see `docs/QNX_PROJECT_FILE_OVERVIEW.md`, which listed every
file's status as `TODO`) — nothing here was implemented differently, it
simply didn't exist yet. This README explains what was written and why,
so a reviewer doesn't have to reverse-engineer the reasoning from the code.

## What each file is for

- **`sys_types.h`** — enums/bitmasks with no behaviour: controller IDs,
  operating mode, crossing state, connectivity state, fault flags, NACK
  reason codes. Anything that's just a *value*, not a message.
- **`ipc_msg.h`** — the message envelope (`ipc_request_t` / `ipc_reply_t`)
  and one payload struct per cross-node verb. Anything that actually
  crosses a Qnet `MsgSend()`/`MsgReceive()`/`MsgReply()` boundary.

## Why the scope is "cross-node only"

`SEQUENCE_DIAGRAMS.md` and `usecase.md` use verbs like `DEMAND_PRESENT`,
`PED_REQUEST`, `TRAIN_APPROACHING`, `QUEUE_WARNING`, and actuator commands
like `GREEN`/`WALK`/`START_FLASHING`. Those all happen **inside one
process** (a sensor or actuator task talking to its own node's FSM), not
between nodes over Qnet. Putting them in the shared contract would force
every node to recompile whenever another node's internal sensor handling
changes, which defeats the point of a shared header being small and
stable. They belong in each node's own private header (`lx_*.h`, `rlx_*.h`).

The verbs that *do* cross a node boundary — confirmed by grepping every
`->>`/`-->>` arrow in `SEQUENCE_DIAGRAMS.md` between distinct
participants — are exactly the ones in `msg_type_t`:
`C1 <-> Lx` (`SET_TIMING_PROFILE`, `SET_MODE`, `REQUEST_OVERRIDE`,
`RENEW_OVERRIDE`, `CANCEL_OVERRIDE`, `REQUEST_FAULT_CLEAR`, `HEARTBEAT`),
`C1 <-> RLx` (`REQUEST_FAULT_CLEAR`, `FAULT_REPORT`, `HEARTBEAT`), and
`RLx -> Lx` / `RLx -> C1` (`CROSSING_STATUS`).

Re-audit note: `MSG_STATUS` is also declared in `msg_type_t` (`Lx/RLx ->
C1`) and `c_main.c` fully implements the receiving side, but no sender for
it exists anywhere today — every controller only ever sends the
equivalent information via the periodic `MSG_HEARTBEAT` (which reuses the
same `status_report_payload_t` shape). This is a known, accepted gap, not
an oversight in this list.

## Why the envelope has fields beyond "type / sender / content / result"

Requested fields were: message type, sender, controller id, content,
result. Three more turned out to be load-bearing, not optional extras:

- **`timestamp_ms`** — without it, `PA-07`'s "3 consecutive missed
  heartbeats" and `PA-08`'s "send complete current state on reconnect"
  have no way to measure elapsed time, and `c_logger.c`'s job is
  explicitly to write *timestamped* events.
- **`target_id`** — `CROSSING_STATUS` goes to three different recipients
  per event (both adjacent `Lx` controllers - e.g. L1 and L2 for RC1 -
  plus `C1`, per Diagram 4/RC-01/SD-04); a single `sender_id` can't tell
  a receiver which relationship a message belongs to. In practice this
  means `rlx_main` calls `ipc_client_post()` three times per crossing
  event, once per recipient - don't forget the second adjacent Lx.
- **`reason` (`nack_reason_t`), separate from `result`** — every NACK in
  the sequence diagrams carries a reason (`PA-11` bad duration, `CC-02`
  railway conflict, `PA-02` pedestrian clearance in progress, etc.). A
  bare pass/fail `result` would force `c_hmi.c`/`c_logger.c` to parse
  free text to know why something was rejected.

`seq_num` was added but marked optional in a comment — no spec document
requires request/reply correlation, but it costs 4 bytes and helps
debugging/log-matching later, so it's there without being relied on by
any required behaviour.

## Why every struct starts with `msg_header_t`, and why nothing is a plain enum field

Checked against `Lecture/Lecture02_Concurrent_Processes.pdf` (slides 25-26)
and `Lecture/lap_6/Lab_06_Task1a_server.c`/`client.c` (the course's own
cross-node IPC example) — two rules apply here specifically because C1,
Lx, and RLx are separate Qnet nodes that may run on different
architectures (x86 VM vs. an ARM target), not just threads in one process:

- **Header first.** `msg_header_t` mirrors `struct _pulse`'s exact layout
  (not the real type, since `sival_ptr` is a different size on 32-bit vs.
  64-bit) and is the first member of both `ipc_request_t` and
  `ipc_reply_t`. This is required, not stylistic: `MsgReceive()` writes
  pulses (`_PULSE_CODE_DISCONNECT`, and any timer pulse landing on the
  same channel) into the same buffer as a real message. If the header
  didn't overlay `_pulse`'s layout, reading `hdr.code`/`hdr.scoid` after
  `rcvid == 0` would read garbage instead of the real pulse code.
- **No enum-typed wire fields.** `msg_type_t`, `msg_result_t`,
  `operating_mode_t`, `crossing_state_t`, etc. are still used as named
  constants, but every struct field that actually crosses a node is
  declared `uint32_t` (or `uint8_t` for small/no-cross-arch-risk fields
  like `severity`), with a comment saying which enum it holds. A plain C
  enum's underlying storage width is implementation-defined; explicit
  fixed-width fields remove that risk instead of hoping the compiler
  picks the same width on every target.

Lab_06_Task1a used the same custom pulse-mirroring header for exactly
this reason (its comment: "due to the difference in data-type size on
64-bit and 32-bit systems, the header struct... has been changed to use
a custom header"). Lab_06_Task2a-2c (same-node `ChannelCreate()`/
`ConnectAttach()`, not `name_attach()`) use the real `struct _pulse`
directly and explicitly say they don't need the custom header "as the
channels will not be communicating across different target
architectures" — that's the ChannelCreate case, not ours; C1/Lx/RLx talk
over Qnet (`name_attach()`/`name_open()`), so the custom header applies.

## Traceability

Every enum value and payload field has a comment pointing at the
assumption ID (`TC-xx`, `PA-xx`, `RC-xx`, `DP-xx`, `CC-xx`) or sequence
diagram it comes from. If a value doesn't map to one of those, it was a
plain implementation necessity (e.g. `profile_id` to distinguish which
coordination profile a `SET_TIMING_PROFILE` belongs to) — noted inline
as such rather than invented and left unexplained.

## Threading pattern: every node runs exactly 2 dedicated IPC threads

Checked against `Lecture/Lecture02_Concurrent_Processes.pdf` slide 28:
*"Avoid designs where two processes synchronously send to each other at
the same time. For deadlock-free message systems, keep a hierarchy:
clients send upward to servers, and servers reply downward."*

C1, Lx, and RLx each have **mixed roles** with the same peer - e.g. C1 is
a client when sending `SET_MODE` to L1, but a server when receiving
`HEARTBEAT` from L1. If one thread tried to do both jobs, a blocking
outgoing `MsgSend()` would stop that same thread from calling
`MsgReceive()` - for Lx this is a real safety problem, not just
sluggishness: if Lx is blocked sending its own `HEARTBEAT` to C1 at the
exact moment `RLx` needs to deliver `CROSSING_STATUS(WARNING)`, Lx cannot
suppress the toward-crossing movement (CC-02) until the send unblocks.

Fix: every node splits its IPC into two threads that never share work:

1. **Server thread** — `ipc_attach()` once, then `ipc_server_run()`
   forever. Never calls `MsgSend()`. This is the only rule that matters
   for slide 28: the thread that can be sent to never itself blocks
   sending to someone else.
2. **Client thread** — `ipc_client_thread_main()` forever, draining the
   queue `ipc_client_post()` feeds. This is the only code in a node
   allowed to call `MsgSend()`.

A third thread (the node's own `main()`/FSM) owns local state; anything
it needs to send goes through `ipc_client_post()` (non-blocking) instead
of calling `MsgSend()` itself.

This exceeds the "2 threads including main()" a student is shown in
`Lecture/lap_6/Lab_06_Task2c_statemachine.c` (that example is 1 role -
pure server - plus main; ours is main + server + client = 3, because our
nodes genuinely need both roles to the same peers, which the lab's
sensor→state-machine example never had to solve).

## Timers: `SIGEV_PULSE` on the node's own channel

Two designs were compared: `SIGEV_PULSE` (pulse lands on the same channel
as `ipc_server_run()`'s `MsgReceive()` loop) vs. `SIGEV_THREAD` (spawns a
new OS thread every time a timer fires). `Lecture04_Data_Protection_and_
Synchronization.pdf` slide 53 is the one fully-worked timer example in
the lecture slides and it uses `SIGEV_THREAD`, with its own warning:
*"if you specify too short an interval, you'll be flooded with new
threads."* That warning is exactly the failure mode a 1 s heartbeat or a
4 s demand-extension recheck would trigger, so `SIGEV_THREAD` was
rejected for those.

`SIGEV_PULSE` is what `Lecture/lap_5/Lab_05_Task3a.c` and
`Lab_05_Task3b.c` ("Timer-Driven Traffic Lights" - same domain as this
project) actually implement and what students submitted for grading, and
`RTS_Lab_Ex_5.pdf` p.4 names it directly: *"QNX also supports a
non-POSIX extension that allows the timer to send a pulse when the timer
expires (as in the example)."* `ipc_timer_arm()` in `qnet_utils.c`
reproduces that exact setup (`ConnectAttach()` back to the node's own
channel, `SIGEV_PULSE`, `timer_create()`/`timer_settime()`).

Lab 5's example only ever needed one timer at a time (reconfigured per
state via `timer_settime()`). This project needs several running
concurrently per node - e.g. `RLx` tracking two overlapping RC-04
occupancy windows while a heartbeat also ticks - so each concurrent
purpose gets its own pulse code (`IPC_PULSE_PHASE_TIMER`,
`IPC_PULSE_HEARTBEAT_TICK`, `IPC_PULSE_RAILWAY_WARNING`,
`IPC_PULSE_RAILWAY_OCCUPANCY` in `qnet_utils.h`), all landing on the same
`ipc_server_run()` loop and disambiguated the same way the lab
disambiguates `_PULSE_CODE_DISCONNECT` from a timer pulse: by
`msg.hdr.code`. This is an extension of the lab's pattern (multiple
codes on one channel), not a different mechanism.

## Qnet attach-point names

`qnet_utils.h`/`qnet_utils.c` are the single source of truth for this —
if this table and the code ever disagree, the code wins and this table
is stale.

10 attach points total, one per node, format `traffic/<suffix>`:

| Controller | `controller_id_t` | Attach name |
|---|---|---|
| Central | `CTRL_C1` | `traffic/c1` |
| Intersection 1-6 | `CTRL_L1`..`CTRL_L6` | `traffic/l1` .. `traffic/l6` |
| Railway 1-3 | `CTRL_RL1`..`CTRL_RL3` | `traffic/rl1` .. `traffic/rl3` |

Each node calls `name_attach()` **once** at startup with its own name;
every peer that needs to talk to it calls `name_open()` on that same
string. There is one channel per node, not one per message type — the
`verb` field inside `ipc_request_t` (see `ipc_msg.h`) tells the receiver
which verb it's looking at, so `C1` doesn't need ten different channels
to talk to ten peers, and neither does any `Lx`/`RLx`. (`ipc_request_t.hdr.type`
is a different field entirely — part of the pulse-compatible header, used
only for `_IO_CONNECT`/pulse detection inside `ipc_server_run()`, and
explicitly zeroed by `ipc_client_post()` so it never collides with verb
dispatch.)

`ipc_attach_name(controller_id_t)` is the one function that knows this
mapping; nothing else should hardcode a `"c1"`/`"l3"`/`"rl2"` string.

### Cross-node resolution: `NAME_FLAG_ATTACH_GLOBAL` + `TRAFFIC_NODE_MAP`

An earlier version of this codebase called `name_attach(NULL, path, 0)`
and `name_open(path, 0)` with flags `0`, which QNX Neutrino resolves in
the **node-local** namespace only (`/dev/name/local/...`). That's fine
when every controller runs in one process/one node, but it silently
breaks the moment `c_main`/`lx_main`/`rlx_main` run on separate Qnet
nodes — exactly the multi-VM topologies
`docs/QNX_DEPLOYMENT_RUN_GUIDE.md` documents — because a node-local name
is never visible to `name_open()` calls originating on a different node.
`name_open()` just returns `-1` (handled gracefully by
`ipc_client_thread_main()`'s `send_ok` path), so the failure mode was
messages silently never arriving, not a crash.

Fix, now implemented in `qnet_utils.c`:

- **Attach side (`ipc_attach()`)**: registers with `NAME_FLAG_ATTACH_GLOBAL`
  instead of `0`, i.e. under `/dev/name/global/traffic/<suffix>`. Per QNX
  Neutrino name-service semantics, a global-namespace name is *still*
  found by a plain, prefix-less `name_open()` from a process on the same
  node — global vs. local only changes whether *other* nodes can see the
  name via Qnet, not whether the owning node can still see its own name
  the old way. **Existing single-machine/same-node setups need no change
  and keep working exactly as before.**
- **Open side (`ipc_client_thread_main()`, via the internal
  `build_open_path()` helper)**: consults the `TRAFFIC_NODE_MAP`
  environment variable — a comma-separated `<suffix>=<qnet-nodename>`
  list, parsed once (cached) on first use — to decide whether a target
  controller lives on a different node:
  - **Suffix absent from the map (including `TRAFFIC_NODE_MAP` unset
    entirely, the default)**: resolves as "same node as the caller",
    identical to the pre-fix `name_open("traffic/<suffix>", 0)` call.
    Zero risk to the current test setup.
  - **Suffix present in the map**: builds
    `/net/<nodename>/dev/name/global/traffic/<suffix>` instead — QNX's
    standard Qnet path for reaching a name registered with
    `NAME_FLAG_ATTACH_GLOBAL` on another node. (The
    `/net/<nodename>/dev/name/<namespace>/<name>` shape itself is not
    invented here — it mirrors the *local*-namespace example
    `Lecture/lap_6/Lab_06_Task1b_client_602.c` hardcodes,
    `/net/VM_x86_Target01/dev/name/local/thang`, with `local` swapped for
    `global` to match the namespace `ipc_attach()` now registers into.)

Both `build_path()` (the attach-name convention) and `build_open_path()`
(the client's resolution logic) are internal to `qnet_utils.c`;
`build_open_path()` always calls `build_path()` first and only wraps its
output, so the naming convention itself is defined in exactly one place.

Example mapping for the deployment guide's "one VM per role" topology
(Case 1/Case 3 — one node runs `c_main`, one runs all of `lx_main`'s
`L1`-`L6` instances, one runs all of `rlx_main`'s `RL1`-`RL3` instances):

```sh
export TRAFFIC_NODE_MAP="c1=VM_x86_Target01,\
l1=VM_x86_Target02,l2=VM_x86_Target02,l3=VM_x86_Target02,\
l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,\
rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"
```

Set this (with real Qnet node names — whatever `ls /net` on a target
shows for its peers, per `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`) in the shell
that launches each binary, before running it — every controller_id_t
`ipc_client_post()` might target should be covered on every node's own
copy of the variable, since it only affects that process's own outgoing
`name_open()` calls, not what it attaches as. See
`docs/QNX_DEPLOYMENT_RUN_GUIDE.md` for the full walkthrough.

**Judgment call / not verified on real hardware:** `NAME_FLAG_ATTACH_GLOBAL`'s
exact numeric value is defined by the real QNX SDP `<sys/neutrino.h>`,
which isn't available on this development machine — `qnet_utils.c` only
ever uses the symbolic macro name, never a hardcoded value, so this is
safe on an actual QNX target. The host-only syntax-check stub
(`tools/host_syntax_stubs/sys/neutrino.h`, used by `make check-syntax`) fakes a plausible value
(`0x2`) purely so a non-QNX compiler can typecheck the `.c` file; that
stub value is never linked into a real binary. This whole cross-node path
is otherwise unverified end-to-end since no QNX SDP toolchain exists in
this environment — test it early on real multi-VM hardware.

## Naming note

The header here uses `MSG_`-prefixed C enum identifiers (`MSG_SET_MODE`,
`MSG_HEARTBEAT`) purely as normal C naming convention; they map 1:1 to the
spec's `SET_MODE` / `HEARTBEAT` verbs, not to a separately-invented name.
(`docs/QNX_PROJECT_FILE_OVERVIEW.md` previously described this contract
using the older `SET_OPERATION_MODE`/`MSG_HEARTBEAT`-as-distinct-verb
wording; that doc has since been reconciled to use `SET_MODE`/`HEARTBEAT`
consistently with this header, so no discrepancy remains.)
