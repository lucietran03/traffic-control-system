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

#ifndef RLX_TIMER_H
#define RLX_TIMER_H

#include <stdint.h>

// Pure timing-policy constants and countdown helpers driving the railway-crossing FSM.

// Lead time provided before gates begin closing.
#define RLX_WARNING_TO_CLOSING_MS 5000u

// Total time allowance consisting of warning lead plus gate-closing time.
#define RLX_CLOSING_DEADLINE_MS   15000u

// Simplified placeholder for the expected arrival time after gates confirm closed.
#define RLX_EXPECTED_ARRIVAL_MS   20000u

// Allowed duration for a per-direction occupancy window.
#define RLX_OCCUPANCY_WINDOW_MS   20000u

// Placeholder time allowance for the gate opening deadline.
#define RLX_OPENING_DEADLINE_MS   15000u

// Reserved placeholder constant for future stuck-sensor continuous diagnostic timeouts.
#define RLX_WARNING_DIAGNOSTIC_TIMEOUT_MS 60000u


// Decrements the remaining countdown without underflowing and returns 1 if the window expires.
uint8_t rlx_timer_tick_window(uint32_t *remaining_ms, uint32_t tick_ms);

#endif /* RLX_TIMER_H */