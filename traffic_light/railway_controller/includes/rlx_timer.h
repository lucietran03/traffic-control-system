#ifndef RLX_TIMER_H
#define RLX_TIMER_H

#include <stdint.h>

/*
 * Pure timing-policy constants and a small countdown helper for the
 * railway-crossing FSM (STATE_CHARTS.md SC-04A/SC-04B;
 * system_assumptions_tables.md RC-03/RC-04/RC-05/RC-11). rlx_fsm.c owns
 * every state transition; this file only owns "how long" and "how to
 * count a duration down without underflowing" - it takes no lock, holds
 * no state of its own, and does no I/O. rlx_fsm.c is the only caller.
 */

#define RLX_WARNING_TO_CLOSING_MS 5000u   /* RC-03: warning lead before gates start closing */
#define RLX_CLOSING_DEADLINE_MS   15000u  /* RC-03: 5s lead + 10s gate-closing allowance */
#define RLX_EXPECTED_ARRIVAL_MS   20000u  /* Placeholder "expected arrival" after gate-confirmed
                                            * CLOSED - no exact pre-arrival timing beyond the 45s
                                            * total RC-03 budget is specified anywhere, so this is
                                            * a simplification, not a documented value. */
#define RLX_OCCUPANCY_WINDOW_MS   20000u  /* RC-04: per-direction occupancy window duration */
#define RLX_OPENING_DEADLINE_MS   15000u  /* Placeholder, same reasoning as RLX_CLOSING_DEADLINE_MS */
#define RLX_WARNING_DIAGNOSTIC_TIMEOUT_MS 60000u  /* RESERVED, not currently referenced: RC-11's
                                                    * "stuck active" fault needs a continuously-
                                                    * asserted sensor line to detect, which the
                                                    * current discrete simulated TRAIN_APPROACHING
                                                    * event cannot represent - see the comment in
                                                    * rlx_fsm.c's rlx_fsm_on_tick() RLX_WARNING case.
                                                    * Kept as a placeholder value for rlx_sensor.c to
                                                    * use once it exists. */

/*
 * Decrements *remaining_ms by tick_ms, clamped at 0 - never underflows
 * even if tick_ms > *remaining_ms. Returns 1 if the window is now at or
 * below 0 (expired this call, or was already 0), 0 if time remains.
 */
uint8_t rlx_timer_tick_window(uint32_t *remaining_ms, uint32_t tick_ms);

#endif /* RLX_TIMER_H */
