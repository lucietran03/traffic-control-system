#include "rlx_timer.h"

uint8_t rlx_timer_tick_window(uint32_t *remaining_ms, uint32_t tick_ms)
{
    if (*remaining_ms > tick_ms) {
        *remaining_ms -= tick_ms;
        return 0u;
    }
    *remaining_ms = 0u;
    return 1u;
}
