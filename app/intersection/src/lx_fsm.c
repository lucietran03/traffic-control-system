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

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "sys_types.h"
#include "ipc_msg.h"
#include "lx_fsm.h"
#include "lx_timer.h"
#include "lx_signal.h"

// Implements the intersection state machine for phase logic and supervisory overlays without managing direct IPC.

// --- internal helpers (all assume fsm->lock is already held) --------- //

static void lx_fsm_terminate_override_locked(lx_fsm_t *fsm);
static void lx_fsm_apply_offset_locked(lx_fsm_t *fsm);

// Applies fault-safe constraints immediately on any tick or event if a fault is detected.
static void lx_fsm_check_fault_locked(lx_fsm_t *fsm)
{
    if (fsm->faults != FAULT_NONE && fsm->supervisory != SUPERVISORY_FAULT_SAFE) {
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
            lx_fsm_terminate_override_locked(fsm);
        }
        fsm->supervisory = SUPERVISORY_FAULT_SAFE;
    }
}

// Maps latched requests on sides 0 and 1 to connector crosswalks compatible with arterial green.
static uint8_t lx_fsm_arterial_ped_compatible_locked(const lx_fsm_t *fsm)
{
    return (uint8_t)(fsm->ped_latched[0] || fsm->ped_latched[1]);
}

// Maps latched requests on sides 2 and 3 to arterial crosswalks compatible with connector green.
static uint8_t lx_fsm_connector_ped_compatible_locked(const lx_fsm_t *fsm)
{
    return (uint8_t)(fsm->ped_latched[2] || fsm->ped_latched[3]);
}

// Manages timing and state sequencing for active pedestrian crossings in lockstep with vehicle phases.
static void lx_fsm_ped_service_tick_locked(lx_fsm_t *fsm)
{
    uint8_t side;

    if (fsm->ped_phase == PED_PHASE_NONE) {
        uint8_t compatible_mask = 0;

        if (fsm->phase == PHASE_ARTERIAL_GREEN) {
            if (fsm->ped_latched[0]) { compatible_mask |= (uint8_t)(1u << 0); }
            if (fsm->ped_latched[1]) { compatible_mask |= (uint8_t)(1u << 1); }
        } else if (fsm->phase == PHASE_CONNECTOR_GREEN) {
            if (fsm->ped_latched[2]) { compatible_mask |= (uint8_t)(1u << 2); }
            if (fsm->ped_latched[3]) { compatible_mask |= (uint8_t)(1u << 3); }
        }

        if (compatible_mask != 0) {
            fsm->ped_serving_mask = compatible_mask;
            fsm->ped_phase = PED_PHASE_WALK;
            fsm->ped_phase_elapsed_ms = 0;
            fsm->ped_clearance_active = 1; 
            for (side = 0; side < 4; side++) {
                if (compatible_mask & (uint8_t)(1u << side)) {
                    lx_signal_show_walk(fsm->self_id, side);
                }
            }
        }
        return;
    }

    fsm->ped_phase_elapsed_ms += LX_PHASE_TICK_MS;

    if (fsm->ped_phase == PED_PHASE_WALK) {
        if (fsm->ped_phase_elapsed_ms >= LX_WALK_MS) {
            fsm->ped_phase = PED_PHASE_FLASHING_DONT_WALK;
            fsm->ped_phase_elapsed_ms = 0;
            for (side = 0; side < 4; side++) {
                if (fsm->ped_serving_mask & (uint8_t)(1u << side)) {
                    lx_signal_show_flashing_dont_walk(fsm->self_id, side);
                }
            }
        }
    } else { 
        if (fsm->ped_phase_elapsed_ms >= LX_FLASHING_DONT_WALK_MS) {
            for (side = 0; side < 4; side++) {
                if (fsm->ped_serving_mask & (uint8_t)(1u << side)) {
                    lx_signal_show_dont_walk(fsm->self_id, side);
                    if (fsm->ped_recall[side]) {
                        fsm->ped_recall[side] = 0;
                    } else {
                        fsm->ped_latched[side] = 0; 
                    }
                }
            }
            fsm->ped_serving_mask = 0;
            fsm->ped_phase = PED_PHASE_NONE;
            fsm->ped_phase_elapsed_ms = 0;
            fsm->ped_clearance_active = 0; 
        }
    }
}

// Safely terminates an active override state and resumes normal phase execution.
static void lx_fsm_terminate_override_locked(lx_fsm_t *fsm)
{
    lx_signal_show_override_clearance(fsm->self_id);
    fsm->override_substate = OVR_NONE;
    fsm->override_remaining_ms = 0;
    if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
        fsm->supervisory = SUPERVISORY_NORMAL_OPERATION;
    }
}

// Sequences the signal through the fixed six-phase cycle and handles pending mode or routing constraints.
static void lx_fsm_advance_phase_locked(lx_fsm_t *fsm)
{
    switch (fsm->phase) {
    case PHASE_ARTERIAL_GREEN:
        fsm->phase = PHASE_ARTERIAL_YELLOW;
        break;
    case PHASE_ARTERIAL_YELLOW:
        fsm->phase = PHASE_ALL_RED_A_TO_B;
        break;
    case PHASE_ALL_RED_A_TO_B:
        if (fsm->mode_change_pending) {
            fsm->mode = fsm->pending_mode;
            fsm->mode_change_pending = 0;
        }
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_ACTIVE) {
            fsm->phase = (fsm->override_target_movement == (uint32_t)OVERRIDE_MOVEMENT_CONNECTOR)
                             ? PHASE_CONNECTOR_GREEN : PHASE_ARTERIAL_GREEN;
            break;
        }
        if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) {
            fsm->phase = PHASE_ARTERIAL_GREEN;
            break;
        }
        if (fsm->drain_pending) {
            fsm->drain_pending = 0;
            fsm->drain_active = 1;
            fsm->drain_extending = 0;
            fsm->drain_extension_total_ms = 0;
        }
        fsm->phase = PHASE_CONNECTOR_GREEN;
        break;
    case PHASE_CONNECTOR_GREEN:
        fsm->phase = PHASE_CONNECTOR_YELLOW;
        break;
    case PHASE_CONNECTOR_YELLOW:
        fsm->phase = PHASE_ALL_RED_B_TO_A;
        break;
    case PHASE_ALL_RED_B_TO_A:
        if (fsm->mode_change_pending) {
            fsm->mode = fsm->pending_mode;
            fsm->mode_change_pending = 0;
        }
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_ACTIVE) {
            fsm->phase = (fsm->override_target_movement == (uint32_t)OVERRIDE_MOVEMENT_CONNECTOR)
                             ? PHASE_CONNECTOR_GREEN : PHASE_ARTERIAL_GREEN;
            break;
        }
        fsm->phase = PHASE_ARTERIAL_GREEN;
        break;
    default:
        fsm->phase = PHASE_ALL_RED_A_TO_B;
        break;
    }
    fsm->green_elapsed_ms = 0;
    if (fsm->phase == PHASE_ARTERIAL_GREEN && fsm->offset_apply_pending) {
        fsm->offset_apply_pending = 0;
        lx_fsm_apply_offset_locked(fsm);
    }
    lx_signal_show_phase(fsm->self_id, fsm->phase);
}

// --- lifecycle --------------------------------------------------------- //

void lx_fsm_init(lx_fsm_t *fsm, controller_id_t self_id)
{
    pthread_mutex_init(&fsm->lock, NULL);

    pthread_mutex_lock(&fsm->lock);
    fsm->self_id = self_id;
    fsm->mode = MODE_PEAK_FIXED;
    fsm->mode_change_pending = 0;
    fsm->pending_mode = MODE_PEAK_FIXED;
    fsm->phase = PHASE_ARTERIAL_GREEN;
    fsm->green_elapsed_ms = 0;
    fsm->arterial_vehicle_demand = 0;
    fsm->connector_vehicle_demand = 0;
    memset(fsm->ped_latched, 0, sizeof(fsm->ped_latched));
    memset(fsm->ped_recall, 0, sizeof(fsm->ped_recall));
    fsm->ped_phase = PED_PHASE_NONE;
    fsm->ped_phase_elapsed_ms = 0;
    fsm->ped_serving_mask = 0;
    fsm->queue_warning_active = 0;
    fsm->drain_pending = 0;
    fsm->drain_active = 0;
    fsm->drain_extending = 0;
    fsm->drain_extension_total_ms = 0;
    fsm->last_crossing_state = CROSSING_OPEN;
    fsm->supervisory = SUPERVISORY_NORMAL_OPERATION;
    fsm->override_substate = OVR_NONE;
    fsm->override_target_movement = 0;
    fsm->override_duration_ms = 0;
    fsm->override_remaining_ms = 0;
    fsm->ped_clearance_active = 0;
    fsm->link_state = LINK_DEGRADED_LOCAL;
    fsm->missed_heartbeat_acks = 0;
    fsm->active_profile_id = 0;
    fsm->assigned_offset_ms = 0;
    fsm->offset_apply_pending = 0;
    fsm->offset_extra_hold_ms = 0;
    fsm->faults = FAULT_NONE;
    lx_signal_show_phase(fsm->self_id, fsm->phase);
    pthread_mutex_unlock(&fsm->lock);
}

// --- sensor-input setters (called by lx_sensor.c) ----------------------- //

void lx_fsm_set_arterial_vehicle_demand(lx_fsm_t *fsm, uint8_t present)
{
    pthread_mutex_lock(&fsm->lock);
    fsm->arterial_vehicle_demand = present ? 1u : 0u;
    pthread_mutex_unlock(&fsm->lock);
}

// Sets the connector vehicle demand flag, which is used to determine whether the connector phase should be extended or skipped.
void lx_fsm_set_connector_vehicle_demand(lx_fsm_t *fsm, uint8_t present)
{
    pthread_mutex_lock(&fsm->lock);
    fsm->connector_vehicle_demand = present ? 1u : 0u;
    pthread_mutex_unlock(&fsm->lock);
}

// Latch a pedestrian request for a side, flagging recalls if it occurs mid-sequence.
void lx_fsm_latch_pedestrian_request(lx_fsm_t *fsm, uint8_t side)
{
    pthread_mutex_lock(&fsm->lock);
    if (side < 4) {
        if (fsm->ped_serving_mask & (uint8_t)(1u << side)) {
            fsm->ped_recall[side] = 1;
        }
        fsm->ped_latched[side] = 1;
    }
    pthread_mutex_unlock(&fsm->lock);
}

void lx_fsm_set_queue_warning(lx_fsm_t *fsm, uint8_t active)
{
    pthread_mutex_lock(&fsm->lock);
    fsm->queue_warning_active = active ? 1u : 0u;
    pthread_mutex_unlock(&fsm->lock);
}

// Directly forces fault-safe entry regardless of server loop health.
void lx_fsm_report_watchdog_trip(lx_fsm_t *fsm)
{
    pthread_mutex_lock(&fsm->lock);
    fsm->faults |= FAULT_WATCHDOG_TRIP;
    if (fsm->supervisory != SUPERVISORY_FAULT_SAFE) {
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
            lx_fsm_terminate_override_locked(fsm);
        }
        fsm->supervisory = SUPERVISORY_FAULT_SAFE;
        lx_signal_apply_fault_safe(fsm->self_id);
    }
    pthread_mutex_unlock(&fsm->lock);
}

// --- verb handlers ------------------------------------------------------ //

// Adjusts the green elapsed timer at a safe arterial boundary to synchronize offsets.
static void lx_fsm_apply_offset_locked(lx_fsm_t *fsm)
{
    struct timespec ts;
    uint64_t        now_ms;
    uint32_t        target_phase_in_cycle;
    uint32_t        actual_start_phase_in_cycle;
    int32_t         error_ms;
    uint32_t        fixed_dur;

    fsm->offset_extra_hold_ms = 0;

    if (fsm->mode != MODE_PEAK_FIXED || fsm->phase != PHASE_ARTERIAL_GREEN) {
        return;
    }
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return; 
    }

    now_ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000);
    target_phase_in_cycle = (uint32_t)(fsm->assigned_offset_ms % LX_CYCLE_LENGTH_MS);
    
    actual_start_phase_in_cycle =
        (uint32_t)(((now_ms % LX_CYCLE_LENGTH_MS) + LX_CYCLE_LENGTH_MS -
                    (fsm->green_elapsed_ms % LX_CYCLE_LENGTH_MS)) % LX_CYCLE_LENGTH_MS);

    error_ms = (int32_t)actual_start_phase_in_cycle - (int32_t)target_phase_in_cycle;
    if (error_ms > (int32_t)(LX_CYCLE_LENGTH_MS / 2)) {
        error_ms -= (int32_t)LX_CYCLE_LENGTH_MS;
    } else if (error_ms < -(int32_t)(LX_CYCLE_LENGTH_MS / 2)) {
        error_ms += (int32_t)LX_CYCLE_LENGTH_MS;
    }

    if (error_ms > 0) {
        fsm->green_elapsed_ms += (uint32_t)error_ms;
    } else if (error_ms < 0) {
        fsm->offset_extra_hold_ms = (uint32_t)(-error_ms);
        return; 
    }

    fixed_dur = lx_timer_peak_green_duration_ms(fsm->phase);
    if (fixed_dur > 0) {
        uint32_t max_elapsed_after_correction =
            (fixed_dur > LX_MIN_GREEN_MS) ? (fixed_dur - LX_MIN_GREEN_MS) : 0;
        if (fsm->green_elapsed_ms > max_elapsed_after_correction) {
            fsm->green_elapsed_ms = max_elapsed_after_correction;
        }
    }
}

// Handles a SET_TIMING_PROFILE request, applying the new profile and offset if valid and safe.
void lx_fsm_on_set_timing_profile(lx_fsm_t *fsm, const set_timing_profile_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_FAULT_ACTIVE;
    } else if (payload->offset_ms >= LX_CYCLE_LENGTH_MS) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_STALE_OR_UNSAFE_PROFILE;
    } else {
        fsm->active_profile_id = payload->profile_id;
        fsm->assigned_offset_ms = payload->offset_ms;
        fsm->offset_apply_pending = 1;
        reply->result = RESULT_ACK;
    }
    pthread_mutex_unlock(&fsm->lock);
}

// Handles a SET_MODE request, validating the requested mode and scheduling a change if necessary.
void lx_fsm_on_set_mode(lx_fsm_t *fsm, const set_mode_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_FAULT_ACTIVE;
    } else if (payload->mode != (uint32_t)MODE_PEAK_FIXED && payload->mode != (uint32_t)MODE_OFF_PEAK_SENSOR) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_OUT_OF_RANGE;
    } else if ((operating_mode_t)payload->mode == fsm->mode) {
        fsm->mode_change_pending = 0;
        reply->result = RESULT_ACK;
    } else {
        fsm->pending_mode = (operating_mode_t)payload->mode;
        fsm->mode_change_pending = 1;
        reply->result = RESULT_ACK_PENDING;
    }
    pthread_mutex_unlock(&fsm->lock);
}

// Handles a REQUEST_OVERRIDE request, validating the requested override and scheduling it if possible.
void lx_fsm_on_request_override(lx_fsm_t *fsm, const request_override_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    // Check for conflicts with existing supervisory states or invalid parameters.
    if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_OUT_OF_RANGE; 
    } else if (payload->duration_ms == 0 || payload->duration_ms > LX_OVERRIDE_DURATION_CAP_MS) { // Check for valid duration
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_INVALID_DURATION;
    } else if (payload->target_movement != (uint32_t)OVERRIDE_MOVEMENT_ARTERIAL &&
               payload->target_movement != (uint32_t)OVERRIDE_MOVEMENT_CONNECTOR) { // Check for valid target movement
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_OUT_OF_RANGE;
    } else if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) { // Check for conflicts with railway preemption
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_RAILWAY_CONFLICT;
    } else if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) { // Check for conflicts with fault-safe state
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_FAULT_ACTIVE;
    } else if (fsm->ped_clearance_active) { // Check for conflicts with active pedestrian clearance
        fsm->override_substate = OVR_PENDING_CLEARANCE;
        fsm->override_target_movement = payload->target_movement;
        fsm->override_duration_ms = payload->duration_ms;
        fsm->override_remaining_ms = payload->duration_ms;
        fsm->supervisory = SUPERVISORY_CENTRAL_OVERRIDE;
        reply->result = RESULT_ACK_PENDING;
    } else { // No conflicts, apply the override immediately
        fsm->override_substate = OVR_ACTIVE;
        fsm->override_target_movement = payload->target_movement;
        fsm->override_duration_ms = payload->duration_ms;
        fsm->override_remaining_ms = payload->duration_ms;
        fsm->supervisory = SUPERVISORY_CENTRAL_OVERRIDE;
        reply->result = RESULT_ACK;
    }
    pthread_mutex_unlock(&fsm->lock);
}

// Handles a RENEW_OVERRIDE request, extending the duration of an active override if valid.
void lx_fsm_on_renew_override(lx_fsm_t *fsm, const renew_override_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    if (fsm->supervisory != SUPERVISORY_CENTRAL_OVERRIDE || fsm->override_substate != OVR_ACTIVE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_UNKNOWN_TARGET;
    } else if (payload->extend_duration_ms > LX_OVERRIDE_DURATION_CAP_MS) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_INVALID_DURATION;
    } else {
        uint32_t new_duration = (payload->extend_duration_ms == 0)
                                     ? fsm->override_duration_ms
                                     : payload->extend_duration_ms;
        fsm->override_duration_ms = new_duration;
        fsm->override_remaining_ms = new_duration; 
        reply->result = RESULT_ACK;
    }
    pthread_mutex_unlock(&fsm->lock);
}

// Handles a CANCEL_OVERRIDE request, terminating an active override if valid.
void lx_fsm_on_cancel_override(lx_fsm_t *fsm, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    if (fsm->override_substate != OVR_PENDING_CLEARANCE && fsm->override_substate != OVR_ACTIVE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_UNKNOWN_TARGET;
    } else {
        lx_fsm_terminate_override_locked(fsm); 
        reply->result = RESULT_ACK;
    }
    pthread_mutex_unlock(&fsm->lock);
}

// Handles a CROSSING_STATUS request, updating the FSM's last known crossing state and adjusting supervisory mode accordingly.
void lx_fsm_on_crossing_status(lx_fsm_t *fsm, const crossing_status_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);

    fsm->last_crossing_state = (crossing_state_t)payload->state;

    if (fsm->supervisory != SUPERVISORY_FAULT_SAFE) {
        crossing_state_t state = (crossing_state_t)payload->state;

        if (state != CROSSING_OPEN) {
            if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
                lx_fsm_terminate_override_locked(fsm);
            }
            fsm->supervisory = SUPERVISORY_RAILWAY_PREEMPTION;
        } else if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) {
            fsm->supervisory = SUPERVISORY_NORMAL_OPERATION;
            if (fsm->queue_warning_active) {
                fsm->drain_pending = 1;
            }
        }
    }

    reply->result = RESULT_ACK;
    reply->reason = NACK_REASON_NONE;
    pthread_mutex_unlock(&fsm->lock);
}

// --- phase timer pulse handler ------------------------------------------ //

// Accumulates a fixed 100ms time tick to drive phase sequence transitions and manual override expiration.
void lx_fsm_on_phase_timer(lx_fsm_t *fsm)
{
    pthread_mutex_lock(&fsm->lock);

    lx_fsm_check_fault_locked(fsm);

    if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) {
        lx_signal_apply_fault_safe(fsm->self_id);
        pthread_mutex_unlock(&fsm->lock);
        return;
    }

    if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE &&
        (fsm->override_substate == OVR_ACTIVE || fsm->override_substate == OVR_PENDING_CLEARANCE)) {
        if (fsm->override_remaining_ms <= LX_PHASE_TICK_MS) {
            lx_fsm_terminate_override_locked(fsm); 
        } else {
            fsm->override_remaining_ms -= LX_PHASE_TICK_MS;
        }
    }

    fsm->green_elapsed_ms += LX_PHASE_TICK_MS;

    lx_fsm_ped_service_tick_locked(fsm);

    if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_PENDING_CLEARANCE) {
        if (!fsm->ped_clearance_active) {
            fsm->override_substate = OVR_ACTIVE;
        }
    }

    switch (fsm->phase) {
    case PHASE_ARTERIAL_YELLOW:
    case PHASE_CONNECTOR_YELLOW:
        if (fsm->green_elapsed_ms >= LX_YELLOW_MS) {
            lx_fsm_advance_phase_locked(fsm);
        }
        break;

    case PHASE_ALL_RED_A_TO_B:
    case PHASE_ALL_RED_B_TO_A:
        if (fsm->green_elapsed_ms >= LX_ALL_RED_MS) {
            lx_fsm_advance_phase_locked(fsm);
        }
        break;

    case PHASE_ARTERIAL_GREEN:
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_ACTIVE &&
            fsm->override_target_movement == (uint32_t)OVERRIDE_MOVEMENT_ARTERIAL) {
            break;
        }
        if (fsm->mode == MODE_PEAK_FIXED) {
            if (fsm->green_elapsed_ms >= lx_timer_peak_green_duration_ms(fsm->phase) + fsm->offset_extra_hold_ms) {
                fsm->offset_extra_hold_ms = 0;
                lx_fsm_advance_phase_locked(fsm);
            }
        } else {
            if ((fsm->green_elapsed_ms % LX_EXTENSION_MS) == 0) {
                uint8_t own_demand   = (uint8_t)(fsm->arterial_vehicle_demand || lx_fsm_arterial_ped_compatible_locked(fsm));
                uint8_t other_demand = (uint8_t)(fsm->connector_vehicle_demand || lx_fsm_connector_ped_compatible_locked(fsm));

                if (lx_timer_should_exit_green(fsm->green_elapsed_ms, own_demand, other_demand, 1u)) {
                    lx_fsm_advance_phase_locked(fsm);
                }
            }
        }
        break;

    case PHASE_CONNECTOR_GREEN:
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_ACTIVE &&
            fsm->override_target_movement == (uint32_t)OVERRIDE_MOVEMENT_CONNECTOR) {
            break;
        }

        if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION && fsm->green_elapsed_ms >= LX_MIN_GREEN_MS) {
            fsm->drain_active = 0;
            fsm->drain_extending = 0;
            fsm->drain_extension_total_ms = 0;
            lx_fsm_advance_phase_locked(fsm);
            break;
        }
        
        if (fsm->drain_active && fsm->drain_extending) {
            if ((fsm->drain_extension_total_ms % LX_EXTENSION_MS) == 0) {
                if (!fsm->queue_warning_active || fsm->drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS) {
                    fsm->drain_active = 0;
                    fsm->drain_extending = 0;
                    fsm->drain_extension_total_ms = 0;
                    lx_fsm_advance_phase_locked(fsm);
                    break;
                }
            }
            fsm->drain_extension_total_ms += LX_PHASE_TICK_MS;
            break;
        }

        if (fsm->mode == MODE_PEAK_FIXED) {
            if (fsm->green_elapsed_ms >= lx_timer_peak_green_duration_ms(fsm->phase)) {
                if (fsm->drain_active) {
                    fsm->drain_extending = 1;
                    fsm->drain_extension_total_ms = 0;
                } else {
                    lx_fsm_advance_phase_locked(fsm);
                }
            }
        } else {
            if ((fsm->green_elapsed_ms % LX_EXTENSION_MS) == 0) {
                uint8_t own_demand = (uint8_t)(fsm->connector_vehicle_demand || lx_fsm_connector_ped_compatible_locked(fsm));

                if (lx_timer_should_exit_green(fsm->green_elapsed_ms, own_demand, 0u, 0u)) {
                    if (fsm->drain_active) {
                        fsm->drain_extending = 1;
                        fsm->drain_extension_total_ms = 0;
                    } else {
                        lx_fsm_advance_phase_locked(fsm);
                    }
                }
            }
        }
        break;

    default:
        break;
    }

    pthread_mutex_unlock(&fsm->lock);
}

// --- status reporting / demo shim --------------------------------------- //

// Fills a status report structure with the current FSM state, ensuring thread safety.
void lx_fsm_fill_status(const lx_fsm_t *fsm, status_report_payload_t *status)
{
    pthread_mutex_lock((pthread_mutex_t *)&fsm->lock);

    status->role              = (uint32_t)ROLE_INTERSECTION;
    status->mode              = (uint32_t)fsm->mode;
    status->signal_phase      = (uint32_t)fsm->phase;
    status->crossing_state    = 0; 
    status->supervisory_state = (uint32_t)fsm->supervisory;
    status->faults            = fsm->faults;
    
    status->sensor_status = SENSOR_NONE;
    if (fsm->arterial_vehicle_demand)  {
         status->sensor_status |= SENSOR_ARTERIAL_DEMAND; 
    }
    if (fsm->connector_vehicle_demand) { 
        status->sensor_status |= SENSOR_CONNECTOR_DEMAND; 
    }
    if (fsm->ped_latched[0]) { 
        status->sensor_status |= SENSOR_PED_LATCHED_SIDE_0; 
    }
    if (fsm->ped_latched[1]) { 
        status->sensor_status |= SENSOR_PED_LATCHED_SIDE_1; 
    }
    if (fsm->ped_latched[2]) { 
        status->sensor_status |= SENSOR_PED_LATCHED_SIDE_2; 
    }
    if (fsm->ped_latched[3]) { 
        status->sensor_status |= SENSOR_PED_LATCHED_SIDE_3; 
    }
    if (fsm->queue_warning_active) { 
        status->sensor_status |= SENSOR_QUEUE_WARNING; 
    }
    status->active_profile_id = fsm->active_profile_id;
    status->override_active   = (uint8_t)((fsm->override_substate == OVR_ACTIVE) ? 1u : 0u);
    status->link_state        = (uint32_t)fsm->link_state;

    pthread_mutex_unlock((pthread_mutex_t *)&fsm->lock);
}

// --- heartbeat / local clock ------------------------------------------- //
// Updates the FSM's link state based on heartbeat acknowledgments, returning a transition code for external handling.
int lx_fsm_on_heartbeat_result(lx_fsm_t *fsm, int acked)
{
    int transition = 0;

    pthread_mutex_lock(&fsm->lock);
    if (acked) {
        if (fsm->link_state != LINK_CENTRAL_CONNECTED) {
            fsm->link_state = LINK_CENTRAL_CONNECTED;
            transition = 2;
        }
        fsm->missed_heartbeat_acks = 0;
    } else {
        if (fsm->missed_heartbeat_acks < 0xFFFFFFFFu) {
            fsm->missed_heartbeat_acks++;
        }
        if (fsm->missed_heartbeat_acks >= 3 && fsm->link_state == LINK_CENTRAL_CONNECTED) {
            fsm->link_state = LINK_DEGRADED_LOCAL;
            transition = 1;
        }
    }
    pthread_mutex_unlock(&fsm->lock);

    return transition;
}

// Checks the local clock to determine if the operating mode should change based on peak/off-peak hours.
void lx_fsm_local_clock_mode_check(lx_fsm_t *fsm)
{
    time_t now;
    struct tm tm_now;
    uint8_t hour;
    operating_mode_t schedule_mode;

    pthread_mutex_lock(&fsm->lock);
    if (fsm->link_state == LINK_CENTRAL_CONNECTED) {
        pthread_mutex_unlock(&fsm->lock);
        return;
    }

    now = time(NULL);
    localtime_r(&now, &tm_now);
    hour = (uint8_t)tm_now.tm_hour;
    schedule_mode = (hour >= LX_LOCAL_PEAK_START_HOUR && hour < LX_LOCAL_PEAK_END_HOUR)
                        ? MODE_PEAK_FIXED : MODE_OFF_PEAK_SENSOR;

    if (schedule_mode == fsm->mode) {
        fsm->mode_change_pending = 0;
    } else {
        fsm->pending_mode = schedule_mode;
        fsm->mode_change_pending = 1;
    }
    pthread_mutex_unlock(&fsm->lock);
}

// Handles a REQUEST_FAULT_CLEAR request, resetting the FSM's fault state and returning to normal operation if safe.
void lx_fsm_on_request_fault_clear(lx_fsm_t *fsm, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    reply->reason = NACK_REASON_NONE;
    fsm->faults = FAULT_NONE;
    if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) {
        fsm->supervisory = (fsm->last_crossing_state != CROSSING_OPEN)
                                ? SUPERVISORY_RAILWAY_PREEMPTION
                                : SUPERVISORY_NORMAL_OPERATION;
    }
    reply->result = RESULT_ACK;
    pthread_mutex_unlock(&fsm->lock);
}