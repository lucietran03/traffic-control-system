#ifndef IPC_MSG_H
#define IPC_MSG_H

#include <stdint.h>
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
 *
 * Wire-format rules (see Lecture/Lecture02_Concurrent_Processes.pdf slides
 * 25-26, and Lecture/lap_6/Lab_06_Task1a_server.c / RTS_Lab_Ex_6.pdf Task
 * 1A note - both apply here because Central/Lx/RLx are separate Qnet nodes,
 * possibly different architectures, not threads in one process):
 *   1. A pulse-compatible header (msg_header_t) is the FIRST member of
 *      every struct that can arrive over a channel, request or reply.
 *   2. Every field that crosses a node stays fixed-width (uint8/16/32/64_t).
 *      Plain C enums are NOT used as struct fields - their underlying type
 *      width is implementation-defined - only as named constants; the
 *      field itself is declared uint32_t and commented with which enum it
 *      holds.
 */

/*
 * Mirrors QNX's struct _pulse layout exactly (same field order and sizes),
 * not the real struct _pulse type, because sival_ptr's size differs between
 * 32-bit and 64-bit QNX targets - using the real type would corrupt this
 * struct's layout for whichever architecture didn't define it. This way,
 * when MsgReceive() returns rcvid == 0 (a pulse - _PULSE_CODE_DISCONNECT or
 * a timer pulse landing on this same channel), hdr.code/hdr.scoid still
 * read correctly out of the same buffer. Do not reorder these fields.
 */
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

/* --- message verbs -------------------------------------------------- */

typedef enum {
    MSG_SET_TIMING_PROFILE = 1,  /* C1  -> Lx  (TC-01..TC-05, TL-04)      */
    MSG_SET_MODE,                /* C1  -> Lx  (DP-01, TL-04)             */
    MSG_REQUEST_OVERRIDE,        /* C1  -> Lx  (PA-11, UC-08)             */
    MSG_RENEW_OVERRIDE,          /* C1  -> Lx  (PA-11)                    */
    MSG_CANCEL_OVERRIDE,         /* C1  -> Lx  (PA-11)                    */
    MSG_REQUEST_FAULT_CLEAR,     /* C1  -> RLx (RC-09); C1 -> Lx (SC-03A - see
                                  * lx_fsm_on_request_fault_clear())      */
    MSG_STATUS,                  /* Lx/RLx -> C1 (PA-08, UC-09)           */
    MSG_FAULT_REPORT,            /* RLx -> C1  (RC-10)                    */
    MSG_CROSSING_STATUS,         /* RLx -> Lx (RC-02); RLx -> C1 (SD-04, SD-05, UC-04) */
    MSG_HEARTBEAT                /* Lx/RLx -> C1 (PA-07)                  */
} msg_type_t;

/* Result carried in every ipc_reply_t. */
typedef enum {
    RESULT_ACK = 1,           /* accepted                                */
    RESULT_ACK_PENDING,       /* ACK_ACCEPTED_PENDING - accepted, activation deferred (PA-12) */
    RESULT_NACK,              /* rejected on safety/validity grounds; see nack_reason_t */
    RESULT_ERROR              /* request could not be processed at all (malformed/unrecognised
                                * verb, internal fault) - distinct from a validly-parsed request
                                * that fails a safety check (RESULT_NACK). Matches the
                                * ACCEPTED/ACCEPTED_PENDING/REJECTED/ERROR outcome set in the
                                * design report's ACK_REPLY payload (TeamQNX_A2_InitialDesignReport-1.pdf
                                * Table 31, p.49). */
} msg_result_t;

/* --- request payloads ------------------------------------------------ */

typedef struct {
    uint32_t profile_id;   /* which coordination profile/version is being applied */
    uint32_t offset_ms;    /* this controller's assigned green-wave offset (TC-02) */
} set_timing_profile_payload_t;

typedef struct {
    uint32_t mode;   /* an operating_mode_t value */
} set_mode_payload_t;

typedef struct {
    uint32_t override_type;    /* an override_type_t value; OVERRIDE_CLEAR_ROUTE today */
    uint32_t target_movement;
    uint32_t duration_ms;      /* capped at 300000 ms (PA-11); Lx NACKs if over */
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
    uint32_t role;               /* a controller_role_t value */
    uint32_t mode;                /* an operating_mode_t value; meaningful for ROLE_INTERSECTION */
    uint32_t signal_phase;        /* a signal_phase_t value; meaningful for ROLE_INTERSECTION.
                                    * SD-03 explicitly reports "STATUS(profile active, phase
                                    * state)" and UC-09 requires the monitoring display to show
                                    * "signal ... state" - without this field neither is possible. */
    uint32_t crossing_state;      /* a crossing_state_t value; meaningful for ROLE_RAILWAY */
    uint32_t supervisory_state;   /* a supervisory_state_t value; meaningful for ROLE_INTERSECTION.
                                    * Needed because RAILWAY_PREEMPTION/CENTRAL_OVERRIDE overlay on
                                    * top of `mode` rather than replacing it (SC-03A) - without this
                                    * field, C1's UC-09 status view cannot tell a railway-suppressed
                                    * or overridden Lx apart from one running normally. */
    uint32_t link_state;          /* a connectivity_state_t value */
    fault_flags_t faults;
    sensor_status_t sensor_status; /* UC-09 "sensor status"; meaningful for ROLE_INTERSECTION
                                     * only (see sys_types.h) - 0 for ROLE_RAILWAY. */
    uint32_t active_profile_id;   /* 0 if none applied */
    uint8_t  override_active;     /* 0/1, not bool - keep this struct's size arch-independent */
} status_report_payload_t;

typedef struct {
    status_report_payload_t summary;
} heartbeat_payload_t;

typedef struct {
    uint32_t fault_code;   /* node-specific fault identifier */
    uint8_t  severity;     /* UC-09: "Central displays the fault with the applicable severity" */
    char     detail[64];   /* short human-readable text for c_hmi.c / c_logger.c */
} fault_report_payload_t;

typedef struct {
    uint32_t state;   /* a crossing_state_t value */
} crossing_status_payload_t;

/* --- envelopes --------------------------------------------------------- */

/* hdr MUST stay the first member (see wire-format rules above): it is what
 * lets a receiver tell a real message (rcvid > 0) apart from a pulse
 * (rcvid == 0, e.g. _PULSE_CODE_DISCONNECT or a timer pulse) landing in the
 * same MsgReceive() buffer, and is what a server must EOK for _IO_CONNECT
 * before touching the rest of the struct (see Lab_06_Task1a_server.c). */
typedef struct {
    msg_header_t     hdr;
    uint32_t         verb;         /* a msg_type_t value */
    uint32_t         seq_num;      /* optional: correlation/logging only, not required for correctness */
    uint32_t         sender_id;    /* a controller_id_t value */
    uint32_t         target_id;    /* a controller_id_t value */
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
    msg_header_t hdr;
    uint32_t     result;        /* a msg_result_t value */
    uint32_t     reason;        /* a nack_reason_t value; valid only when result == RESULT_NACK */
    uint64_t     timestamp_ms;
    union {
        status_report_payload_t applied_status; /* e.g. STATUS(profile active, phase state) echoed back */
        /* HEARTBEAT_ACK / STATE_SYNC_ACCEPTED carry no extra payload */
    } payload;
} ipc_reply_t;

#endif /* IPC_MSG_H */
