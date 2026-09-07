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

/*
 * Externally-broadcast CROSSING_STATUS vocabulary only (SD-04/SD-05/SD-06:
 * CROSSING_STATUS(WARNING|CLOSED|OPEN|FAULT)). RLx's own richer internal
 * sub-states (CLOSING, TRAIN_PRESENT, OPENING, RECLOSING - STATE_CHARTS.md
 * SC-04A/SC-04B) are private to rlx_fsm.h/.c and deliberately excluded
 * here: they never cross a Qnet boundary, so they don't belong in the
 * shared contract (same reasoning as excluding raw signal-head commands).
 */
typedef enum {
    CROSSING_OPEN = 0,
    CROSSING_WARNING,
    CROSSING_CLOSED,
    CROSSING_FAULT
} crossing_state_t;

/*
 * Intersection supervisory authority (STATE_CHARTS.md SC-03A;
 * spec-vi/03-van-hanh-va-hanh-vi.md Muc 17). Priority is highest-first
 * (lower value = higher priority), matching the documented ranking:
 * FAULT_SAFE always wins locally; RAILWAY_PREEMPTION overrides normal
 * operation for the toward-crossing approach only; CENTRAL_OVERRIDE is
 * rejected outright if it conflicts with an active RAILWAY_PREEMPTION.
 * Reported in status_report_payload_t so C1's UC-09 monitoring view can
 * tell a railway-suppressed or overridden Lx apart from one running
 * normally - operating_mode_t alone cannot, since this axis overlays on
 * top of PEAK_FIXED/OFF_PEAK_SENSOR rather than replacing it.
 */
typedef enum {
    SUPERVISORY_FAULT_SAFE = 0,
    SUPERVISORY_RAILWAY_PREEMPTION,
    SUPERVISORY_CENTRAL_OVERRIDE,
    SUPERVISORY_NORMAL_OPERATION
} supervisory_state_t;

/*
 * Vehicle-signal phase, reported in status_report_payload_t alongside
 * supervisory_state_t (SC-01B/SC-01C; SD-03's "STATUS(profile active,
 * phase state)"; UC-09's "signal ... state"). Meaningful for
 * ROLE_INTERSECTION only. Shared by PEAK_FIXED and OFF_PEAK_SENSOR - both
 * use the same six-phase arterial/connector cycle (TL-01, TL-02, TL-03),
 * just with different durations, so one enum covers both modes.
 */
typedef enum {
    PHASE_ARTERIAL_GREEN = 0,
    PHASE_ARTERIAL_YELLOW,
    PHASE_ALL_RED_A_TO_B,
    PHASE_CONNECTOR_GREEN,
    PHASE_CONNECTOR_YELLOW,
    PHASE_ALL_RED_B_TO_A
} signal_phase_t;

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
 * UC-08/SD-07: request_override_payload_t.target_movement's encoding
 * (ipc_msg.h). No spec document ever defined what this uint32_t means -
 * found and closed by a compliance audit as a real functional gap (an
 * override was accepted/tracked/reported correctly but never actually
 * actuated any signal, since nothing could interpret this field). Kept as
 * a real enum for documentation clarity even though the wire field itself
 * stays a plain uint32_t per ipc_msg.h's "no enum-typed wire fields" rule
 * - only these two values are ever placed in target_movement. Judgment
 * call: only the two approach-level movements this system already models
 * (arterial vs. connector, matching arterial_vehicle_demand/connector_
 * vehicle_demand) are representable - not a specific single approach -
 * since nothing in the shared contract identifies individual approaches.
 */
typedef enum {
    OVERRIDE_MOVEMENT_ARTERIAL = 0,
    OVERRIDE_MOVEMENT_CONNECTOR = 1
} override_movement_t;

/*
 * PA-03/PA-06/PA-10/RC-06/RC-11: hardware fault flags, OR-able bitmask
 * reported in STATUS and FAULT_REPORT payloads. Add new bits at the end;
 * never reorder or reuse a bit once a node has shipped with it.
 */
typedef uint32_t fault_flags_t;
#define FAULT_NONE                  ((fault_flags_t)0)
#define FAULT_GATE_CONFIRM_MISSING  ((fault_flags_t)1u << 0)  /* RC-06 */
#define FAULT_TRAIN_SENSOR_STUCK    ((fault_flags_t)1u << 1)  /* RC-11 */
#define FAULT_PED_BUTTON_STUCK      ((fault_flags_t)1u << 2)  /* PA-03 */
#define FAULT_VEHICLE_SENSOR_STUCK  ((fault_flags_t)1u << 3)  /* PA-06 */
#define FAULT_WATCHDOG_TRIP         ((fault_flags_t)1u << 4)  /* PA-10 */

/*
 * UC-09: "the display presents each available controller's mode, signal
 * or crossing state, sensor status, and active faults" - this bitmask is
 * the "sensor status" part, reported alongside fault_flags_t (which
 * reports a sensor STUCK, not its ordinary reading) in
 * status_report_payload_t. Meaningful for ROLE_INTERSECTION only - RLx's
 * train-approach sensor has no equivalent steady-state "reading" to
 * report this way; its presence is already fully reflected in
 * crossing_state_t, so RLx leaves this field at 0.
 */
typedef uint32_t sensor_status_t;
#define SENSOR_NONE               ((sensor_status_t)0)
#define SENSOR_ARTERIAL_DEMAND    ((sensor_status_t)1u << 0)  /* PA-04 */
#define SENSOR_CONNECTOR_DEMAND   ((sensor_status_t)1u << 1)  /* PA-04 */
#define SENSOR_PED_LATCHED_SIDE_0 ((sensor_status_t)1u << 2)  /* TL-06, NU-03 */
#define SENSOR_PED_LATCHED_SIDE_1 ((sensor_status_t)1u << 3)
#define SENSOR_PED_LATCHED_SIDE_2 ((sensor_status_t)1u << 4)
#define SENSOR_PED_LATCHED_SIDE_3 ((sensor_status_t)1u << 5)
#define SENSOR_QUEUE_WARNING      ((sensor_status_t)1u << 6)  /* CC-01 */

/*
 * Reason a COMMAND was rejected with NACK. Kept separate from a free-text
 * string so C1's HMI/logger can filter and count reasons programmatically;
 * a short human-readable string still travels in fault_report_payload_t
 * for the cases that need one.
 */
typedef enum {
    NACK_REASON_NONE = 0,
    NACK_REASON_INVALID_DURATION,          /* PA-11: override duration missing, non-positive, or over the 5 min cap */
    NACK_REASON_RAILWAY_CONFLICT,          /* CC-02: requested movement is toward a non-open crossing */
    NACK_REASON_PEDESTRIAN_ACTIVE,         /* UC-08 (SD-07): pedestrian clearance active and cannot be safely deferred */
    NACK_REASON_FAULT_ACTIVE,              /* PA-09: local fault means the request would violate a safety invariant */
    NACK_REASON_STALE_OR_UNSAFE_PROFILE,   /* UC-03 Alt 3.1 / TL-01: offset or timing violates a local safety bound */
    NACK_REASON_OUT_OF_RANGE,
    NACK_REASON_UNKNOWN_TARGET
} nack_reason_t;

#endif /* SYS_TYPES_H */
