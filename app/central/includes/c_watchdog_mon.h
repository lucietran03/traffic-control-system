#ifndef C_WATCHDOG_MON_H
#define C_WATCHDOG_MON_H

#include "c_mode_eng.h"

/* Called once per second from c_main.c's IPC_PULSE_HEARTBEAT_TICK case
 * (the same 1 Hz timer already armed for C1's own housekeeping - no new
 * timer needed). For every one of the 9 controllers, increments
 * missed_heartbeat_ticks; once it reaches 3 (PA-07: three consecutive
 * missed 1 s heartbeats), marks it unavailable (edge-triggered via
 * marked_unavailable) and appends its id to out_newly_unavailable.
 * Does NOT reset the counter itself - only c_server_record_status()/
 * record_crossing_status() do that, on actual proof of life.
 *
 * Deliberately does NOT call c_logger_log() itself: this function runs
 * under c_main.c's mode_eng_lock, and c_logger_log() needs console_io_lock
 * (see c_main.c's central_context_t doc comment) - taking console_io_lock
 * while mode_eng_lock is already held would be the exact reverse of the
 * lock order used everywhere else in this codebase (console_io_lock is
 * always the OUTER lock), risking an AB-BA deadlock against the operator
 * thread. The caller logs each returned id itself, after releasing
 * mode_eng_lock, under console_io_lock only - same pattern already used
 * for the peak-hour auto-switch broadcast in on_pulse().
 *
 * out_newly_unavailable must have room for at least 9 entries (one per
 * controller). Returns the number of entries written (0-9). */
int c_watchdog_mon_tick(c_mode_eng_t *eng, controller_id_t *out_newly_unavailable);

#endif /* C_WATCHDOG_MON_H */
