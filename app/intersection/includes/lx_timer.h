/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Team QNX
    Member: Tran Dong Nghi - s3914633
			Le Hung - s4061665
			Hoang Minh Thang - s3999925
    Assessment: 2 - Project Implementation 
    Due date: 18/09/2026
*/

#ifndef LX_TIMER_H
#define LX_TIMER_H

#include <stdint.h>
#include "sys_types.h"

// Pure, side-effect-free timing policies and constants for evaluating intersection phase durations and demand conditions.

// Fixed phase durations for peak hours and shared transition intervals.
#define LX_PEAK_ARTERIAL_GREEN_MS   48000u
#define LX_PEAK_CONNECTOR_GREEN_MS  30000u
#define LX_YELLOW_MS                4000u
#define LX_ALL_RED_MS               2000u

// Actuated green bounds and extensions for off-peak sensor mode.
#define LX_MIN_GREEN_MS             8000u
#define LX_MAX_GREEN_MS             40000u
#define LX_EXTENSION_MS             4000u

// Maximum permitted duration for manual routing overrides.
#define LX_OVERRIDE_DURATION_CAP_MS 300000u

// Fallback schedule boundaries used for local mode selection when disconnected from Central.
#define LX_LOCAL_PEAK_START_HOUR    6u
#define LX_LOCAL_PEAK_END_HOUR      9u

// The fixed millisecond period for the phase timer tick.
#define LX_PHASE_TICK_MS            100u

// Placeholder intervals for the pedestrian crossing sequence.
#define LX_WALK_MS                    6000u
#define LX_FLASHING_DONT_WALK_MS      4000u

// The absolute maximum total extension time granted to a post-closure connector drain phase.
#define LX_DRAIN_MAX_EXTENSION_MS     60000u

// Derived total duration of a complete peak fixed cycle, used for green-wave offset alignment.
#define LX_CYCLE_LENGTH_MS  (LX_PEAK_ARTERIAL_GREEN_MS + LX_YELLOW_MS + LX_ALL_RED_MS + \
                              LX_PEAK_CONNECTOR_GREEN_MS + LX_YELLOW_MS + LX_ALL_RED_MS)

// Returns the predetermined duration for a given peak fixed green phase.
uint32_t lx_timer_peak_green_duration_ms(signal_phase_t phase);

// Evaluates demand and elapsed time to determine if an off-peak green phase should safely terminate.
uint8_t lx_timer_should_exit_green(uint32_t elapsed_ms, uint8_t own_demand,
                                    uint8_t other_demand, uint8_t requires_other_demand);

#endif /* LX_TIMER_H */