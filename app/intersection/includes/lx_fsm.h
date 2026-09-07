#ifndef LX_FSM_H
#define LX_FSM_H

#include <stdint.h>
#include <pthread.h>

#include "sys_types.h"
#include "ipc_msg.h"

/*
 * Lx phase/supervisory finite-state machine.
 *
 * Owns exactly the state described in STATE_CHARTS.md SC-01A/SC-01B/SC-03A
 * and system_assumptions_tables.md TL-01/TL-02/TL-03/DP-04/DP-06/PA-11 for
 * one intersection controller: which of the two operating_mode_t "modes"
 * is selected, where the six-phase signal_phase_t cycle currently is, and
 * which supervisory_state_t authority (normal/railway/override/fault) is
 * in charge right now. Actual signal-head actuation (lx_signal.c), sensor
 * debouncing (lx_sensor.c), and outgoing IPC (lx_comm.c) are separate,
 * not-yet-written files - this FSM only decides state, never drives a
 * GPIO/relay and never calls MsgSend()/ipc_client_post() itself.
 *
 * Threading: every lx_fsm_on_*()/lx_fsm_fill_status()/
 * lx_fsm_local_fault_clear() function locks fsm->lock on entry and
 * unlocks before every return. lx_main.c's server thread calls the
 * lx_fsm_on_*() verb/pulse handlers directly from ipc_server_run()'s
 * on_request()/on_pulse() callbacks, which must never block (see
 * app/shared/README.md "Threading pattern") - the lock here is expected
 * to be uncontended/short, same constraint c_main.c's TODO comment notes
 * for its own future locking.
 *
 * Known follow-up items previously listed here have been closed:
 *   - SC-02/TL-05/TL-06/UC-02 pedestrian WALK/FLASHING_DONT_WALK/
 *     DONT_WALK sequencing is implemented (lx_fsm_ped_service_tick_locked()
 *     in lx_fsm.c, driven from the existing 100 ms phase-timer tick).
 *   - CC-03/UC-05/SD-05's post-railway-closure connector drain phase is
 *     implemented (drain_pending/drain_active/drain_extending fields
 *     below, applied inside lx_fsm_on_phase_timer()'s PHASE_CONNECTOR_
 *     GREEN case).
 *   - TC-02/TC-03's green-wave offset is now actually applied to phase
 *     timing (lx_fsm_on_set_timing_profile(), one-time wall-clock
 *     realignment via clock_gettime(CLOCK_REALTIME, ...); see that
 *     function's doc comment in lx_fsm.c for the exact algorithm and its
 *     documented limitations). PA-09's offset sanity bound
 *     (NACK_REASON_STALE_OR_UNSAFE_PROFILE for offset_ms >=
 *     LX_CYCLE_LENGTH_MS) is implemented alongside it.
 */

/*
 * Override sub-state (SC-03B/PA-11). Private to this FSM - it never
 * crosses a Qnet boundary (status_report_payload_t only exposes the
 * coarser override_active bit), so it does not belong in sys_types.h /
 * ipc_msg.h alongside the shared wire contract.
 */
typedef enum {
    OVR_NONE = 0,
    OVR_PENDING_CLEARANCE,  /* valid request, waiting behind an in-progress WALK/FDW clearance */
    OVR_ACTIVE
} lx_override_substate_t;

/*
 * SC-02/TL-05: which step of the WALK -> FLASHING_DONT_WALK -> DONT_WALK
 * sequence is currently running. Private to this FSM for the same reason
 * lx_override_substate_t is - it never crosses a Qnet boundary. No
 * PED_PHASE_DONT_WALK value exists on purpose: DONT_WALK is the resting
 * state, represented simply by PED_PHASE_NONE (no sequence in progress),
 * exactly like signal_phase_t has no separate IDLE value.
 */
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
    /*
     * Elapsed time in the CURRENT phase, in milliseconds. Despite the
     * name (matching the field list this struct was specified with),
     * this counts elapsed time for every phase, not only the two GREEN
     * phases - yellow and all-red need an elapsed counter too, and the
     * struct only carries one such field, so it is reused generically
     * and reset to 0 on every phase transition. During a GREEN phase in
     * MODE_OFF_PEAK_SENSOR it doubles as the demand-extension recheck
     * clock (TL-01/TL-03/DP-04).
     */
    uint32_t                 green_elapsed_ms;
    uint8_t                  arterial_vehicle_demand;
    uint8_t                  connector_vehicle_demand;
    /*
     * NU-03: 4 pedestrian-call sides, latched until served. Judgment
     * call/ASSUMPTION (TL-06 is not literally specified anywhere): since
     * the actual side-to-approach mapping is not defined by any doc,
     * sides [0]/[1] are treated as crossing the CONNECTOR roadway (and
     * are therefore compatible with ARTERIAL_GREEN, which holds the
     * connector approach red), and sides [2]/[3] as crossing the
     * ARTERIAL roadway (compatible with CONNECTOR_GREEN). See the
     * lx_fsm_arterial_ped_compatible()/lx_fsm_connector_ped_compatible()
     * helpers in lx_fsm.c, which compute the two aggregate booleans this
     * FSM actually reasons about from this array.
     */
    uint8_t                  ped_latched[4];
    /*
     * Verifier-audit fix: a same-side press that arrives WHILE that side
     * is already in ped_serving_mask (mid WALK/FLASHING_DONT_WALK) cannot
     * be recorded by re-setting ped_latched[side] - it is already 1 for
     * the entire in-progress sequence and isn't re-read until the
     * sequence completes, at which point lx_fsm_ped_service_tick_locked()
     * unconditionally clears it. Without this flag that new call is
     * silently lost. Set only by lx_fsm_latch_pedestrian_request() when
     * the side is currently being served; consumed once, at sequence
     * completion, to decide whether to re-latch the side for a fresh
     * sequence instead of clearing it.
     */
    uint8_t                  ped_recall[4];
    /*
     * SC-02/TL-05/TL-06/UC-02: drives the WALK/FLASHING_DONT_WALK
     * sequence. Only one of these is ever needed process-wide (not one
     * per side) because only one vehicle-*_GREEN phase is ever active at
     * a time, and ped_serving_mask records exactly which of the 4
     * ped_latched[] sides are compatible with THAT phase and therefore
     * being served together by this one sequence instance - see
     * lx_fsm_ped_service_tick_locked() in lx_fsm.c.
     */
    lx_ped_phase_t           ped_phase;
    uint32_t                 ped_phase_elapsed_ms;
    uint8_t                  ped_serving_mask; /* bitmask, 1u<<side, of ped_latched[] sides being served by ped_phase right now */
    uint8_t                  queue_warning_active;
    /*
     * CC-03/UC-05/SD-05: connector drain-phase bookkeeping. drain_pending
     * is set exactly once, the moment RAILWAY_PREEMPTION ends with
     * queue_warning_active still set (lx_fsm_on_crossing_status()), and
     * consumed exactly once, at the next PHASE_CONNECTOR_GREEN entry
     * (lx_fsm_advance_phase_locked()) - this pairing is what guarantees
     * the drain fires only once per pre-emption episode, not on every
     * connector green afterward. drain_active marks that THIS connector-
     * green instance is the designated drain phase; drain_extending
     * becomes true only once this phase's ordinary PEAK_FIXED/OFF_PEAK_
     * SENSOR exit point has actually been reached, at which point
     * drain_extension_total_ms starts counting the 4 s increments granted
     * beyond that ordinary point, capped at LX_DRAIN_MAX_EXTENSION_MS.
     */
    uint8_t                  drain_pending;
    uint8_t                  drain_active;
    uint8_t                  drain_extending;
    uint32_t                 drain_extension_total_ms;
    supervisory_state_t      supervisory;
    lx_override_substate_t   override_substate;
    uint32_t                 override_target_movement;
    /*
     * Deviation from the originally-specified field: the spec's
     * `override_expiry_ms` implied an absolute wall-clock comparison,
     * but no monotonic-clock helper exists in qnet_utils.h/sys_types.h
     * yet. Replaced with an elapsed-ms countdown, decremented by exactly
     * the phase-timer's 100 ms tick period in lx_fsm_on_phase_timer(),
     * which is simpler for a PoC with no established wall-clock API.
     */
    uint32_t                 override_remaining_ms;
    /*
     * Judgment call / added field (not in the original field list): the
     * duration most recently accepted for the active override, kept
     * separately from override_remaining_ms (which counts down) so that
     * MSG_RENEW_OVERRIDE's "0 = renew original duration" has something
     * to renew to.
     */
    uint32_t                 override_duration_ms;
    /*
     * Judgment call / added field (not in the original field list): set
     * by lx_fsm_ped_service_tick_locked() (in lx_fsm.c) for the whole
     * duration of a WALK/FLASHING_DONT_WALK sequence (both steps - the
     * override must wait out the full clearance, not just WALK).
     * lx_fsm_on_request_override() reads this to decide OVR_ACTIVE vs.
     * OVR_PENDING_CLEARANCE (SC-03B); lx_fsm_on_phase_timer() polls it to
     * know when a queued pending-clearance override can be re-validated
     * and activated.
     */
    uint8_t                  ped_clearance_active;
    connectivity_state_t     link_state;
    uint32_t                 active_profile_id;
    uint32_t                 assigned_offset_ms;
    /*
     * Compliance-audit fix (TC-02/TC-03): set by lx_fsm_on_set_timing_
     * profile() when a new offset is accepted; consumed exactly once by
     * lx_fsm_advance_phase_locked() at the next fresh PHASE_ARTERIAL_GREEN
     * entry, which calls lx_fsm_apply_offset_locked() at that safe
     * boundary instead of correcting whatever green phase happened to
     * already be running (which could otherwise truncate it).
     */
    uint8_t                  offset_apply_pending;
    fault_flags_t            faults;
    pthread_mutex_t          lock;   /* protects this whole struct - taken by every lx_fsm_* function */
} lx_fsm_t;

void lx_fsm_init(lx_fsm_t *fsm, controller_id_t self_id);

/* verb handlers - called from lx_main.c's on_request(), fill *reply */
void lx_fsm_on_set_timing_profile(lx_fsm_t *fsm, const set_timing_profile_payload_t *payload, ipc_reply_t *reply);
void lx_fsm_on_set_mode(lx_fsm_t *fsm, const set_mode_payload_t *payload, ipc_reply_t *reply);
void lx_fsm_on_request_override(lx_fsm_t *fsm, const request_override_payload_t *payload, ipc_reply_t *reply);
void lx_fsm_on_renew_override(lx_fsm_t *fsm, const renew_override_payload_t *payload, ipc_reply_t *reply);
void lx_fsm_on_cancel_override(lx_fsm_t *fsm, ipc_reply_t *reply);
void lx_fsm_on_crossing_status(lx_fsm_t *fsm, const crossing_status_payload_t *payload, ipc_reply_t *reply);

/* --- sensor-input setters (called by lx_sensor.c) ------------------------
 * Distinct naming from the "on_*" verb-handler family on purpose: on_* is
 * reserved for cross-node Qnet dispatch; these are local, intra-process
 * sensor updates with no ipc_reply_t and no supervisory-state side
 * effects, so they do NOT call lx_fsm_check_fault_locked(). Each takes
 * fsm->lock itself, exactly like every other lx_fsm_* function;
 * lx_sensor.c must never touch fsm->arterial_vehicle_demand /
 * connector_vehicle_demand / ped_latched[] / queue_warning_active
 * directly. */

/* PA-04 approach-level vehicle presence. present=1 sets, 0 clears. */
void lx_fsm_set_arterial_vehicle_demand(lx_fsm_t *fsm, uint8_t present);
void lx_fsm_set_connector_vehicle_demand(lx_fsm_t *fsm, uint8_t present);

/* TL-06: latches ped_latched[side]=1 for side in [0,3] (NU-03's 4 sides).
 * Repeated calls for the same side are idempotent. Out-of-range side is a
 * no-op. There is intentionally no "clear" counterpart - only lx_fsm.c
 * itself may clear a latch, at the point it actually finishes serving
 * that side (lx_fsm_ped_service_tick_locked(), called from
 * lx_fsm_on_phase_timer() - see SC-02/TL-05/TL-06). */
void lx_fsm_latch_pedestrian_request(lx_fsm_t *fsm, uint8_t side);

/* CC-01 queue-warning flag - NOT the same as ordinary vehicle demand, used
 * only by drain logic. Two setters (not a toggle) so lx_sensor.c's key
 * table has one dedicated assert key and one dedicated clear key. */
void lx_fsm_set_queue_warning(lx_fsm_t *fsm, uint8_t active);

/* PA-10: called by lx_watchdog.c when the main loop appears to have
 * stalled (no phase-timer tick observed for too long). Sets
 * FAULT_WATCHDOG_TRIP in fsm->faults; the existing
 * lx_fsm_check_fault_locked() machinery (already implemented) takes it
 * from there at the next event - this function does not itself decide
 * supervisory state or drive output, it only reports the trip. */
void lx_fsm_report_watchdog_trip(lx_fsm_t *fsm);

/*
 * Pulse handler - called from lx_main.c's on_pulse() for
 * IPC_PULSE_PHASE_TIMER, which fires every 100 ms fixed (see lx_main.c's
 * ipc_timer_arm() call). Signature deliberately has no now_ms/timestamp
 * parameter: see the "fixed-tick accumulation" design comment above this
 * function's definition in lx_fsm.c for why.
 */
void lx_fsm_on_phase_timer(lx_fsm_t *fsm);

/* Fills the fields this FSM owns in a status report (mode, signal_phase,
 * supervisory_state, active_profile_id, override_active, faults, role).
 * link_state is intentionally left untouched - lx_comm.c (not yet
 * written) owns that field and is responsible for actually sending the
 * populated status out. */
void lx_fsm_fill_status(const lx_fsm_t *fsm, status_report_payload_t *status);

/*
 * MSG_REQUEST_FAULT_CLEAR handler (C1 -> Lx), symmetric to RLx's
 * rlx_fsm_on_fault_clear(). No payload on the wire - same reasoning as
 * RLx's (the envelope's target_id fully identifies which Lx). Test-plan
 * finding: this used to be lx_fsm_local_fault_clear(), a function nothing
 * ever called - an Lx that entered FAULT_SAFE (e.g. via a watchdog trip)
 * had no way back to NORMAL_OPERATION short of a process restart. Unlike
 * RLx's version, there is no physical actuator state to re-verify here
 * (no gate) - clearing is unconditional and idempotent: always ACKs,
 * clears fsm->faults, and only changes supervisory if it was actually
 * FAULT_SAFE.
 */
void lx_fsm_on_request_fault_clear(lx_fsm_t *fsm, ipc_reply_t *reply);

#endif /* LX_FSM_H */
