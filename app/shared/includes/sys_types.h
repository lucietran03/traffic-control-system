#ifndef SYS_TYPES_H
#define SYS_TYPES_H

#include <stdint.h>

/*
 * Shared enumerations and bitmasks for the distributed traffic-control
 * system. Every node (c_main, lx_main, rlx_main) includes this file so
 * that Qnet payloads and state values are interpreted identically.
 *
 * Traceability: names match SEQUENCE_DIAGRAMS.md / usecase.md /
 * system_assumptions_tables.md verbatim. Do not rename a value here
 * without updating those documents.
 */

/* Identifies one of the 10 distributed nodes (C1, L1-L6, RL1-RL3). */
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

typedef enum {
    ROLE_CENTRAL = 0,
    ROLE_INTERSECTION,
    ROLE_RAILWAY
} controller_role_t;

/* DP-01: only two normal traffic-demand modes are modelled. */
typedef enum {
    MODE_PEAK_FIXED = 0,
    MODE_OFF_PEAK_SENSOR
} operating_mode_t;

/* RC-xx: railway crossing lifecycle, reported via CROSSING_STATUS. */
typedef enum {
    CROSSING_OPEN = 0,
    CROSSING_WARNING,
    CROSSING_CLOSED,
    CROSSING_FAULT
} crossing_state_t;

/* PA-07/SC-05: Central-connectivity state of a local controller (Lx or RLx). */
typedef enum {
    LINK_CENTRAL_CONNECTED = 0,
    LINK_DEGRADED_LOCAL,
    LINK_RESYNCHRONISING
} connectivity_state_t;

/* UC-08/PA-11: only CLEAR_ROUTE exists today; kept as an enum for
 * future override types rather than a bare bool. */
typedef enum {
    OVERRIDE_CLEAR_ROUTE = 0
} override_type_t;

/*
 * PA-03/PA-06/RC-11: hardware fault flags, OR-able bitmask reported in
 * STATUS and FAULT_REPORT payloads. Add new bits at the end; never
 * reorder or reuse a bit once a node has shipped with it.
 */
typedef uint32_t fault_flags_t;
#define FAULT_NONE                  ((fault_flags_t)0)
#define FAULT_GATE_CONFIRM_MISSING  ((fault_flags_t)1u << 0)  /* RC-06 */
#define FAULT_TRAIN_SENSOR_STUCK    ((fault_flags_t)1u << 1)  /* RC-11 */
#define FAULT_PED_BUTTON_STUCK      ((fault_flags_t)1u << 2)  /* PA-03 */
#define FAULT_VEHICLE_SENSOR_STUCK  ((fault_flags_t)1u << 3)  /* PA-06 */
#define FAULT_WATCHDOG_TRIP         ((fault_flags_t)1u << 4)  /* PA-10 */

/*
 * Reason a COMMAND was rejected with NACK. Kept separate from a free-text
 * string so C1's HMI/logger can filter and count reasons programmatically;
 * a short human-readable string still travels in fault_report_payload_t
 * for the cases that need one.
 */
typedef enum {
    NACK_REASON_NONE = 0,
    NACK_REASON_INVALID_DURATION,          /* PA-11 */
    NACK_REASON_RAILWAY_CONFLICT,          /* CC-02 */
    NACK_REASON_PEDESTRIAN_ACTIVE,         /* PA-02 */
    NACK_REASON_FAULT_ACTIVE,              /* PA-10 */
    NACK_REASON_STALE_OR_UNSAFE_PROFILE,   /* TC-04 */
    NACK_REASON_OUT_OF_RANGE,
    NACK_REASON_UNKNOWN_TARGET
} nack_reason_t;

#endif /* SYS_TYPES_H */
