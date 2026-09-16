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

#ifndef IPC_MSG_H
#define IPC_MSG_H

#include <stdint.h>
#include "sys_types.h"

// Shared Qnet IPC cross-node message contract; local events belong in private headers.
// Pulse-compatible header must be first, and fields must be fixed-width types (no enums).

// Mirrors QNX's struct _pulse layout to prevent cross-architecture layout corruption.
typedef union {
    union {
        uint32_t sival_int;
        void    *sival_ptr;
    };
    uint32_t dummy[4];
} ipc_sigval_t;

typedef struct {
    uint16_t     type;
    uint16_t     subtype;
    int8_t       code;
    uint8_t      zero[3];
    ipc_sigval_t value;
    uint8_t      zero2[2];
    int32_t      scoid;
} msg_header_t;

// Message verbs for Qnet exchanges.
typedef enum {
    MSG_SET_TIMING_PROFILE = 1,  // C1  -> Lx  
    MSG_SET_MODE,                // C1  -> Lx
    MSG_REQUEST_OVERRIDE,        // C1  -> Lx 
    MSG_RENEW_OVERRIDE,          // C1  -> Lx
    MSG_CANCEL_OVERRIDE,         // C1  -> Lx 
    MSG_REQUEST_FAULT_CLEAR,     // C1  -> RLx / C1 -> Lx
    MSG_STATUS,                  // Lx/RLx -> C1
    MSG_FAULT_REPORT,            // RLx -> C1 
    MSG_CROSSING_STATUS,         // RLx -> Lx / RLx -> C1 
    MSG_HEARTBEAT                // Lx/RLx -> C1
} msg_type_t;

// Result carried in every ipc_reply_t.
typedef enum {
    RESULT_ACK = 1,           // Accept     
    RESULT_ACK_PENDING,       // Accepted, activation deferred
    RESULT_NACK,              // Rejected on safety/validity grounds
    RESULT_ERROR              // Request malformed or internal fault
} msg_result_t;

/* --- request payloads ------------------------------------------------ */

typedef struct {
    uint32_t profile_id;   // Applied coordination profile/version
    uint32_t offset_ms;    // Assigned green-wave offset
} set_timing_profile_payload_t;

typedef struct {
    uint32_t mode;   // Target operating_mode_t value
} set_mode_payload_t;

typedef struct {
    uint32_t override_type;    // Target override_type_t value
    uint32_t target_movement;  // Target override_movement_t value
    uint32_t duration_ms;      // Capped at 300000 ms; Lx NACKs if over
} request_override_payload_t;

typedef struct {
    uint32_t extend_duration_ms;   // 0 = renew for the original duration
} renew_override_payload_t;

// Reused for periodic STATUS and HEARTBEAT payloads to ensure consistent shape.
typedef struct {
    uint32_t role;               // Controller's controller_role_t value 
    uint32_t mode;               // Controller's operating_mode_t value
    uint32_t signal_phase;       // Controller's signal_phase_t value
    uint32_t crossing_state;     // Railway's crossing_state_t value
    uint32_t supervisory_state;  // Intersection's supervisory_state_t value
    uint32_t link_state;         // Link's connectivity_state_t value
    fault_flags_t faults;        // Active fault flags
    sensor_status_t sensor_status; // Intersection's sensor status mask
    uint32_t active_profile_id;   // 0 if none applied
    uint8_t  override_active;     // 0/1 boolean representation
} status_report_payload_t;

typedef struct {
    status_report_payload_t summary;
} heartbeat_payload_t;

typedef struct {
    uint32_t fault_code;   // Node-specific fault ID 
    uint8_t  severity;     // 0 = info, 1 = warning, 2 = critical
    char     detail[64];   // Human-readable fault description
} fault_report_payload_t;

typedef struct {
    uint32_t state;   // Railway's crossing_state_t
} crossing_status_payload_t;

/* --- envelopes --------------------------------------------------------- */

// Main IPC request evelope (header must remain first member).
typedef struct {
    msg_header_t     hdr;
    uint32_t         verb;         // Associated a msg_type_t
    uint32_t         seq_num;      // Optional correlation ID
    uint32_t         sender_id;    // Origin controller_id_t 
    uint32_t         target_id;    // Destination controller_id_t 
    uint64_t         timestamp_ms; // Cadence and staleness tracking
    union {
        set_timing_profile_payload_t timing_profile;
        set_mode_payload_t           mode;
        request_override_payload_t   override_request;
        renew_override_payload_t     override_renew;
        status_report_payload_t      status;
        fault_report_payload_t       fault_report;
        crossing_status_payload_t    crossing_status;
        heartbeat_payload_t          heartbeat;
    } payload;
} ipc_request_t;

// Main IPC reply envelope (header must remain first member).
typedef struct {
    msg_header_t hdr;
    uint32_t     result;        // Associated msg_result_t
    uint32_t     reason;        // Associated nack_reason_t if NACKed
    uint64_t     timestamp_ms;
    union {
        status_report_payload_t applied_status; // Echoed status state
    } payload;
} ipc_reply_t;

#endif /* IPC_MSG_H */
