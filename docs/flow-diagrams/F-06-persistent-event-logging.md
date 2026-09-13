# F-06 — Persistent Event Logging: Function-Level Flow

## What this feature does

Central (C1) persists a timestamped, human-readable line for every significant
event it handles or originates — reconnects, faults, watchdog trips, mode
switches, and every operator command — to a plain local file
(`central_log.txt`, append mode) as well as to stdout. It is infrastructure
(Layer 5 — Observability, see `00-ARCHITECTURE-OVERVIEW.md`), not one of the
10 formally-specified use cases in `usecase.md`.

## Entry point

`c_logger_init()` (`app/central/includes/c_logger.h` line 9,
`app/central/src/c_logger.c` lines 14-20), called once from C1's `main()`
(`app/central/src/c_main.c` line 246) right after the startup banner and
before `ipc_attach()` — i.e. before any request/pulse/operator thread could
possibly call `c_logger_log()`.

## Function call chain

- **`c_logger_init()`** (`c_logger.c` lines 14-20) opens `central_log.txt`
  with `fopen(..., "a")` and stores the handle in the module-static
  `g_log_file`. If `fopen()` fails, `g_log_file` stays `NULL` and a one-line
  warning is printed to stdout — every later `c_logger_log()` call then
  silently degrades to stdout-only (`c_logger.h`'s doc comment calls this out
  explicitly; there is no retry).
- **`c_logger_log(fmt, ...)`** (`c_logger.c` lines 22-50) is a plain printf-
  style variadic function with **no internal lock of its own**. It builds a
  `YYYY-MM-DD HH:MM:SS` wall-clock timestamp via `time()`/`localtime()`/
  `strftime()` (lines 24-34), unconditionally `fprintf()`s
  `"[timestamp] fmt\n"` to stdout (lines 36-40), and — only if `g_log_file !=
  NULL` — repeats the same formatted line to the file and `fflush()`s it
  (lines 42-49) so a crash doesn't lose a buffered line. Because there is no
  lock inside the function itself, two threads calling it concurrently could
  interleave their output.
- That's exactly why **every real call site wraps the call in
  `console_io_lock`** — the same mutex that also serialises `c_hmi_render()`'s
  1 Hz status table and `c_operator.c`'s interactive prompts (see
  `c_main.c` line 63's doc comment). Three representative call sites:
  - `c_main.c`'s `log_reconnect_if_needed()` (lines 71-79) locks
    `ctx->console_io_lock`, calls
    `c_logger_log("Controller %d reconnected (PA-08)", (int)sender_id)`,
    unlocks.
  - `c_operator.c`'s `handle_set_mode()` (line 167) calls
    `c_logger_log("Operator: SET_MODE(target=%d, mode=%s) submitted", ...)`
    with no lock of its own — but the *entire* handler already runs under
    `console_io_lock`, acquired by the caller,
    `c_operator_reader_thread()`'s `case 'm'` (lines 479-481), before
    `handle_set_mode()` is ever invoked.
  - `c_comm.c`'s `on_command_reply()` (lines 87-118) explicitly locks/unlocks
    the module-static `g_console_io_lock` (set once via
    `c_comm_set_console_io_lock()`, `c_main.c` line 265) around its three
    `c_logger_log()` variants (`send failed` / `NACK reason=...` /
    `-> RESULT`), since it fires asynchronously on the **client thread**,
    unrelated to whichever thread originally posted the request.

**One gap worth flagging explicitly:** `c_comm.c`'s per-verb "outgoing queue
full" log lines (e.g. `c_comm_send_set_mode()` line 132) take **no lock of
their own** — they rely on already running inside a `console_io_lock`-held
context. That holds for every `c_operator.c` handler (all six wrap their
whole body in the lock, as above). It does **not** hold for
`c_main.c`'s `on_pulse()` DP-01/DP-02 auto peak-hour path: `console_io_lock`
is taken only around the `"Auto peak-hour switch..."` log line (lines
207-211) and released *before* calling `c_comm_broadcast_set_mode()`
(line 212, unlocked). If `ipc_client_post()` ever returned non-zero there,
that one `c_logger_log()` call (`c_comm.c` line 132, reached via the
broadcast loop) would run without `console_io_lock` held — the sole call
site in this codebase where the wrap isn't guaranteed.

## Call site inventory

| File : function | What it logs | Locked at this call site? |
|---|---|---|
| `c_main.c` : `main()` (line 246) | *(not a log call — `c_logger_init()` opens the file)* | n/a |
| `c_main.c` : `log_reconnect_if_needed()` (line 77) | "`Controller %d reconnected (PA-08)`" | Yes — explicit `console_io_lock` (lines 76-78) |
| `c_main.c` : `on_request()`, `MSG_FAULT_REPORT` case (lines 115-117) | fault code/severity/detail from an inbound `FAULT_REPORT` | Yes — explicit `console_io_lock` (lines 114-118) |
| `c_main.c` : `on_pulse()`, `IPC_PULSE_HEARTBEAT_TICK`, PA-07 branch (lines 170-171) | each newly-`UNAVAILABLE` controller (3 missed heartbeats) | Yes — explicit `console_io_lock` (lines 168-173) |
| `c_main.c` : `on_pulse()`, auto peak-hour branch (lines 208-210) | DP-01/DP-02 auto `SET_MODE` broadcast trigger | Yes — explicit `console_io_lock` (lines 207-211), released *before* the actual broadcast call (see gap above) |
| `c_operator.c` : `handle_set_mode` / `handle_timing_profile` / `handle_request_override` / `handle_renew_override` / `handle_cancel_override` / `handle_request_fault_clear` (lines 167, 207-208, 276-279 & 283-284, 328-329, 355, 396) | operator command submitted (or pre-check rejection, `REQUEST_OVERRIDE` only) | Yes, indirectly — each `handle_*()` runs entirely under `console_io_lock`, acquired by `c_operator_reader_thread()`'s switch (lines 479-506) before the handler is called |
| `c_operator.c` : `handle_demo_hour()` / `handle_resume_automatic()` (lines 430-431, 445) | demo peak-hour clock forced to hour N / resumed to real clock | Yes, indirectly — same reader-thread switch (lines 509-516) |
| `c_comm.c` : `on_command_reply()` (lines 105-112) | async outcome of any outgoing command — send failed / `NACK reason=...` / `-> RESULT` | Yes — explicit, module-static `g_console_io_lock` (lines 100-117), since this runs on the client thread |
| `c_comm.c` : `c_comm_send_set_mode` / `c_comm_send_timing_profile` / `c_comm_broadcast_timing_profile` / `c_comm_send_request_override` / `c_comm_send_renew_override` / `c_comm_send_cancel_override` / `c_comm_send_request_fault_clear` (lines 132, 159, 173, 185-186, 206, 222, 238, 254) | "dropped — outgoing queue full" (or, line 173, an invalid arterial chain) | No lock of its own — relies on the caller already holding `console_io_lock`; true everywhere except the one `on_pulse()` broadcast path noted above |

## Cross-node view

Single-node, C1-only. `c_logger_log()` is **not** itself an IPC mechanism and
carries nothing across the Qnet boundary — every event it records (a fault
report, a reconnect, a heartbeat timeout, a command outcome) has already
either arrived over IPC (handled by `on_request()`/`on_pulse()`/
`on_command_reply()`, per `SEQUENCE_DIAGRAMS.md`'s SD-0x flows) or been
generated locally by the operator console. This module's only job is turning
those already-decided events into a durable, timestamped record after the
fact — Lx and RLx have no equivalent of it; they only `printf()` locally
through their own signal/sensor layers.

## System-level summary diagram

```text
c_main.c
  log_reconnect_if_needed()          (PA-08 reconnect)          --+
  on_request()  MSG_FAULT_REPORT     (fault report)              |
  on_pulse()    HEARTBEAT_TICK       (PA-07 newly UNAVAILABLE)    |
  on_pulse()    auto peak-hour       (DP-01/02 mode switch)       |
                                                                   |
c_operator.c                                                      |
  handle_set_mode / handle_timing_profile /                       +--> c_logger_log(fmt, ...)
  handle_request_override / handle_renew_override /                |      |
  handle_cancel_override / handle_request_fault_clear /             |      |  (no lock of its own -
  handle_demo_hour / handle_resume_automatic (operator commands)    |      |   caller must already
                                                                   |      |   hold console_io_lock)
c_comm.c                                                          |      v
  on_command_reply()                 (async ACK/NACK/send-failed) |   stdout   (always)
  c_comm_send_*() / c_comm_broadcast_*() (queue-full drops)      --+   central_log.txt (if fopen() in
                                                                          c_logger_init() succeeded)
```
