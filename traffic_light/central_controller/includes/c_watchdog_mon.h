#ifndef C_WATCHDOG_MON_H
#define C_WATCHDOG_MON_H

#include "c_mode_eng.h"

/* Called once per second from c_main.c's IPC_PULSE_HEARTBEAT_TICK case
 * (the same 1 Hz timer already armed for C1's own housekeeping - no new
 * timer needed). For every one of the 9 controllers, increments
 * missed_heartbeat_ticks; once it reaches 3 (PA-07: three consecutive
 * missed 1 s heartbeats), marks it unavailable and logs it exactly once
 * (edge-triggered via marked_unavailable) via c_logger_log(). Does NOT
 * reset the counter itself - only c_server_record_status()/
 * record_crossing_status() do that, on actual proof of life. */
void c_watchdog_mon_tick(c_mode_eng_t *eng);

#endif /* C_WATCHDOG_MON_H */
