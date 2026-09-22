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

#include <string.h>

#include "c_mode_eng.h"

// Defines pre-configured arterial chains for coordinated green-wave offsets.
static const c_arterial_offset_t R1_CHAIN[] = {
    { CTRL_L1, R1_L1_OFFSET_MS },
    { CTRL_L3, R1_L3_OFFSET_MS },
    { CTRL_L5, R1_L5_OFFSET_MS }
};

static const c_arterial_offset_t R2_CHAIN[] = {
    { CTRL_L2, R2_L2_OFFSET_MS },
    { CTRL_L4, R2_L4_OFFSET_MS },
    { CTRL_L6, R2_L6_OFFSET_MS }
};

// Initialises all 9 controller slots and the default schedule/profile state.
void c_mode_eng_init(c_mode_eng_t *eng)
{
    int i;

    memset(eng->controllers, 0, sizeof(eng->controllers));

    for (i = 0; i < 6; i++) {
        eng->controllers[i].id   = (controller_id_t)(CTRL_L1 + i);
        eng->controllers[i].role = ROLE_INTERSECTION;
    }
    for (i = 0; i < 3; i++) {
        eng->controllers[6 + i].id   = (controller_id_t)(CTRL_RL1 + i);
        eng->controllers[6 + i].role = ROLE_RAILWAY;
    }

    eng->schedule.peak_start_hour = C_MODE_ENG_DEFAULT_PEAK_START_HOUR;
    eng->schedule.peak_end_hour   = C_MODE_ENG_DEFAULT_PEAK_END_HOUR;

    eng->next_profile_id = 1;

    eng->demo_hour_override_active = 0;
    eng->demo_hour                 = 0;
    eng->last_auto_mode            = MODE_PEAK_FIXED;
    eng->last_auto_mode_valid      = 0;
}

// Maps a controller ID to its slot index (0-5 = Lx, 6-8 = RLx) in the status table.
int c_mode_eng_controller_index(controller_id_t id)
{
    if (id >= CTRL_L1 && id <= CTRL_L6) {
        return (int)(id - CTRL_L1);
    }
    if (id >= CTRL_RL1 && id <= CTRL_RL3) {
        return 6 + (int)(id - CTRL_RL1);
    }
    return -1;
}

// Selects PEAK_FIXED or OFF_PEAK_SENSOR for the given hour against the configured schedule.
operating_mode_t c_mode_eng_select_mode(const c_mode_eng_t *eng, uint8_t current_hour)
{
    if (current_hour >= eng->schedule.peak_start_hour && current_hour < eng->schedule.peak_end_hour) {
        return MODE_PEAK_FIXED;
    }
    return MODE_OFF_PEAK_SENSOR;
}

// Checks whether the auto-selected mode for this hour differs from the last one applied.
int c_mode_eng_auto_check(c_mode_eng_t *eng, uint8_t current_hour, operating_mode_t *out_mode)
{
    operating_mode_t computed = c_mode_eng_select_mode(eng, current_hour);

    if (!eng->last_auto_mode_valid) {
        eng->last_auto_mode       = computed;
        eng->last_auto_mode_valid = 1;
        return 0;
    }

    if (computed == eng->last_auto_mode) {
        return 0;
    }

    eng->last_auto_mode = computed;
    if (out_mode != NULL) {
        *out_mode = computed;
    }
    return 1;
}

// Records the mode just broadcast to all six Lx controllers for status-table display.
void c_mode_eng_mark_all_lx_commanded(c_mode_eng_t *eng, operating_mode_t mode)
{
    int i;

    for (i = 0; i < 6; i++) {
        eng->controllers[i].last_commanded_mode = mode;
    }
}

// Builds one SET_TIMING_PROFILE request per controller in the given arterial chain.
int c_mode_eng_build_timing_profile(uint32_t profile_id, const c_arterial_offset_t *chain, int chain_len, ipc_request_t *out_requests)
{
    int i;

    for (i = 0; i < chain_len; i++) {
        memset(&out_requests[i], 0, sizeof(out_requests[i]));
        out_requests[i].verb      = MSG_SET_TIMING_PROFILE;
        out_requests[i].sender_id = CTRL_C1;
        out_requests[i].target_id = chain[i].id;
        out_requests[i].payload.timing_profile.profile_id = profile_id;
        out_requests[i].payload.timing_profile.offset_ms  = chain[i].offset_ms;
    }

    return chain_len;
}

// Returns the next unused timing-profile ID and advances the counter.
uint32_t c_mode_eng_next_profile_id(c_mode_eng_t *eng)
{
    return eng->next_profile_id++;
}

// Returns the pre-configured R1 or R2 arterial chain and its length.
const c_arterial_offset_t *c_mode_eng_get_chain(c_arterial_chain_id_t chain_id, int *out_len)
{
    switch (chain_id) {
    case C_ARTERIAL_CHAIN_R1:
        *out_len = (int)(sizeof(R1_CHAIN) / sizeof(R1_CHAIN[0]));
        return R1_CHAIN;
    case C_ARTERIAL_CHAIN_R2:
        *out_len = (int)(sizeof(R2_CHAIN) / sizeof(R2_CHAIN[0]));
        return R2_CHAIN;
    default:
        *out_len = 0;
        return NULL;
    }
}

// Validates manual override requests, ensuring target, duration, and type bounds are met.
int c_mode_eng_validate_override_request(controller_id_t target_id, const request_override_payload_t *payload, nack_reason_t *out_reason)
{
    *out_reason = NACK_REASON_NONE;

    if (target_id < CTRL_L1 || target_id > CTRL_L6) {
        *out_reason = NACK_REASON_UNKNOWN_TARGET;
        return 0;
    }
    if (payload->duration_ms == 0 || payload->duration_ms > 300000) {
        *out_reason = NACK_REASON_INVALID_DURATION;
        return 0;
    }
    if (payload->override_type != OVERRIDE_CLEAR_ROUTE) {
        *out_reason = NACK_REASON_OUT_OF_RANGE;
        return 0;
    }

    return 1;
}