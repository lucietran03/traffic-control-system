# F-04 — PA-10 Local Watchdog Self-Detection: Function-Level Flow

## What this feature does

`system_assumptions_tables.md`'s PA-10 row states: "A controller or
supervised-process fault affects only the owning location, whose watchdog
forces the documented safe outputs" — because "one local failure must not
cascade through the distributed network." This is not one of the 10
formally-specified use cases in `usecase.md`; it is Layer 4 (Resilience)
infrastructure present in every `lx_main`/`rlx_main` process, implementing
the "controller ... fault" half of PA-10 (the other half, Central-link loss,
is `UC-10`'s subject). Concretely: a dedicated **watchdog thread** in every
Lx and RLx process polls a tick counter the **server thread** bumps on every
timer pulse, and if that counter stops advancing for a few seconds, the
watchdog thread — not the (possibly hung) server thread — forces the fault
into the FSM itself.

## Entry point

- **Lx**: `lx_watchdog_thread()` in `app/intersection/src/lx_watchdog.c`
  (function body lines 22-42), an infinite `for (;;) { sleep(...); ... }`
  loop.
- **RLx**: `rlx_watchdog_thread()` in `app/railway/src/rlx_watchdog.c`
  (function body lines 15-32), the same `for (;;) { sleep(...); ... }`
  shape.

Both are spawned as a **fourth thread** per process (alongside server,
client, and sensor threads), each fed a small, decoupled args struct rather
than the process's full context struct:

- Lx: `lx_watchdog_args_t` (`lx_watchdog.h` lines 12-15) holds `fsm` and
  `phase_tick_counter`; `app/intersection/src/lx_main.c` populates
  `watchdog_args` (declared around line 128, `phase_tick_counter` pointer
  assigned around line 173) and calls
  `pthread_create(&watchdog_tid, NULL, lx_watchdog_thread, &watchdog_args)`
  around line 175.
- RLx: `rlx_watchdog_args_t` (`rlx_watchdog.h` lines 21-24) holds `fsm` and
  `tick_counter`; `app/railway/src/rlx_main.c` populates `watchdog_args`
  (declared around line 102, `tick_counter` pointer assigned around line
  141) and calls
  `pthread_create(&watchdog_tid, NULL, rlx_watchdog_thread, &watchdog_args)`
  around line 142.

## Function call chain

### Lx (intersection)

1. **Server thread — tick increment.** `app/intersection/src/lx_main.c`
   arms a 100 ms repeating pulse, `IPC_PULSE_PHASE_TIMER` (around line 185,
   same timer UC-01 rides). `on_pulse()`'s `case IPC_PULSE_PHASE_TIMER:`
   (around line 85) calls `lx_fsm_on_phase_timer(&ctx->fsm)` and then, right
   after, does `ctx->phase_tick_counter++` (around line 92). This is a
   **plain, unprotected read-modify-write** on a `volatile uint32_t`
   (declared around line 34) — no mutex guards the increment; `volatile`
   only prevents the compiler from caching the value in a register across
   loop iterations, it gives no atomicity guarantee. The comment at line
   29-34 documents the field as "incremented by `on_pulse()` every
   `IPC_PULSE_PHASE_TIMER` tick; polled by `lx_watchdog.c`."

2. **Watchdog thread — periodic check.** `lx_watchdog_thread()` sleeps
   `LX_WATCHDOG_CHECK_INTERVAL_S` (2 s, `lx_watchdog.c` line 20 — "2 s of
   total silence (20 missed ticks) is unambiguous, not a false positive
   from ordinary scheduling jitter"), then does
   `current = *args->phase_tick_counter` (line 31) — again a **plain,
   unlocked read**, the same lock-free contract as the increment side; the
   two threads communicate purely through ordinary memory visibility on
   a `volatile` word, not through `fsm->lock` or any IPC primitive.

3. **Watchdog thread — stall decision.** If `current == last_seen` (line
   32), no tick landed in the last 2 s: it logs to stderr (line 33-34) and
   calls `lx_fsm_report_watchdog_trip(args->fsm)` (line 35). Either way,
   `last_seen = current` (line 37) and the loop repeats — the thread never
   exits, so a stall that later resolves (e.g. after `FAULT_SAFE` is
   cleared) resumes being monitored normally.

4. **Watchdog thread, inside `lx_fsm_report_watchdog_trip()`**
   (`app/intersection/src/lx_fsm.c` lines 473-485) — runs **entirely on the
   watchdog thread**, under its own `pthread_mutex_lock(&fsm->lock)` (line
   475), independent of whether the server thread is still alive. It sets
   `fsm->faults |= FAULT_WATCHDOG_TRIP` (line 476), and — per the
   "Compliance-audit fix" doc comment directly above it (lines 455-472) —
   also forces the supervisory transition itself: if
   `fsm->supervisory != SUPERVISORY_FAULT_SAFE` (line 477), it terminates
   any active override first (`lx_fsm_terminate_override_locked()`, line
   479, only if `SUPERVISORY_CENTRAL_OVERRIDE`), sets
   `fsm->supervisory = SUPERVISORY_FAULT_SAFE` (line 481), and calls
   `lx_signal_apply_fault_safe(fsm->self_id)` (line 482) directly — which
   `printf`s `"Lx %d: entering FAULT_SAFE mode - holding safe outputs
   (all-red/dark)\n"` (`lx_signal.c` lines 45-48). The doc comment explains
   *why* this was changed from a lazier design: originally the function
   only OR'd the fault bit and relied on `lx_fsm_check_fault_locked()` —
   called at the top of every `lx_fsm_on_*()`/`lx_fsm_on_phase_timer()` —
   to notice it later; but "if the server thread that runs those functions
   is the thing that's actually hung, nothing would ever call
   `check_fault_locked()` again," which "would defeat the watchdog at the
   exact moment it exists to catch." So both Lx and RLx now force the
   transition **synchronously, from the watchdog thread itself**.

### RLx (railway)

1. **Server thread — tick increment.** `app/railway/src/rlx_main.c` arms a
   1000 ms repeating pulse reusing `IPC_PULSE_RAILWAY_WARNING` for the
   FSM's single recurring tick (around line 156). `on_pulse()`'s
   `case IPC_PULSE_RAILWAY_WARNING:` (around line 51) calls
   `rlx_fsm_on_tick(&ctx->fsm)` and then `ctx->tick_counter++` (line 58) —
   again a **plain, unprotected increment** on `volatile uint32_t
   tick_counter` (declared line 28, comment: "bumped every FSM tick;
   watched by `rlx_watchdog_thread()`").

2. **Watchdog thread — periodic check.** `rlx_watchdog_thread()`
   (`rlx_watchdog.c` lines 15-32) sleeps `RLX_WATCHDOG_CHECK_INTERVAL_S`
   (3 s, line 13 — "the tick fires every 1000 ms, so 3 s of total silence
   (3 missed ticks) is unambiguous"), reads `current = *args->tick_counter`
   (line 23) — again a plain unlocked read, mirroring the Lx side exactly.

3. **Watchdog thread — stall decision.** If `current == last_seen` (line
   24): logs to stderr (line 25-26), calls
   `rlx_fsm_report_watchdog_trip(args->fsm)` (line 27). Loop continues
   forever, same non-exiting pattern as Lx.

4. **Watchdog thread, inside `rlx_fsm_report_watchdog_trip()`**
   (`app/railway/src/rlx_fsm.c` lines 476-483) — takes `fsm->lock` (line
   478), and if `fsm->state != RLX_FAULT` (line 479, idempotence guard),
   calls `enter_fault(fsm, FAULT_WATCHDOG_TRIP)` (line 480) — **directly**,
   with no intermediate flag-and-defer step. `enter_fault()` (static,
   lines 106-121) is the same helper used by every other RLx fault path
   (`FAULT_GATE_CONFIRM_MISSING` etc. at lines 140/147/158): it
   unconditionally calls `rlx_gate_command_close()` (line 115 — "safe to
   call unconditionally: it just (re)starts a close motion, which is a
   no-op in outcome if gates are already closed/closing"), calls
   `rlx_signal_show_fault(fault_bit)` (line 116), sets
   `fsm->state = RLX_FAULT` (line 117), resets `state_elapsed_ms = 0`
   (line 118), OR's the fault bit into `fsm->faults` (line 119), and sets
   `fsm->fault_report_pending = 1` (line 120) so the next server-thread
   tick sends `MSG_FAULT_REPORT` (see Cross-node view).

### Why the two differ

Both implementations now act **synchronously from the watchdog thread**,
not lazily — that part is identical, and is itself a documented compliance
fix (see the "Compliance-audit fix" comment in `lx_fsm.c` lines 455-472).
The remaining, *intentional* difference is in **what "forcing the fault"
actually does to the physical world**:

- **Lx** sets `fsm->supervisory = SUPERVISORY_FAULT_SAFE` and prints a
  static "holding safe outputs" message. It does not command any new
  physical motion — a road signal's safe state (all-red/dark) requires no
  moving part to reach; it is simply "stop actuating," which is exactly
  what `SUPERVISORY_FAULT_SAFE` achieves once
  `lx_fsm_check_fault_locked()` (checked at the top of every subsequent
  `lx_fsm_on_*()` call, `lx_fsm.c` line 60-68) short-circuits normal phase
  logic. A hung server thread producing *no* further output is, for an
  intersection, already indistinguishable from the safe state — dark/
  all-red is a "do nothing more" outcome.
- **RLx**, via `enter_fault()`, immediately calls
  `rlx_gate_command_close()` — an active, physical, safety-critical
  actuation command — *and* transitions `fsm->state` to `RLX_FAULT`
  outright, not just an overlay flag like Lx's `supervisory` field. This
  is because a level-crossing gate that is UP or mid-motion when the
  controlling process hangs is unprotected against an approaching train;
  RC-06's occupancy invariant ("gates must be confirmed closed before
  granting PROCEED") and PA-10's requirement that the watchdog "forces the
  documented safe outputs" both demand the *physical* actuator be driven
  to its safe position immediately, not merely have future decision-making
  suppressed. Road traffic signals fail safe by going dark; railway gates
  do not fail safe by being left wherever they were — they must be
  actively driven down. This is exactly the asymmetry the RC-06/PA-10 pair
  encodes, and why `rlx_fsm_report_watchdog_trip()` is the more aggressive
  of the two: it doesn't just flag a fault for the next event to notice,
  it *is* the safe-state application.

## Cross-node view

**This detection is entirely single-node/local by design.** No message
crosses the wire for the detection step itself — the tick-counter write,
the watchdog thread's read, and the fault-forcing call are all in-process,
matching PA-10's own wording: "a controller or supervised-process fault
affects only the owning location." Neither `lx_watchdog.c` nor
`rlx_watchdog.c` imports `qnet_utils.h`, calls `ipc_client_post()`, or
touches a channel — grep confirms both files' includes are limited to
`<stdio.h>`, `<unistd.h>`, and their own node's watchdog/fsm headers.

The *consequence* of a trip does eventually surface outward, but through
each node's **pre-existing, unrelated reporting path** — not through any
watchdog-specific wire verb:

- **RLx**: `enter_fault()` sets `fsm->fault_report_pending = 1`
  (`rlx_fsm.c` line 120). The next `IPC_PULSE_RAILWAY_WARNING` tick's
  `on_pulse()` (`rlx_main.c` around line 59) calls
  `rlx_fsm_take_fault_report_pending()` (clears the flag) and, if it was
  set, `rlx_comm_send_fault_report()` (`rlx_comm.c` lines 74-98), which
  builds a `MSG_FAULT_REPORT` envelope carrying `tmp_status.faults` as
  `fault_code` (line 86) and posts it to C1 via `ipc_client_post()`.
- **Lx**: there is no separate fault-report verb at all — `fsm->faults`
  simply rides inside `status_report_payload_t` (shared shape, used for
  both periodic STATUS and HEARTBEAT bodies — `ipc_msg.h` lines 115-141),
  filled by `lx_fsm_fill_status()` (`lx_fsm.c` line 1196, `faults` field
  set at line 1214). The next `IPC_PULSE_HEARTBEAT_TICK` (armed 1000 ms,
  `lx_main.c` line 190) calls `lx_comm_send_heartbeat()`
  (`lx_comm.c` lines 35-65), which calls `lx_fsm_fill_status(fsm,
  &req.payload.heartbeat.summary)` (line 51) and sends `MSG_HEARTBEAT` —
  so `FAULT_WATCHDOG_TRIP` simply shows up as one more bit in the next
  ordinary heartbeat's fault mask, with no dedicated verb needed.

One subtlety worth naming: both of these outward-notification paths are
themselves driven by `on_pulse()` on the **server thread** — the very
thread whose stall is what the watchdog exists to catch. If the server
thread is merely failing to advance the tick counter (e.g. stuck in a long
computation or a lock it will eventually release) the fault still reaches
C1 on the next pulse it manages to process. But a truly wedged server
thread (permanently blocked) would prevent the outward `MSG_FAULT_REPORT`/
`MSG_HEARTBEAT` from ever being sent, even though the local safe-state
actuation (Lx's `SUPERVISORY_FAULT_SAFE`, RLx's `rlx_gate_command_close()`)
has already been forced by the watchdog thread itself, independent of the
server thread. This matches PA-10's framing precisely: the *local* safe
outputs are guaranteed regardless of the server thread's health; only the
*remote notification* rides on the machinery PA-10 does not claim to fix
(that is C1's own missed-heartbeat detection in `c_watchdog_mon.c`, a
different, pre-existing mechanism, watching from the other end).

## System-level summary diagram

```mermaid
---
title: F-04 PA-10 local watchdog trip (single node, generic Lx or RLx)
---
sequenceDiagram
    accTitle: PA-10 local watchdog self-detection, function call trace
    accDescr: The server thread bumps a plain volatile tick counter on every timer pulse; a separate watchdog thread polls it every few seconds with no lock; on a stall it forces the fault into the FSM directly, then the existing outward-reporting path (heartbeat for Lx, fault report for RLx) carries it to Central on its next opportunity.
    autonumber

    participant Timer as Timer pulse<br/>(100ms Lx / 1000ms RLx)
    participant ServerT as Server thread<br/>on_pulse()
    participant Counter as tick_counter<br/>(volatile uint32_t, no lock)
    participant WdT as Watchdog thread<br/>lx/rlx_watchdog_thread()
    participant FSM as fsm-&gt;lock<br/>(FSM struct)
    participant Actuator as Signal / Gate output

    loop every tick
        Timer->>ServerT: IPC_PULSE_PHASE_TIMER / _RAILWAY_WARNING
        ServerT->>Counter: counter++  (plain increment, no mutex)
    end

    loop every 2s (Lx) / 3s (RLx)
        WdT->>Counter: current = *counter  (plain read, no mutex)
        alt current == last_seen (stalled)
            WdT->>FSM: lx/rlx_fsm_report_watchdog_trip(fsm) - takes fsm->lock itself
            alt Lx
                FSM->>FSM: faults |= FAULT_WATCHDOG_TRIP; supervisory = FAULT_SAFE
                FSM->>Actuator: lx_signal_apply_fault_safe() - stop actuating (dark/all-red)
            else RLx
                FSM->>FSM: enter_fault(): state = RLX_FAULT; fault_report_pending = 1
                FSM->>Actuator: rlx_gate_command_close() - actively drive gates DOWN
            end
        end
        WdT->>Counter: last_seen = current
    end

    Note over ServerT,Actuator: Outward report piggybacks on the NEXT server-thread<br/>pulse the server thread manages to run: Lx -> next<br/>HEARTBEAT (fault bits in status_report_payload_t);<br/>RLx -> next tick's MSG_FAULT_REPORT. Local safe<br/>output above does NOT wait for this.
```

Zoomed all the way out: **timer pulse -> server thread increments a
lock-free `volatile` counter -> watchdog thread polls it on its own
schedule, no shared lock beyond the plain word itself -> on stall, the
watchdog thread forces the fault into the FSM synchronously, under
`fsm->lock`, independent of the server thread's health**. The severity
split is the whole point of this feature: Lx merely stops actuating
(`SUPERVISORY_FAULT_SAFE`), because road-signal safety is achieved by
inaction; RLx actively drives the gates down (`enter_fault()` ->
`rlx_gate_command_close()`), because railway-crossing safety requires a
positive physical action that inaction alone cannot provide.
