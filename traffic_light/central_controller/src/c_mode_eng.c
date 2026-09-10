#include <string.h>

#include "c_mode_eng.h"

/*
 * TC-01..05 arterial chains. Exposed via c_mode_eng_get_chain() below;
 * c_operator.c picks one of these based on which arterial an operator's
 * UC-03 timing-profile command targets, and hands it to
 * c_mode_eng_build_timing_profile().
 */
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

    /* PLACEHOLDER schedule (see c_mode_eng.h) - not a spec value. */
    eng->schedule.peak_start_hour = C_MODE_ENG_DEFAULT_PEAK_START_HOUR;
    eng->schedule.peak_end_hour   = C_MODE_ENG_DEFAULT_PEAK_END_HOUR;

    eng->next_profile_id = 1;
}

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

operating_mode_t c_mode_eng_select_mode(const c_mode_eng_t *eng, uint8_t current_hour)
{
    if (current_hour >= eng->schedule.peak_start_hour && current_hour < eng->schedule.peak_end_hour) {
        return MODE_PEAK_FIXED;
    }
    return MODE_OFF_PEAK_SENSOR;
}

int c_mode_eng_build_timing_profile(uint32_t profile_id, const c_arterial_offset_t *chain,
                                     int chain_len, ipc_request_t *out_requests)
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

uint32_t c_mode_eng_next_profile_id(c_mode_eng_t *eng)
{
    return eng->next_profile_id++;
}

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

int c_mode_eng_validate_override_request(controller_id_t target_id, const request_override_payload_t *payload,
                                          nack_reason_t *out_reason)
{
    /* Verifier-audit fix: always set *out_reason, even on the accept path
     * (return 1), so a future caller that reads it without first checking
     * the return value never sees indeterminate memory. */
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
