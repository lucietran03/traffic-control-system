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

#include "lx_timer.h"

// Implements the timing logic for green phase durations and off-peak exit conditions.
uint32_t lx_timer_peak_green_duration_ms(signal_phase_t phase)
{
    switch (phase) {
    case PHASE_ARTERIAL_GREEN:
        return LX_PEAK_ARTERIAL_GREEN_MS;
    case PHASE_CONNECTOR_GREEN:
        return LX_PEAK_CONNECTOR_GREEN_MS;
    default:
        return 0u;
    }
}

// Determines if an off-peak green phase should safely terminate based on minimum runtimes, demand balance, or maximum duration.
uint8_t lx_timer_should_exit_green(uint32_t elapsed_ms, uint8_t own_demand,
                                    uint8_t other_demand, uint8_t requires_other_demand)
{
    uint8_t maxed;

    if (elapsed_ms < LX_MIN_GREEN_MS) {
        return 0u;
    }
    if (requires_other_demand && !other_demand) {
        return 0u;
    }
    maxed = (uint8_t)(elapsed_ms >= LX_MAX_GREEN_MS);
    return (uint8_t)(!own_demand || maxed);
}