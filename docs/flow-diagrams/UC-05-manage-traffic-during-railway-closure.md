# UC-05 — Manage Road Traffic During and After a Railway Closure: Function-Level Flow

## What this feature does

`usecase.md` §3.2.5 (UC-05) requires that once an adjacent crossing reports a
state other than `OPEN`, the intersection "withholds green from the
toward-crossing movement while the crossing remains unavailable" (Main Flow
steps 1-4, business rule `CC-02`), while a binary advance/queue detector only
ever reports whether the queue has reached its fixed detection point
(`CC-01`). After the crossing reopens, if that detector still reports
`QUEUE_WARNING`, the controller runs a bounded connector drain phase in 4 s
increments up to a 60 s cap (Main Flow steps 6-10, `CC-03`), and every
transition in and out of this behaviour must still pass through the ordinary
yellow/all-red clearance (`TL-01`). This document traces that entire
behaviour as it is actually implemented on the `Lx` (intersection
controller) side, in `app/intersection/src/lx_fsm.c`.

## Entry point

The trigger is not a local sensor or keypress like most other UCs — it is an
inbound cross-node IPC message, `MSG_CROSSING_STATUS`, sent by the adjacent
railway controller (`RLx`, see UC-04) and delivered to `Lx`'s single QNX
server thread.

`app/intersection/src/lx_main.c`, `on_request()`, around line 59:

```c
case MSG_CROSSING_STATUS:
    /* Lx only ever reads this to feed the railway pre-emption
     * overlay (CC-02); it never replies with a command of its own
     * (RC-02: no Lx may command railway equipment). */
    lx_fsm_on_crossing_status(&ctx->fsm, &req->payload.crossing_status, reply);
    break;
```

This runs inside `ipc_server_run()`'s request callback, on the one thread in
the process that is allowed to call `MsgReceive()`/`MsgReply()`
(`lx_main.c` around line 198-201). Every other verb in this same `switch`
(`MSG_SET_MODE`, `MSG_REQUEST_OVERRIDE`, etc.) and the `IPC_PULSE_PHASE_TIMER`
pulse handled by `on_pulse()` (around line 80-93) also run on this same
server thread, so `lx_fsm_on_crossing_status()` and `lx_fsm_on_phase_timer()`
are never executing concurrently with each other — they are serialized by
QNX's single-channel receive loop, not just by `fsm->lock`. The lock still
matters because `lx_sensor.c`'s reader thread, `lx_watchdog.c`'s watchdog
thread, and `lx_comm.c`'s heartbeat-reply callback (client thread) all touch
`lx_fsm_t` from outside the server thread.

## Function call chain

### (a) Suppression moment — crossing goes non-`OPEN`

**Step 1 — `lx_fsm_on_crossing_status()`** (`lx_fsm.c` around line 849-909),
server thread, `fsm->lock` held for the whole function:

```c
fsm->last_crossing_state = (crossing_state_t)payload->state;   // ~line 859, unconditional
...
if (fsm->supervisory != SUPERVISORY_FAULT_SAFE) {
    crossing_state_t state = (crossing_state_t)payload->state;
    if (state != CROSSING_OPEN) {
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
            lx_fsm_terminate_override_locked(fsm);   // CC-02: evict any active override first
        }
        fsm->supervisory = SUPERVISORY_RAILWAY_PREEMPTION;   // ~line 873
        ...
```

`last_crossing_state` is recorded even while `FAULT_SAFE`, so a later
`lx_fsm_on_request_fault_clear()` (around line 1291-1319) knows to resume
into `SUPERVISORY_RAILWAY_PREEMPTION` rather than `NORMAL_OPERATION` if the
crossing is still closed — but that path is a fault-recovery corner case,
not part of the main UC-05 trace. This handler itself does **not** touch
`fsm->phase` or call `lx_signal_*` directly; it only flips the supervisory
flag and always replies `RESULT_ACK` (line ~906, RC-02: Lx never rejects a
crossing report). The actual suppression happens later, split across two
mutually-exclusive branches depending on what `fsm->phase` already is at
the moment `SUPERVISORY_RAILWAY_PREEMPTION` becomes active:

**Branch A — a connector green is already running (the fixed bug).**
The next `IPC_PULSE_PHASE_TIMER` pulse (100 ms cadence, armed in
`lx_main.c` around line 185) drives **`lx_fsm_on_phase_timer()`**
(`lx_fsm.c` around line 931), which reaches the `PHASE_CONNECTOR_GREEN` case
(around line 1078) and hits the early-cutoff check at line 1090:

```c
if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION && fsm->green_elapsed_ms >= LX_MIN_GREEN_MS) {
    /* CC-01/CC-02/UC-05 step 2/SD-05 fix: cut this already-running
     * toward-crossing green to its MINIMUM-green requirement ... */
    fsm->drain_active = 0;
    fsm->drain_extending = 0;
    fsm->drain_extension_total_ms = 0;
    lx_fsm_advance_phase_locked(fsm);
    break;
}
```

This is the safety fix called out by commit `850ecb5` ("Demo-readiness UX +
spec-compliance audit fixes + real DEGRADED_LOCAL/resync"): before it
existed, nothing in the file *truncated* a connector green that was already
on-air when the crossing closed — the pre-existing checks only ever stopped
a **new** connector green from starting (Branch B below). An already-running
green could otherwise run to its full `LX_PEAK_CONNECTOR_GREEN_MS` (30 s) or
extended `LX_MAX_GREEN_MS`/drain duration (40 s+) before the ordinary
end-of-phase logic ever consulted `supervisory` again. The fix re-checks
`SUPERVISORY_RAILWAY_PREEMPTION` on **every** 100 ms tick (not gated to a 4 s
modulo like the ordinary demand re-checks), so it fires on the exact tick
`green_elapsed_ms` first reaches `LX_MIN_GREEN_MS` (8000 ms,
`lx_timer.h` line 31) — satisfying UC-05 step 2's "completes its
minimum-green requirement" without ever showing extra toward-crossing green
after that floor. It also defensively zeroes any in-flight `drain_*` state,
since a drain episode legitimately cannot coexist with a fresh pre-emption.
`lx_fsm_advance_phase_locked()` is then called to move to
`PHASE_CONNECTOR_YELLOW`, which runs the ordinary
yellow → all-red clearance before red-holding (`TL-01`/`CC-02`'s "may not
bypass yellow or all-red clearance").

**Branch B — no connector green is running yet (ordinary case).**
When the all-red boundary between arterial and connector is next reached in
the normal cycle, **`lx_fsm_advance_phase_locked()`**
(`lx_fsm.c` around line 228), `PHASE_ALL_RED_A_TO_B` case, around line 263:

```c
if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) {
    /* CC-02/NU-05: a railway crossing only ever sits on a connector
     * road, so suppressing "toward-crossing" means skipping
     * CONNECTOR_GREEN and going straight back to PHASE_ARTERIAL_GREEN ... */
    fsm->phase = PHASE_ARTERIAL_GREEN;
    break;
}
```

This is the boundary guard that prevents a **new** connector green from ever
starting while pre-emption is active — arterial (cross-traffic,
away-from-crossing) keeps cycling normally the whole time, matching
`CC-02`'s "compatible away-from-crossing and cross-traffic movements may be
prioritised" and SD-05's "continue non-conflicting movements". This branch
and Branch A are complementary, not redundant: Branch A handles the case
where pre-emption starts *mid-green*; Branch B handles every subsequent
cycle for as long as pre-emption remains active (both are called from the
same server thread, `fsm->lock` held throughout).

Either way, the caller of `lx_fsm_advance_phase_locked()` always ends with
`lx_signal_show_phase(fsm->self_id, fsm->phase)` (line ~347), which in
`app/intersection/src/lx_signal.c` (line 40-43) is the only place phase
output is actually emitted:

```c
void lx_signal_show_phase(controller_id_t id, signal_phase_t phase)
{
    printf("Lx %d: signal phase now %s\n", (int)id, lx_signal_phase_name(phase));
}
```

There is no separate "hold connector red" API in `lx_signal.c` — the
red-hold on the toward-crossing approach is entirely implicit in the fact
that `fsm->phase` is simply never set to `PHASE_CONNECTOR_GREEN` while
`SUPERVISORY_RAILWAY_PREEMPTION` is active; whatever phase name is printed
(`ARTERIAL GREEN`, `ALL RED (...)`, etc.) never includes `CONNECTOR GREEN`
for the duration of the closure.

### (b) Recovery moment — crossing reports `OPEN` again

**Step 1 — `lx_fsm_on_crossing_status()`** again, same function, same
server thread and lock, but now the `else if` branch at line 879 fires
because `fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION`:

```c
} else if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) {
    fsm->supervisory = SUPERVISORY_NORMAL_OPERATION;         // ~line 883
    if (fsm->queue_warning_active) {
        fsm->drain_pending = 1;                              // ~line 898, CC-03/UC-05 steps 6-7
    }
}
```

`queue_warning_active` is the binary detector flag set/cleared elsewhere by
`lx_fsm_set_queue_warning()` (`lx_fsm.c` line 448-453, called from
`lx_sensor.c` on the sensor-reader thread, under `fsm->lock`). `drain_pending`
is armed **only** at this one `RAILWAY_PREEMPTION → NORMAL_OPERATION` edge —
never re-armed by a later queue-warning change during ordinary operation —
which is what keeps the drain a once-per-closure-episode event (UC-05 alt
flow 6.1, "no queue warning → skip the extended drain phase", falls out for
free here since `drain_pending` is simply never set).

**Step 2 — consuming `drain_pending`.** The very next time
`lx_fsm_advance_phase_locked()` reaches `PHASE_ALL_RED_A_TO_B` (line 237)
and falls through past the now-inactive `RAILWAY_PREEMPTION` check, it hits
the drain-arming block at line 288:

```c
if (fsm->drain_pending) {
    fsm->drain_pending = 0;
    fsm->drain_active = 1;
    fsm->drain_extending = 0;
    fsm->drain_extension_total_ms = 0;
}
fsm->phase = PHASE_CONNECTOR_GREEN;
```

This guarantees the drain applies to exactly the one connector green that
immediately follows reopening.

**Step 3 — ordinary green runs, then hands off to drain extension.** Back in
`lx_fsm_on_phase_timer()`'s `PHASE_CONNECTOR_GREEN` case, the override check
(line 1079) is false and the (now-dormant) early-cutoff check at line 1090
is false (`supervisory` is `NORMAL_OPERATION` again), so control falls to
the drain-state block at line 1134:

```c
if (fsm->drain_active && fsm->drain_extending) {
    ...
}
```

`drain_extending` is still 0 at this point, so this block is skipped and the
*ordinary* per-mode duration/demand check runs (`PEAK_FIXED` block at line
1152, or `OFF_PEAK_SENSOR` block at line 1166). When that ordinary exit
condition is finally satisfied, instead of calling
`lx_fsm_advance_phase_locked()` immediately, each branch checks
`fsm->drain_active` first:

```c
if (fsm->drain_active) {
    fsm->drain_extending = 1;          // ~line 1160 (PEAK_FIXED) / ~line 1177 (OFF_PEAK_SENSOR)
    fsm->drain_extension_total_ms = 0;
} else {
    lx_fsm_advance_phase_locked(fsm);
}
```

This is the moment the phase would ordinarily have transitioned to yellow,
but because this is the designated drain phase, it is held open instead and
`drain_extending` latches true.

**Step 4 — 4 s extension loop with a 60 s cap.** On every subsequent tick,
control now goes straight to the `drain_active && drain_extending` block
(line 1134) and skips the ordinary per-mode check entirely (the two are
kept mutually exclusive precisely so drain ticks at
`drain_extension_total_ms`'s own cadence are never missed by the
differently-timed `% LX_EXTENSION_MS` off-peak recheck):

```c
if ((fsm->drain_extension_total_ms % LX_EXTENSION_MS) == 0) {          // every 4000 ms
    if (!fsm->queue_warning_active || fsm->drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS) {
        fsm->drain_active = 0;
        fsm->drain_extending = 0;
        fsm->drain_extension_total_ms = 0;
        lx_fsm_advance_phase_locked(fsm);      // UC-05 steps 9/9.1
        break;
    }
}
fsm->drain_extension_total_ms += LX_PHASE_TICK_MS;   // 100 ms per tick
```

`LX_EXTENSION_MS` is 4000 ms and `LX_DRAIN_MAX_EXTENSION_MS` is 60000 ms
(`lx_timer.h` lines 33 and 84). Every 4 s boundary, the detector's current
`queue_warning_active` value is re-read: if it has cleared (UC-05 step 9) or
the 60 s cap has been reached (alt flow 9.1), the drain state is reset and
`lx_fsm_advance_phase_locked()` runs the ordinary
`PHASE_CONNECTOR_YELLOW → PHASE_ALL_RED_B_TO_A → PHASE_ARTERIAL_GREEN`
sequence — the required clearance intervals are never skipped by the drain
(`CC-03`'s note that extension is still bounded by `TL-01`). Otherwise the
loop grants one more 4 s increment and keeps ticking.

## Cross-node view

This entire trace is `Lx`'s reaction to a message it did not originate. The
sender is the railway controller `RLx`, whose own state machine and
protection sequence are documented separately in UC-04's flow diagram; only
the send side relevant to this hand-off is noted here for continuity.

`app/railway/src/rlx_comm.c`, `send_crossing_status()` (around line 126-144):

```c
req.verb = MSG_CROSSING_STATUS;
req.sender_id = (uint32_t)self_id;      // e.g. CTRL_RL1
req.target_id = (uint32_t)target_id;    // e.g. CTRL_L1 or CTRL_L2
req.payload.crossing_status.state = (uint32_t)state;   // a crossing_state_t: OPEN/WARNING/CLOSED/FAULT
ipc_client_post(client_queue, target_id, &req, on_reply_log_failure, NULL);
```

driven from `rlx_comm_broadcast_crossing_status_if_changed()`, which is
called from `RLx`'s own FSM whenever its crossing state actually changes,
and fans the message out to both adjacent `Lx` controllers per the fixed
`ADJACENCY` table (`RC1` ↔ `L1`/`L2`, `RC2` ↔ `L3`/`L4`, `RC3` ↔ `L5`/`L6`,
`rlx_comm.c` line 109-113). This is a one-way, fire-and-forget
`ipc_client_post()` from `RLx`'s client thread; `Lx` always `ACK`s
(`ipc_msg.h` line 73: `MSG_CROSSING_STATUS, /* RLx -> Lx (RC-02) */`) and
never sends a command back, since `RC-02` reserves railway-equipment
actuation exclusively to the owning `RLx`.

`SEQUENCE_DIAGRAMS.md` §4.2.5, **SD-05 — "Suppress and Drain Road Traffic
Around a Railway Closure"**, is the sequence diagram that documents exactly
this interaction end-to-end (`RLx->>Lx: CROSSING_STATUS(not OPEN)` through
the drain loop and `Lx->>Sig: resume previous normal phase family`); this
document is the function-level expansion of that same diagram's `Lx`-side
steps. UC-04's flow diagram is the correct place to look for how `RLx`
itself decides *when* to send `WARNING`/`CLOSED`/`OPEN`/`FAULT` (SD-04 in
the same file, §4.2.4).

## System-level summary diagram

```
 RLx crossing state:   OPEN ──▶ WARNING/CLOSED ─────────────────────▶ OPEN
                         |            |  MSG_CROSSING_STATUS              |  MSG_CROSSING_STATUS
                         |            ▼ (not OPEN)                        ▼ (OPEN)
 Lx supervisory state: NORMAL ──▶ RAILWAY_PREEMPTION ───────────────▶ NORMAL_OPERATION (+ drain_pending?)
                         |            |                                   |
 Lx phase sequence:  ...CONN_GREEN?   |                              ALL_RED_A_TO_B
                         |            ├─ Branch A (mid-green):            |  drain_pending consumed
                         |            |  green_elapsed_ms>=LX_MIN_GREEN_MS|
                         |            |  -> cut to min-green, advance     ▼
                         |            |     (lx_fsm_on_phase_timer)  CONNECTOR_GREEN  (drain_active=1)
                         |            |                                   |
                         |            └─ Branch B (boundary):        ordinary duration reached
                         |               ALL_RED_A_TO_B skips              |  (drain_active) -> drain_extending=1
                         |               CONNECTOR_GREEN, goes         ┌───┴────────────────────────┐
                         |               straight to ARTERIAL_GREEN    │  every 4 s: QUEUE_WARNING?  │
                         |               (lx_fsm_advance_phase_locked) │   active & <60s -> +4s      │
                         |                                             │   clear | >=60s -> exit     │
                         ▼                                             └───┬────────────────────────┘
                    arterial keeps cycling normally the whole                  ▼
                    time; connector approach stays red by                CONNECTOR_YELLOW -> ALL_RED_B_TO_A
                    omission (no PHASE_CONNECTOR_GREEN entry)             -> ARTERIAL_GREEN (ordinary cycle resumes)
```

Key takeaway for the timeline: the crossing status feed
(`MSG_CROSSING_STATUS`) only ever *sets flags* (`supervisory`,
`last_crossing_state`, `drain_pending`) inside
`lx_fsm_on_crossing_status()`; every visible signal change is actually
produced later, on the 100 ms `IPC_PULSE_PHASE_TIMER` tick, by
`lx_fsm_on_phase_timer()` and `lx_fsm_advance_phase_locked()` reading those
flags — which is why the suppression logic needed two separate checks
(Branch A for a green already in flight, Branch B for the boundary guard)
rather than one, and why the drain logic needed a `_pending` → `_active` →
`_extending` handoff spanning two different functions and one full
phase-boundary crossing before the first 4 s extension check could ever run.
