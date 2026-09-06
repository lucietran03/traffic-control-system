#ifndef IPC_MSG_H
#define IPC_MSG_H

#include <stdint.h>
#include <stdbool.h>
#include "sys_types.h"

/*
 * Shared Qnet IPC contract for the distributed traffic-control system.
 *
 * Scope: this file covers only CROSS-NODE messages (C1 <-> Lx, C1 <-> RLx,
 * RLx -> Lx). Sensor/actuator events local to one process (DEMAND_PRESENT,
 * PED_REQUEST, TRAIN_APPROACHING, QUEUE_WARNING, signal-head commands such
 * as GREEN/YELLOW/WALK) never cross a Qnet boundary and belong in each
 * node's own private header (lx_*.h / rlx_*.h), not here.
 *
 * Every cross-node exchange is a single MsgSend()/MsgReceive()/MsgReply()
 * round trip: the caller sends an ipc_request_t and blocks for an
 * ipc_reply_t. Verb names below match SEQUENCE_DIAGRAMS.md / usecase.md /
 * system_assumptions_tables.md; do not rename without updating those docs.
 */

/* --- message verbs -------------------------------------------------- */

typedef enum {
    MSG_SET_TIMING_PROFILE = 1,  /* C1  -> Lx  (TC-01..TC-05, TL-04)      */
    MSG_SET_MODE,                /* C1  -> Lx  (DP-01, TL-04)             */
    MSG_REQUEST_OVERRIDE,        /* C1  -> Lx  (PA-11, UC-08)             */
    MSG_RENEW_OVERRIDE,          /* C1  -> Lx  (PA-11)                    */
    MSG_CANCEL_OVERRIDE,         /* C1  -> Lx  (PA-11)                    */
    MSG_REQUEST_FAULT_CLEAR,     /* C1  -> RLx (RC-09)                    */
    MSG_STATUS,                  /* Lx/RLx -> C1 (PA-08, UC-09)           */
    MSG_FAULT_REPORT,            /* RLx -> C1  (RC-10)                    */
    MSG_CROSSING_STATUS,         /* RLx -> Lx and RLx -> C1 (RC-02)       */
    MSG_HEARTBEAT                /* Lx/RLx -> C1 (PA-07)                  */
} msg_type_t;

/* Result carried in every ipc_reply_t. */
typedef enum {
    RESULT_ACK = 1,           /* accepted                                */
    RESULT_ACK_PENDING,       /* ACK_ACCEPTED_PENDING - accepted, activation deferred (PA-12) */
    RESULT_NACK               /* rejected; see nack_reason_t in the reply */
} msg_result_t;

/* --- request payloads ------------------------------------------------ */

typedef struct {
    uint32_t profile_id;   /* which coordination profile/version is being applied */
    uint32_t offset_ms;    /* this controller's assigned green-wave offset (TC-02) */
} set_timing_profile_payload_t;

typedef struct {
    operating_mode_t mode;
} set_mode_payload_t;

typedef struct {
    override_type_t type;          /* OVERRIDE_CLEAR_ROUTE today */
    uint32_t        target_movement;
    uint32_t        duration_ms;   /* capped at 300000 ms (PA-11); Lx NACKs if over */
} request_override_payload_t;

typedef struct {
    uint32_t extend_duration_ms;   /* 0 = renew for the original duration */
} renew_override_payload_t;

/* CANCEL_OVERRIDE and REQUEST_FAULT_CLEAR carry no payload: the envelope's
 * sender_id/target_id fully identifies which active override or crossing
 * fault is being addressed. */

/* Reused for both the periodic STATUS report and the body of a HEARTBEAT -
 * PA-08 requires the "complete current state" on reconnect to be the same
 * shape as an ordinary status report. */
typedef struct {
    controller_role_t    role;
    operating_mode_t      mode;            /* meaningful for ROLE_INTERSECTION */
    crossing_state_t      crossing_state;  /* meaningful for ROLE_RAILWAY */
    connectivity_state_t  link_state;
    fault_flags_t         faults;
    uint32_t               active_profile_id; /* 0 if none applied */
    bool                   override_active;
} status_report_payload_t;

typedef struct {
    status_report_payload_t summary;
} heartbeat_payload_t;

typedef struct {
    uint32_t       fault_code;   /* node-specific fault identifier */
    uint8_t        severity;     /* UC-09: "Central displays the fault with the applicable severity" */
    char           detail[64];   /* short human-readable text for c_hmi.c / c_logger.c */
} fault_report_payload_t;

typedef struct {
    crossing_state_t state;
} crossing_status_payload_t;

/* --- envelopes --------------------------------------------------------- */

typedef struct {
    uint32_t         seq_num;      /* optional: correlation/logging only, not required for correctness */
    controller_id_t  sender_id;
    controller_id_t  target_id;
    msg_type_t       type;
    uint64_t         timestamp_ms; /* PA-07 heartbeat cadence, PA-08 staleness, c_logger.c timestamps */
    union {
        set_timing_profile_payload_t timing_profile;
        set_mode_payload_t           mode;
        request_override_payload_t   override_request;
        renew_override_payload_t     override_renew;
        status_report_payload_t      status;
        fault_report_payload_t       fault_report;
        crossing_status_payload_t    crossing_status;
        heartbeat_payload_t          heartbeat;
        /* MSG_CANCEL_OVERRIDE, MSG_REQUEST_FAULT_CLEAR: no payload, field unused */
    } payload;
} ipc_request_t;

typedef struct {
    msg_result_t   result;
    nack_reason_t  reason;         /* valid only when result == RESULT_NACK */
    uint64_t       timestamp_ms;
    union {
        status_report_payload_t applied_status; /* e.g. STATUS(profile active, phase state) echoed back */
        /* HEARTBEAT_ACK / STATE_SYNC_ACCEPTED carry no extra payload */
    } payload;
} ipc_reply_t;

#endif /* IPC_MSG_H */
