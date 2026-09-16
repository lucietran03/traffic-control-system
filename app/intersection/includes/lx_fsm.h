#ifndef LX_FSM_H
#define LX_FSM_H

#include <stdint.h>
#include <pthread.h>

#include "sys_types.h"
#include "ipc_msg.h"

// Manages the intersection's phase, mode, and supervisory state logic without directly handling hardware actuation or IPC.

// Internal state tracking for active or pending manual routing overrides.
typedef enum {
    OVR_NONE = 0,
    OVR_PENDING_CLEARANCE,
    OVR_ACTIVE
} lx_override_substate_t;

// Internal state tracking for the active pedestrian crossing sequence.
typedef enum {
    PED_PHASE_NONE = 0,
    PED_PHASE_WALK,
    PED_PHASE_FLASHING_DONT_WALK
} lx_ped_phase_t;

typedef struct {
    controller_id_t         self_id;
    operating_mode_t         mode;
    uint8_t                  mode_change_pending;
    operating_mode_t         pending_mode;
    signal_phase_t           phase;

    // Tracks elapsed milliseconds for the current phase and serves as the demand-extension clock.
    uint32_t                 green_elapsed_ms;
    uint8_t                  arterial_vehicle_demand;
    uint8_t                  connector_vehicle_demand;

    // Tracks latched pedestrian calls for the connector (sides 0, 1) and arterial (sides 2, 3) approaches.
    uint8_t                  ped_latched[4];

    // Flags pedestrian calls made during an active service sequence to ensure they are re-latched afterward.
    uint8_t                  ped_recall[4];

    // Tracks the active pedestrian sequence step for the currently served crossing.
    lx_ped_phase_t           ped_phase;
    uint32_t                 ped_phase_elapsed_ms;
    uint8_t                  ped_serving_mask; 
    uint8_t                  queue_warning_active;

    // Manages the post-railway-closure connector drain phase and its granted extension times.
    uint8_t                  drain_pending;
    uint8_t                  drain_active;
    uint8_t                  drain_extending;
    uint32_t                 drain_extension_total_ms;

    // Stores the latest reported railway crossing state to ensure correct supervisory recovery after a fault.
    crossing_state_t         last_crossing_state;
    supervisory_state_t      supervisory;
    lx_override_substate_t   override_substate;
    uint32_t                 override_target_movement;

    // Tracks remaining milliseconds for an active override by decrementing on each phase tick.
    uint32_t                 override_remaining_ms;

    // Stores the most recently accepted override duration for renewal requests.
    uint32_t                 override_duration_ms;

    // Flags active pedestrian clearance sequences to delay pending overrides until safe.
    uint8_t                  ped_clearance_active;
    connectivity_state_t     link_state;

    // Tracks consecutive unacknowledged heartbeats to determine Central link degradation.
    uint32_t                 missed_heartbeat_acks;
    uint32_t                 active_profile_id;
    uint32_t                 assigned_offset_ms;

    // Flags a pending green-wave offset correction to be applied at the next safe arterial green entry.
    uint8_t                  offset_apply_pending;

    // Temporarily extends the current green phase threshold to compensate for an early start.
    uint32_t                 offset_extra_hold_ms;
    fault_flags_t            faults;

    // Mutex protecting the entire FSM state struct across all threads.
    pthread_mutex_t          lock;   
} lx_fsm_t;

// Initializes the FSM state, while the subsequent handlers process inbound Qnet requests.
void lx_fsm_init(lx_fsm_t *fsm, controller_id_t self_id);
void lx_fsm_on_set_timing_profile(lx_fsm_t *fsm, const set_timing_profile_payload_t *payload, ipc_reply_t *reply);
void lx_fsm_on_set_mode(lx_fsm_t *fsm, const set_mode_payload_t *payload, ipc_reply_t *reply);
void lx_fsm_on_request_override(lx_fsm_t *fsm, const request_override_payload_t *payload, ipc_reply_t *reply);
void lx_fsm_on_renew_override(lx_fsm_t *fsm, const renew_override_payload_t *payload, ipc_reply_t *reply);
void lx_fsm_on_cancel_override(lx_fsm_t *fsm, ipc_reply_t *reply);
void lx_fsm_on_crossing_status(lx_fsm_t *fsm, const crossing_status_payload_t *payload, ipc_reply_t *reply);

// Thread-safe setters for intra-process sensor states, explicitly side-stepping supervisory side effects.
void lx_fsm_set_arterial_vehicle_demand(lx_fsm_t *fsm, uint8_t present);
void lx_fsm_set_connector_vehicle_demand(lx_fsm_t *fsm, uint8_t present);

// Idempotently latches a pedestrian request for the specified side until fully served.
void lx_fsm_latch_pedestrian_request(lx_fsm_t *fsm, uint8_t side);

// Explicitly asserts or clears the connector queue warning flag used by drain logic.
void lx_fsm_set_queue_warning(lx_fsm_t *fsm, uint8_t active);

// Flags a watchdog trip fault to safely shift the FSM supervisory state on the next tick.
void lx_fsm_report_watchdog_trip(lx_fsm_t *fsm);

// Processes the fixed 100ms phase timer tick to drive signal transitions and decrement timers.
void lx_fsm_on_phase_timer(lx_fsm_t *fsm);

// Populates a status report payload with the current FSM snapshot.
void lx_fsm_fill_status(const lx_fsm_t *fsm, status_report_payload_t *status);

// Updates connectivity state based on the success or failure of an outgoing heartbeat.
int lx_fsm_on_heartbeat_result(lx_fsm_t *fsm, int acked);

// Safely evaluates and applies local peak-hour mode changes when disconnected from Central.
void lx_fsm_local_clock_mode_check(lx_fsm_t *fsm);

// Idempotently clears active faults and resumes normal operation if previously in a fault-safe state.
void lx_fsm_on_request_fault_clear(lx_fsm_t *fsm, ipc_reply_t *reply);

#endif /* LX_FSM_H */