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
#include "rlx_timer.h"

// Safely decrements active timers without underflowing and returns 1 upon window expiration.
uint8_t rlx_timer_tick_window(uint32_t *remaining_ms, uint32_t tick_ms)
{
    if (*remaining_ms > tick_ms) {
        *remaining_ms -= tick_ms;
        return 0u;
    }
    *remaining_ms = 0u;
    return 1u;
}