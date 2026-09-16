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

#ifndef C_MODE_ENG_H
#define C_MODE_ENG_H

#include <stdint.h>

#include "sys_types.h"
#include "ipc_msg.h"

// Central's decision layer for tracking and managing network-wide controller states.

// Configurable schedule bounds for peak and off-peak operating hours.
typedef struct {
    uint8_t peak_start_hour;   
    uint8_t peak_end_hour;     
} c_mode_schedule_t;

// Arbitrary fallback peak hour configuration.
#define C_MODE_ENG_DEFAULT_PEAK_START_HOUR 6
#define C_MODE_ENG_DEFAULT_PEAK_END_HOUR   9

// Pre-defined arterial green-wave offsets.
#define R1_L1_OFFSET_MS 0
#define R1_L3_OFFSET_MS 21000
#define R1_L5_OFFSET_MS 45000
#define R2_L2_OFFSET_MS 0
#define R2_L4_OFFSET_MS 19000
#define R2_L6_OFFSET_MS 42000

typedef struct {
    controller_id_t id;
    uint32_t        offset_ms;
} c_arterial_offset_t;

// Decision-layer tracking view for an individual controller.
typedef struct {
    controller_id_t   id;
    controller_role_t role;
    operating_mode_t  last_commanded_mode;
    uint32_t          last_applied_profile_id;
    uint8_t           override_in_flight;

    uint32_t last_reported_mode;             
    uint32_t last_reported_signal_phase;     
    uint32_t last_reported_crossing_state;   
    uint32_t last_reported_supervisory_state; 
    fault_flags_t last_reported_faults;
    sensor_status_t last_reported_sensor_status; 
    uint32_t last_reported_active_profile_id;
    uint8_t  last_reported_override_active;
    uint32_t last_reported_link_state;       
    uint64_t last_seen_timestamp_ms;         

    uint32_t missed_heartbeat_ticks;
    uint8_t  marked_unavailable;   
} c_controller_view_t;

// Global mode engine tracking states and schedule triggers.
typedef struct {
    c_controller_view_t controllers[9];   
    c_mode_schedule_t   schedule;
    uint32_t            next_profile_id;

    uint8_t          demo_hour_override_active;
    uint8_t          demo_hour;
    operating_mode_t last_auto_mode;
    uint8_t          last_auto_mode_valid;
} c_mode_eng_t;

// Initializes the mode engine, schedules, and profile sequencing defaults.
void c_mode_eng_init(c_mode_eng_t *eng);

// Maps a controller ID to its internal tracking array index.
int c_mode_eng_controller_index(controller_id_t id);

// Determines the appropriate operating mode based on the current scheduled hour.
operating_mode_t c_mode_eng_select_mode(const c_mode_eng_t *eng, uint8_t current_hour);

// Evaluates schedule bounds and returns 1 if a peak-hour mode transition is required.
int c_mode_eng_auto_check(c_mode_eng_t *eng, uint8_t current_hour, operating_mode_t *out_mode);

// Records the latest commanded operating mode for all intersection controllers.
void c_mode_eng_mark_all_lx_commanded(c_mode_eng_t *eng, operating_mode_t mode);

// Populates outbound IPC requests for an arterial green-wave chain.
int c_mode_eng_build_timing_profile(uint32_t profile_id, const c_arterial_offset_t *chain, int chain_len, ipc_request_t *out_requests);

// Retrieves and increments the sequential coordination profile ID.
uint32_t c_mode_eng_next_profile_id(c_mode_eng_t *eng);

// Pre-defined arterial chain sequence definitions.
typedef enum {
    C_ARTERIAL_CHAIN_R1 = 1, // R1: L1 -> L3 -> L5
    C_ARTERIAL_CHAIN_R2 = 2  // R2: L2 -> L4 -> L6
} c_arterial_chain_id_t;

// Retrieves the pre-defined sequence and offsets for a specific arterial chain.
const c_arterial_offset_t *c_mode_eng_get_chain(c_arterial_chain_id_t chain_id, int *out_len);

// Performs surface-level validation on manual override requests before dispatching.
int c_mode_eng_validate_override_request(controller_id_t target_id, const request_override_payload_t *payload, nack_reason_t *out_reason);

#endif /* C_MODE_ENG_H */