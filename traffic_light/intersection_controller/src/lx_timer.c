#include "lx_timer.h"

uint32_t lx_timer_peak_green_duration_ms(signal_phase_t phase)
{
    switch (phase) {
    case PHASE_ARTERIAL_GREEN:
        return LX_PEAK_ARTERIAL_GREEN_MS;
    case PHASE_CONNECTOR_GREEN:
        return LX_PEAK_CONNECTOR_GREEN_MS;
    default:
        /* Callers should never ask for a non-green phase's PEAK_FIXED
         * duration; 0 makes any such misuse immediately obvious (the
         * caller's elapsed-ms comparison would trip on the very next
         * tick) rather than silently returning a plausible-looking value. */
        return 0u;
    }
}

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
