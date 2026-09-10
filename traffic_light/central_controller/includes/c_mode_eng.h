#ifndef C_MODE_ENG_H
#define C_MODE_ENG_H

#include <stdint.h>

#include "sys_types.h"
#include "ipc_msg.h"

/*
 * Central's mode/timing/override DECISION layer (DP-01/DP-02, TC-01..05,
 * PA-11/PA-12).
 *
 * Same separation of concerns as rlx_fsm.h: this is pure state + decision
 * logic. It never calls MsgSend()/ipc_client_post() itself (see
 * app/shared/README.md "Threading pattern" - only the client thread's
 * queue is allowed to originate an outgoing request). c_main.c (and
 * eventually c_server.c, once it exists) owns all IPC; it calls into these
 * functions to decide what to build/validate, then posts the result itself
 * via ipc_client_post().
 *
 * Explicitly OUT of scope here - left as TODO hooks elsewhere until these
 * files exist:
 *   - heartbeat/staleness tracking                          -> c_watchdog_mon.c
 *   - terminal/HMI rendering (UC-09)                         -> c_hmi.c
 *   - parsing/recording inbound STATUS/HEARTBEAT/
 *     FAULT_REPORT/CROSSING_STATUS                           -> c_server.c
 *   - persistent logging                                     -> c_logger.c
 */

/*
 * DP-01/DP-02: which clock hours use MODE_PEAK_FIXED vs
 * MODE_OFF_PEAK_SENSOR. The exact boundary hour is NOT specified anywhere
 * in STATE_CHARTS.md / system_assumptions_tables.md - this is a genuine
 * documentation gap, confirmed by a separate Doc-Analyst pass, not an
 * oversight here. peak_start_hour/peak_end_hour are therefore a
 * configurable PLACEHOLDER, not a hardcoded assumption dressed up as spec.
 */
typedef struct {
    uint8_t peak_start_hour;   /* PLACEHOLDER - team has not chosen an exact value yet */
    uint8_t peak_end_hour;     /* PLACEHOLDER - team has not chosen an exact value yet */
} c_mode_schedule_t;

/*
 * PLACEHOLDER defaults only - ARBITRARY, NOT a spec value. Unlike the
 * TC-01..05 offsets below (which are real, documented spec constants), do
 * not treat these two as anything other than a stand-in pending an
 * explicit team decision.
 */
#define C_MODE_ENG_DEFAULT_PEAK_START_HOUR 6
#define C_MODE_ENG_DEFAULT_PEAK_END_HOUR   9

/* --- TC-01..05: arterial green-wave offsets. Real spec values. -------- */
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

/*
 * Minimal per-controller view kept by the decision layer only - NOT the
 * full UC-09 monitoring view (last-seen timestamp, connectivity, faults
 * belong to c_watchdog_mon.c/c_server.c once they exist). Just enough for
 * this file to remember what it last told a controller to do, and whether
 * an override is still outstanding so C1 doesn't issue a second
 * REQUEST_OVERRIDE to the same Lx on top of one already pending/active.
 */
typedef struct {
    controller_id_t   id;
    controller_role_t role;
    operating_mode_t  last_commanded_mode;
    uint32_t          last_applied_profile_id;
    uint8_t           override_in_flight;

    /* Recorded from the controller's own STATUS/HEARTBEAT/CROSSING_STATUS
     * reports by c_server.c - distinct from last_commanded_mode/
     * last_applied_profile_id/override_in_flight above, which are C1's own
     * decision-layer bookkeeping ("what C1 last told this controller"), not
     * a record of what the controller actually reported back. Do not
     * conflate the two groups. */
    uint32_t last_reported_mode;             /* an operating_mode_t value */
    uint32_t last_reported_signal_phase;     /* a signal_phase_t value */
    uint32_t last_reported_crossing_state;   /* a crossing_state_t value - shared by MSG_STATUS/MSG_HEARTBEAT's status.crossing_state and standalone MSG_CROSSING_STATUS.state */
    uint32_t last_reported_supervisory_state; /* a supervisory_state_t value */
    fault_flags_t last_reported_faults;
    sensor_status_t last_reported_sensor_status; /* UC-09 "sensor status"; 0 for ROLE_RAILWAY (see sys_types.h) */
    uint32_t last_reported_active_profile_id;
    uint8_t  last_reported_override_active;
    uint32_t last_reported_link_state;       /* a connectivity_state_t value */
    uint64_t last_seen_timestamp_ms;         /* req->timestamp_ms at arrival; will read 0 until a real clock helper exists (known codebase-wide gap) */

    /* PA-07 missed-heartbeat tracking (c_watchdog_mon.c). Ticks, not wall-
     * clock time, since no monotonic-clock helper exists yet: incremented
     * once per 1 s watchdog tick, reset to 0 whenever this controller's
     * c_server_record_status()/record_crossing_status() is called (proof of
     * life). 3 consecutive ticks with no reset = declare unavailable. */
    uint32_t missed_heartbeat_ticks;
    uint8_t  marked_unavailable;   /* edge-triggered latch so the "controller went stale" log line fires once, not every tick */
} c_controller_view_t;

typedef struct {
    c_controller_view_t controllers[9];   /* index 0-5 = L1-L6, index 6-8 = RL1-RL3 (see c_mode_eng_controller_index()) */
    c_mode_schedule_t   schedule;
    uint32_t            next_profile_id;
} c_mode_eng_t;

/* One-time setup: populates every controllers[] slot's id/role, seeds
 * `schedule` with the placeholder defaults above, and starts
 * next_profile_id at 1. */
void c_mode_eng_init(c_mode_eng_t *eng);

/* Maps CTRL_L1..CTRL_L6 / CTRL_RL1..CTRL_RL3 to an index 0-8 into
 * c_mode_eng_t.controllers. Returns -1 for CTRL_C1, CTRL_UNKNOWN, or any
 * other out-of-range value. */
int c_mode_eng_controller_index(controller_id_t id);

/* DP-01/DP-02: returns MODE_PEAK_FIXED if current_hour falls within
 * [eng->schedule.peak_start_hour, eng->schedule.peak_end_hour), else
 * MODE_OFF_PEAK_SENSOR. current_hour (0-23) is caller-supplied - this file
 * does not read the wall clock itself. */
operating_mode_t c_mode_eng_select_mode(const c_mode_eng_t *eng, uint8_t current_hour);

/*
 * TC-01..05: fills one ipc_request_t per chain entry into out_requests -
 * verb=MSG_SET_TIMING_PROFILE, sender_id=CTRL_C1, target_id=chain[i].id,
 * payload.timing_profile={profile_id, chain[i].offset_ms}. Does NOT call
 * ipc_client_post() itself (kept out of this file to stay a pure decision
 * layer, per app/shared/README.md "Threading pattern") - the caller
 * (c_main.c today, c_server.c eventually) is responsible for actually
 * posting each built request. out_requests must have room for at least
 * chain_len entries (R1_CHAIN/R2_CHAIN in c_mode_eng.c are both length 3).
 * Returns the number of entries written (== chain_len).
 */
int c_mode_eng_build_timing_profile(uint32_t profile_id, const c_arterial_offset_t *chain,
                                     int chain_len, ipc_request_t *out_requests);

/* Returns eng->next_profile_id, then increments it. Starts at 1 (set by
 * c_mode_eng_init()). */
uint32_t c_mode_eng_next_profile_id(c_mode_eng_t *eng);

/*
 * UC-03/TC-01/TC-02: which pre-defined arterial chain to fetch from
 * c_mode_eng_get_chain(). Values match the R1/R2 naming used throughout
 * usecase.md/SEQUENCE_DIAGRAMS.md (SD-03) - not to be confused with
 * controller_id_t.
 */
typedef enum {
    C_ARTERIAL_CHAIN_R1 = 1,   /* L1 -> L3 -> L5 */
    C_ARTERIAL_CHAIN_R2 = 2    /* L2 -> L4 -> L6 */
} c_arterial_chain_id_t;

/*
 * Small addition (Verifier/Core-Engineer note, added when wiring up
 * c_comm.c/c_operator.c): exposes the R1_CHAIN/R2_CHAIN tables that were
 * already defined as file-local statics in c_mode_eng.c, so an operator-
 * triggered UC-03 broadcast can hand the right table straight to
 * c_mode_eng_build_timing_profile() without a second copy of the same
 * {controller_id_t, offset_ms} data living outside this file. Does not
 * change any existing function's signature or behaviour.
 *
 * Returns a pointer to the matching read-only table (3 entries today) and
 * writes its length to *out_len. Returns NULL and sets *out_len to 0 for
 * an unrecognised chain_id - callers must check the return value before
 * dereferencing.
 */
const c_arterial_offset_t *c_mode_eng_get_chain(c_arterial_chain_id_t chain_id, int *out_len);

/*
 * PA-11/PA-12: surface-level format/bounds validation ONLY - target
 * validity, duration bounds, override type. This is NOT a substitute for
 * the target Lx's own deeper validation (railway conflict, pedestrian
 * state, local fault - PA-09), which only that Lx can perform with its own
 * local state.
 *
 * Returns 1 if the request should be forwarded to target_id. Returns 0 if
 * Central rejects it outright without ever contacting the Lx, filling
 * *out_reason in that case.
 */
int c_mode_eng_validate_override_request(controller_id_t target_id, const request_override_payload_t *payload,
                                          nack_reason_t *out_reason);

#endif /* C_MODE_ENG_H */
