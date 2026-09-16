#ifndef SYS_TYPES_H
#define SYS_TYPES_H

#include <stdint.h>

// Shared global enumerations and bitmasks for the distributed traffic-control system.

// Identifies one of the 10 distributed nodes (C1, L1-L6, RL1-RL3).
typedef enum {
    CTRL_C1 = 0,
    CTRL_L1,
    CTRL_L2,
    CTRL_L3,
    CTRL_L4,
    CTRL_L5,
    CTRL_L6,
    CTRL_RL1,
    CTRL_RL2,
    CTRL_RL3,
    CTRL_UNKNOWN
} controller_id_t;

// Identifies the specific behavioral role of a node.
typedef enum {
    ROLE_CENTRAL = 0,
    ROLE_INTERSECTION,
    ROLE_RAILWAY
} controller_role_t;

// Main traffic-demand operating mode.
typedef enum {
    MODE_PEAK_FIXED = 0,
    MODE_OFF_PEAK_SENSOR
} operating_mode_t;

// Externally boardcast crossing safety states.
typedef enum {
    CROSSING_OPEN = 0,
    CROSSING_WARNING,
    CROSSING_CLOSED,
    CROSSING_FAULT
} crossing_state_t;

// Evaluates node operation priority hierarchy (lower value = higher priority).
typedef enum {
    SUPERVISORY_FAULT_SAFE = 0,
    SUPERVISORY_RAILWAY_PREEMPTION,
    SUPERVISORY_CENTRAL_OVERRIDE,
    SUPERVISORY_NORMAL_OPERATION
} supervisory_state_t;

// Vehicle signal phase mapping.
typedef enum {
    PHASE_ARTERIAL_GREEN = 0,
    PHASE_ARTERIAL_YELLOW,
    PHASE_ALL_RED_A_TO_B,
    PHASE_CONNECTOR_GREEN,
    PHASE_CONNECTOR_YELLOW,
    PHASE_ALL_RED_B_TO_A
} signal_phase_t;

// Local controller connectivity states.
typedef enum {
    LINK_CENTRAL_CONNECTED = 0,
    LINK_DEGRADED_LOCAL,
    LINK_RESYNCHRONISING
} connectivity_state_t;

// Manual control override types.
typedef enum {
    OVERRIDE_CLEAR_ROUTE = 0
} override_type_t;

// Intersection target movements for active overrides.
typedef enum {
    OVERRIDE_MOVEMENT_ARTERIAL = 0,
    OVERRIDE_MOVEMENT_CONNECTOR = 1
} override_movement_t;

// Or-able hardware fault flags 
typedef uint32_t fault_flags_t;
#define FAULT_NONE                  ((fault_flags_t)0)
#define FAULT_GATE_CONFIRM_MISSING  ((fault_flags_t)1u << 0)  
#define FAULT_TRAIN_SENSOR_STUCK    ((fault_flags_t)1u << 1)  
#define FAULT_PED_BUTTON_STUCK      ((fault_flags_t)1u << 2)  
#define FAULT_VEHICLE_SENSOR_STUCK  ((fault_flags_t)1u << 3) 
#define FAULT_WATCHDOG_TRIP         ((fault_flags_t)1u << 4)  

// Or-able bismasks for tracking active intersection sensor demand.
typedef uint32_t sensor_status_t;
#define SENSOR_NONE               ((sensor_status_t)0)
#define SENSOR_ARTERIAL_DEMAND    ((sensor_status_t)1u << 0)
#define SENSOR_CONNECTOR_DEMAND   ((sensor_status_t)1u << 1)  
#define SENSOR_PED_LATCHED_SIDE_0 ((sensor_status_t)1u << 2) 
#define SENSOR_PED_LATCHED_SIDE_1 ((sensor_status_t)1u << 3)
#define SENSOR_PED_LATCHED_SIDE_2 ((sensor_status_t)1u << 4)
#define SENSOR_PED_LATCHED_SIDE_3 ((sensor_status_t)1u << 5)
#define SENSOR_QUEUE_WARNING      ((sensor_status_t)1u << 6) 

// Error codes for rejected requests
typedef enum {
    NACK_REASON_NONE = 0,
    NACK_REASON_INVALID_DURATION, // Override duration_ms > 300000 ms
    NACK_REASON_RAILWAY_CONFLICT, // Override request conflicts with active railway preemption          
    NACK_REASON_PEDESTRIAN_ACTIVE, // Override request conflicts with active pedestrian demand        
    NACK_REASON_FAULT_ACTIVE,      // Override request conflicts with active fault condition        
    NACK_REASON_STALE_OR_UNSAFE_PROFILE,   // SET_TIMING_PROFILE request is stale or unsafe for the current mode
    NACK_REASON_OUT_OF_RANGE, // SET_MODE request is out of range for the current node role
    NACK_REASON_UNKNOWN_TARGET // Request sent to a node that is not a valid target for the request type
} nack_reason_t;

#endif /* SYS_TYPES_H */
