#ifndef LX_TIMER_H
#define LX_TIMER_H

#include <stdint.h>

#include "sys_types.h"

/*
 * Pure timing-policy constants and functions for the intersection phase
 * sequencer (STATE_CHARTS.md SC-01B/SC-01C; system_assumptions_tables.md
 * TL-01/TL-02/TL-03/DP-04/DP-06/PA-11).
 *
 * Deliberately side-effect-free: no mutex, no struct mutation, no I/O -
 * every function here takes plain values in and returns a plain value
 * out, so the timing POLICY (how long, and under what demand conditions
 * a phase should end) can be read, reasoned about, and unit-tested
 * independently of lx_fsm.c's state-machine orchestration (which phase
 * comes next, how supervisory authority overlays on top of it). lx_fsm.c
 * is the only caller of this file.
 */

/* TL-02: PEAK_FIXED phase durations - fixed regardless of demand. */
#define LX_PEAK_ARTERIAL_GREEN_MS   48000u
#define LX_PEAK_CONNECTOR_GREEN_MS  30000u

/* Common to both modes. */
#define LX_YELLOW_MS                4000u
#define LX_ALL_RED_MS               2000u

/* TL-01/TL-03/DP-04: OFF_PEAK_SENSOR actuated-green bounds. */
#define LX_MIN_GREEN_MS             8000u
#define LX_MAX_GREEN_MS             40000u
#define LX_EXTENSION_MS             4000u

/* PA-11: override duration cap (5 minutes). */
#define LX_OVERRIDE_DURATION_CAP_MS 300000u

/* lx_main.c arms IPC_PULSE_PHASE_TIMER at this fixed period - see the
 * fixed-tick design comment on lx_fsm_on_phase_timer() in lx_fsm.c. */
#define LX_PHASE_TICK_MS            100u

/*
 * TL-05/TL-06/SC-02/UC-02: pedestrian WALK -> FLASHING_DONT_WALK ->
 * DONT_WALK sequence durations. PLACEHOLDER - ARBITRARY, NOT a spec value:
 * no document in this repository (STATE_CHARTS.md, system_assumptions_
 * tables.md, usecase.md, SEQUENCE_DIAGRAMS.md, or
 * TeamQNX_A2_InitialDesignReport-1.pdf) states a numeric WALK or
 * FLASHING_DONT_WALK duration or a crossing-distance/walking-speed
 * formula anywhere - confirmed by a direct text search of the design
 * report appendices. These two values are a reasonable stand-in loosely
 * consistent with common real-world pedestrian-signal timing (a short
 * fixed WALK interval plus a clearance interval), same spirit as
 * c_mode_eng.h's C_MODE_ENG_DEFAULT_PEAK_START_HOUR placeholder. Do not
 * treat these as anything other than a stand-in pending an explicit team
 * decision.
 */
#define LX_WALK_MS                    6000u
#define LX_FLASHING_DONT_WALK_MS      4000u

/*
 * CC-03/UC-05/SD-05: after a railway crossing reopens with QUEUE_WARNING
 * still active, the immediately-following connector green phase drains
 * the backlog by extending past its ordinary duration. The 4 s increment
 * re-check cadence reuses LX_EXTENSION_MS above verbatim - CC-03's "4 s
 * increments" is numerically and semantically the same recheck period as
 * TL-03's off-peak extension increment, so a second identical constant
 * would just be a duplicate magic number. The 60 s cap on TOTAL granted
 * extension (not on phase duration) is CC-03-specific and has no existing
 * counterpart, so it gets its own named constant.
 */
#define LX_DRAIN_MAX_EXTENSION_MS     60000u

/*
 * TC-01/TC-02: SC-01B's fixed PEAK_FIXED cycle is 48 s arterial green + 4 s
 * arterial yellow + 2 s all-red + 30 s connector green + 4 s connector
 * yellow + 2 s all-red = 90 s total. Derived from the existing duration
 * constants (never hand-duplicated) so the two can never silently drift
 * apart if either is retuned. TC-02/TC-03 green-wave offsets are only
 * meaningful modulo this cycle length.
 */
#define LX_CYCLE_LENGTH_MS  (LX_PEAK_ARTERIAL_GREEN_MS + LX_YELLOW_MS + LX_ALL_RED_MS + \
                              LX_PEAK_CONNECTOR_GREEN_MS + LX_YELLOW_MS + LX_ALL_RED_MS)

/* TL-02: the fixed PEAK_FIXED duration for whichever *_GREEN phase is
 * passed in. Returns 0 for a non-green phase - callers should never ask. */
uint32_t lx_timer_peak_green_duration_ms(signal_phase_t phase);

/*
 * TL-01/TL-03/DP-06: whether an OFF_PEAK_SENSOR *_GREEN phase should exit
 * to yellow now, given elapsed green time and the two approaches' demand.
 *
 * `requires_other_demand` encodes DP-06's anti-starvation asymmetry: the
 * ARTERIAL phase must not exit without a genuine pending CONNECTOR
 * request (pass 1, own_demand=arterial, other_demand=connector) - a
 * waiting connector approach must never be starved past one further
 * arterial session, but arterial must also never yield to a phantom
 * connector-side request. The CONNECTOR phase has no such requirement
 * (pass 0, own_demand=connector, other_demand unused) - the arterial gets
 * service again unconditionally next cycle regardless of its own demand.
 *
 * Never returns 1 (exit) below LX_MIN_GREEN_MS, even if already "maxed"
 * by the caller's own bookkeeping - minimum green is never skipped.
 */
uint8_t lx_timer_should_exit_green(uint32_t elapsed_ms, uint8_t own_demand,
                                    uint8_t other_demand, uint8_t requires_other_demand);

#endif /* LX_TIMER_H */
