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
`RENEW_OVERRIDE`, `CANCEL_OVERRIDE`, `HEARTBEAT`), `C1 <-> RLx`
(`REQUEST_FAULT_CLEAR`, `FAULT_REPORT`, `HEARTBEAT`), and `RLx -> Lx` /
`RLx -> C1` (`CROSSING_STATUS`).

## Why the envelope has fields beyond "type / sender / content / result"

Requested fields were: message type, sender, controller id, content,
result. Three more turned out to be load-bearing, not optional extras:

- **`timestamp_ms`** — without it, `PA-07`'s "3 consecutive missed
  heartbeats" and `PA-08`'s "send complete current state on reconnect"
  have no way to measure elapsed time, and `c_logger.c`'s job is
  explicitly to write *timestamped* events.
- **`target_id`** — `CROSSING_STATUS` goes to two different recipients
  (the adjacent `Lx` and `C1`) from the same event; a single `sender_id`
  can't tell a receiver which relationship a message belongs to.
- **`reason` (`nack_reason_t`), separate from `result`** — every NACK in
  the sequence diagrams carries a reason (`PA-11` bad duration, `CC-02`
  railway conflict, `PA-02` pedestrian clearance in progress, etc.). A
  bare pass/fail `result` would force `c_hmi.c`/`c_logger.c` to parse
  free text to know why something was rejected.

`seq_num` was added but marked optional in a comment — no spec document
requires request/reply correlation, but it costs 4 bytes and helps
debugging/log-matching later, so it's there without being relied on by
any required behaviour.

## Traceability

Every enum value and payload field has a comment pointing at the
assumption ID (`TC-xx`, `PA-xx`, `RC-xx`, `DP-xx`, `CC-xx`) or sequence
diagram it comes from. If a value doesn't map to one of those, it was a
plain implementation necessity (e.g. `profile_id` to distinguish which
coordination profile a `SET_TIMING_PROFILE` belongs to) — noted inline
as such rather than invented and left unexplained.

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
`type` field inside `ipc_request_t` (see `ipc_msg.h`) tells the receiver
which verb it's looking at, so `C1` doesn't need ten different channels
to talk to ten peers, and neither does any `Lx`/`RLx`.

`ipc_attach_name(controller_id_t)` is the one function that knows this
mapping; nothing else should hardcode a `"c1"`/`"l3"`/`"rl2"` string.

What this does **not** cover yet: which physical Qnet node (VM/hostname)
each of these ten names actually lives on. That depends on the chosen
deployment topology (1, 2, or 3 computers — see
`docs/QNX_DEPLOYMENT_RUN_GUIDE.md`) and hasn't been decided in code yet;
resolving a peer's `/net/<node>/dev/name/local/traffic/<suffix>` path is
a separate, still-open piece of work.

## Known follow-up

`docs/QNX_PROJECT_FILE_OVERVIEW.md` still describes this contract using
`SET_OPERATION_MODE` and `MSG_HEARTBEAT` as if those were different from
the spec's `SET_MODE`/`HEARTBEAT` verbs — that doc predates this header
and hasn't been reconciled with it yet. The header here uses `MSG_`-
prefixed C enum identifiers (`MSG_SET_MODE`, `MSG_HEARTBEAT`) purely as
normal C naming convention; they map 1:1 to the spec's `SET_MODE` /
`HEARTBEAT` verbs, not to a separately-invented name.
