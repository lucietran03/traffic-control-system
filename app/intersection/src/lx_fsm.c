#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "sys_types.h"
#include "ipc_msg.h"
#include "lx_fsm.h"
#include "lx_timer.h"
#include "lx_signal.h"

/*
 * Lx phase/supervisory finite-state machine - implementation.
 *
 * See lx_fsm.h for the ownership/threading contract.
 *
 * Timing durations/thresholds and the OFF_PEAK_SENSOR exit-guard formula
 * live in lx_timer.h/.c, not here: this file owns WHICH phase/mode/
 * supervisory state is active and WHAT happens on a transition; lx_timer
 * owns HOW LONG a phase lasts and WHETHER current elapsed-time+demand
 * says it's time to leave. Keeping the timing formula in one pure,
 * lock-free function also removes what used to be two near-duplicate
 * copies of the same guard logic (one per *_GREEN phase below).
 */

/* --- internal helpers (all assume fsm->lock is already held) --------- */

/* Forward declaration: lx_fsm_check_fault_locked() (below) must evict an
 * active override through its own safe clearance before entering
 * FAULT_SAFE (SC-03A), but lx_fsm_terminate_override_locked() is defined
 * further down this file, next to the other override-lifecycle logic it
 * belongs with. */
static void lx_fsm_terminate_override_locked(lx_fsm_t *fsm);

/* Forward declaration: lx_fsm_advance_phase_locked() (below) must apply a
 * pending TC-02/TC-03 offset correction at the exact moment a fresh
 * PHASE_ARTERIAL_GREEN begins (see the "safe boundary" fix in both
 * functions' doc comments), but lx_fsm_apply_offset_locked() is defined
 * further down, next to lx_fsm_on_set_timing_profile() which sets the
 * pending flag it consumes. */
static void lx_fsm_apply_offset_locked(lx_fsm_t *fsm);

/*
 * FAULT_SAFE always wins (STATE_CHARTS.md SC-03A) and must be re-checked
 * on every event, not only when leaving NORMAL_OPERATION - a fault can be
 * raised at any time by lx_watchdog.c (see lx_fsm_report_watchdog_trip());
 * fsm->faults is the hook it sets. Every lx_fsm_on_*() handler and the
 * phase timer call this first, right after taking the lock.
 *
 * Compliance-audit fix: entering FAULT_SAFE from CENTRAL_OVERRIDE must
 * terminate the override through its own safe clearance first (SC-03A:
 * "cancel or terminate override, apply safe outputs") - otherwise the
 * override silently resumes, unterminated, the moment the fault clears,
 * violating the "never automatically resumes" rule that already applies
 * to a railway-interrupted override. Guarding on `supervisory !=
 * SUPERVISORY_FAULT_SAFE` also makes this idempotent: once the fault has
 * been processed once, repeated calls (e.g. every phase-timer tick while
 * still faulted) are a no-op instead of re-logging every 100 ms.
 */
static void lx_fsm_check_fault_locked(lx_fsm_t *fsm)
{
    if (fsm->faults != FAULT_NONE && fsm->supervisory != SUPERVISORY_FAULT_SAFE) {
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
            lx_fsm_terminate_override_locked(fsm);
        }
        fsm->supervisory = SUPERVISORY_FAULT_SAFE;
    }
}

/*
 * TL-06 ASSUMPTION (not literally specified anywhere - see the ped_latched
 * comment in lx_fsm.h): a pedestrian side crossing the CONNECTOR roadway
 * is compatible with ARTERIAL_GREEN (it crosses perpendicular to the
 * currently-red connector approach); ped_latched[0]/[1] are treated as
 * that pair here. The real per-side-to-approach mapping isn't defined by
 * any doc, so this aggregate is a practical stand-in, not a derived fact.
 */
static uint8_t lx_fsm_arterial_ped_compatible_locked(const lx_fsm_t *fsm)
{
    return (uint8_t)(fsm->ped_latched[0] || fsm->ped_latched[1]);
}

/* Mirror of the above: sides [2]/[3] cross the ARTERIAL roadway, so they
 * are compatible with CONNECTOR_GREEN. Same ASSUMPTION as above. */
static uint8_t lx_fsm_connector_ped_compatible_locked(const lx_fsm_t *fsm)
{
    return (uint8_t)(fsm->ped_latched[2] || fsm->ped_latched[3]);
}

/*
 * SC-02/TL-05/TL-06/UC-02/SD-02: runs the WALK -> FLASHING_DONT_WALK ->
 * DONT_WALK sequence, called once per 100 ms tick from
 * lx_fsm_on_phase_timer() (same fixed-tick-accumulation design as every
 * other timing check in that function - no second timing mechanism).
 *
 * Only one sequence instance is ever needed at a time: PHASE_ARTERIAL_
 * GREEN and PHASE_CONNECTOR_GREEN are never active simultaneously, and
 * ped_latched[0]/[1] (arterial-compatible) vs. [2]/[3] (connector-
 * compatible) are each served only by their one compatible phase - so
 * whichever green phase is active determines a single compatible_mask,
 * and every currently-latched side in that mask starts and finishes WALK/
 * FDW together, in lockstep, sharing fsm->ped_phase/ped_phase_elapsed_ms.
 * A side that latches mid-sequence for the SAME phase is picked up the
 * next time a sequence starts (PED_PHASE_NONE), not folded into one
 * already in progress - TL-06's "coalesce repeated presses" only promises
 * one pending request per side, not that it joins an in-flight WALK late.
 *
 * PA-02 ("stays latched, served if compatible, else waits"): if the
 * compatible phase never comes (e.g. FAULT_SAFE holds everything, or
 * RAILWAY_PREEMPTION never admits CONNECTOR_GREEN), this function is
 * simply never called (FAULT_SAFE returns early in
 * lx_fsm_on_phase_timer()) or never sees compatible_mask != 0 (connector
 * green never occurs) - the latch is left untouched either way, exactly
 * matching "remains latched until reachable again".
 *
 * Known PoC-scope limitation: this does not defer a *_GREEN phase's own
 * ordinary exit to protect an in-progress WALK/FDW. PEAK_FIXED's fixed
 * 48 s/30 s green durations comfortably exceed LX_WALK_MS+
 * LX_FLASHING_DONT_WALK_MS in every normal case, and OFF_PEAK_SENSOR's
 * exit guard already treats ped_latched[] as demand (see own_demand in
 * lx_fsm_on_phase_timer()), so the phase will not exit early - but if a
 * request first latches very close to OFF_PEAK_SENSOR's LX_MAX_GREEN_MS
 * ceiling, the forced max-green exit could still truncate an in-progress
 * clearance. Fully closing that edge case would require plumbing a
 * "ped clearance in progress" veto into lx_timer_should_exit_green()'s
 * max-green branch; left as documented future work rather than expanding
 * this pass's scope.
 */
static void lx_fsm_ped_service_tick_locked(lx_fsm_t *fsm)
{
    uint8_t side;

    if (fsm->ped_phase == PED_PHASE_NONE) {
        uint8_t compatible_mask = 0;

        if (fsm->phase == PHASE_ARTERIAL_GREEN) {
            if (fsm->ped_latched[0]) { compatible_mask |= (uint8_t)(1u << 0); }
            if (fsm->ped_latched[1]) { compatible_mask |= (uint8_t)(1u << 1); }
        } else if (fsm->phase == PHASE_CONNECTOR_GREEN) {
            if (fsm->ped_latched[2]) { compatible_mask |= (uint8_t)(1u << 2); }
            if (fsm->ped_latched[3]) { compatible_mask |= (uint8_t)(1u << 3); }
        }

        if (compatible_mask != 0) {
            fsm->ped_serving_mask = compatible_mask;
            fsm->ped_phase = PED_PHASE_WALK;
            fsm->ped_phase_elapsed_ms = 0;
            /* SC-03B trigger: makes OVR_PENDING_CLEARANCE deferral live -
             * see lx_fsm_on_request_override()/lx_fsm_on_phase_timer(). */
            fsm->ped_clearance_active = 1;
            for (side = 0; side < 4; side++) {
                if (compatible_mask & (uint8_t)(1u << side)) {
                    lx_signal_show_walk(fsm->self_id, side);
                }
            }
        }
        return;
    }

    fsm->ped_phase_elapsed_ms += LX_PHASE_TICK_MS;

    if (fsm->ped_phase == PED_PHASE_WALK) {
        if (fsm->ped_phase_elapsed_ms >= LX_WALK_MS) {
            fsm->ped_phase = PED_PHASE_FLASHING_DONT_WALK;
            fsm->ped_phase_elapsed_ms = 0;
            for (side = 0; side < 4; side++) {
                if (fsm->ped_serving_mask & (uint8_t)(1u << side)) {
                    lx_signal_show_flashing_dont_walk(fsm->self_id, side);
                }
            }
        }
    } else { /* PED_PHASE_FLASHING_DONT_WALK */
        if (fsm->ped_phase_elapsed_ms >= LX_FLASHING_DONT_WALK_MS) {
            for (side = 0; side < 4; side++) {
                if (fsm->ped_serving_mask & (uint8_t)(1u << side)) {
                    lx_signal_show_dont_walk(fsm->self_id, side);
                    if (fsm->ped_recall[side]) {
                        /* A new press for this side arrived mid-sequence
                         * (see ped_recall's doc comment in lx_fsm.h) -
                         * keep it latched so it starts a brand-new
                         * WALK/FDW the next time this phase is reachable,
                         * instead of silently dropping the request. */
                        fsm->ped_recall[side] = 0;
                    } else {
                        fsm->ped_latched[side] = 0; /* TL-06: cleared only now, sequence fully complete */
                    }
                }
            }
            fsm->ped_serving_mask = 0;
            fsm->ped_phase = PED_PHASE_NONE;
            fsm->ped_phase_elapsed_ms = 0;
            fsm->ped_clearance_active = 0; /* releases any OVR_PENDING_CLEARANCE waiting behind this */
        }
    }
}

/*
 * Runs the (placeholder) safe-clearance sequence and returns supervisory
 * authority to NORMAL_OPERATION, used identically by cancel, expiry, and
 * a railway pre-emption that must first evict an active CENTRAL_OVERRIDE
 * (SC-03A/SC-03B). Never truncates a pedestrian phase itself - that
 * belongs to lx_signal.c, which doesn't exist yet, so this only flips
 * FSM state and logs a placeholder action.
 */
static void lx_fsm_terminate_override_locked(lx_fsm_t *fsm)
{
    /* TODO(lx_signal.c): drive the actual safe-clearance signal sequence
     * for the overridden movement before resuming normal sequencing;
     * this FSM only owns state, not actuation. */
    lx_signal_show_override_clearance(fsm->self_id);
    fsm->override_substate = OVR_NONE;
    fsm->override_remaining_ms = 0;
    if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
        fsm->supervisory = SUPERVISORY_NORMAL_OPERATION;
    }
}

/*
 * Advances fsm->phase to the next step in the fixed six-phase cycle
 * (PHASE_ARTERIAL_GREEN -> ... -> PHASE_ALL_RED_B_TO_A -> repeat) and
 * resets green_elapsed_ms for the new phase. The two ALL_RED phases are
 * the only points where (a) a pending MSG_SET_MODE switch is actually
 * applied (SC-01A: never reset to arterial, just carry the new mode into
 * whichever green comes next) and (b) an active RAILWAY_PREEMPTION holds
 * red instead of admitting a green phase (CC-02).
 */
static void lx_fsm_advance_phase_locked(lx_fsm_t *fsm)
{
    switch (fsm->phase) {
    case PHASE_ARTERIAL_GREEN:
        fsm->phase = PHASE_ARTERIAL_YELLOW;
        break;
    case PHASE_ARTERIAL_YELLOW:
        fsm->phase = PHASE_ALL_RED_A_TO_B;
        break;
    case PHASE_ALL_RED_A_TO_B:
        /* boundary_after_arterial: apply a pending mode switch here, then
         * continue into CONNECTOR_GREEN either way (SC-01A). */
        if (fsm->mode_change_pending) {
            fsm->mode = fsm->pending_mode;
            fsm->mode_change_pending = 0;
        }
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_ACTIVE) {
            /*
             * UC-08/SD-07 fix: force the override's requested movement at
             * this boundary instead of letting the ordinary cycle pick the
             * next green - this and the equivalent check at
             * PHASE_ALL_RED_B_TO_A below are what actually make an
             * "active" override do something, instead of just being
             * bookkeeping. Never skips the yellow/all-red clearance that
             * got us here - only decides which green comes next, exactly
             * like the RAILWAY_PREEMPTION branch just below (mutually
             * exclusive with it by construction: lx_fsm_on_crossing_
             * status() evicts any active override before entering
             * RAILWAY_PREEMPTION, and lx_fsm_on_request_override() NACKs a
             * new override while RAILWAY_PREEMPTION is active).
             */
            fsm->phase = (fsm->override_target_movement == (uint32_t)OVERRIDE_MOVEMENT_CONNECTOR)
                             ? PHASE_CONNECTOR_GREEN : PHASE_ARTERIAL_GREEN;
            break;
        }
        if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) {
            /*
             * CC-02/NU-05: a railway crossing only ever sits on a
             * connector road, so suppressing "toward-crossing" means
             * skipping CONNECTOR_GREEN and going straight back to
             * PHASE_ARTERIAL_GREEN - arterial keeps cycling normally for
             * the whole pre-emption instead of admitting a connector
             * green. Audit fix: a bare `break` here left fsm->phase
             * permanently stuck at PHASE_ALL_RED_A_TO_B (nothing else
             * ever re-evaluates this switch on its own), freezing the
             * ENTIRE intersection at all-red for the rest of the
             * pre-emption instead of just suppressing the connector.
             */
            fsm->phase = PHASE_ARTERIAL_GREEN;
            break;
        }
        /*
         * CC-03/UC-05/SD-05: consume a pending drain request exactly here,
         * at entry to the connector green that immediately follows a
         * railway reopening (drain_pending is only ever set in
         * lx_fsm_on_crossing_status() at the RAILWAY_PREEMPTION ->
         * NORMAL_OPERATION edge - see the field comment in lx_fsm.h).
         * Consuming it here, once, is what guarantees the drain applies
         * to exactly one connector green per pre-emption episode.
         */
        if (fsm->drain_pending) {
            fsm->drain_pending = 0;
            fsm->drain_active = 1;
            fsm->drain_extending = 0;
            fsm->drain_extension_total_ms = 0;
        }
        fsm->phase = PHASE_CONNECTOR_GREEN;
        break;
    case PHASE_CONNECTOR_GREEN:
        fsm->phase = PHASE_CONNECTOR_YELLOW;
        break;
    case PHASE_CONNECTOR_YELLOW:
        fsm->phase = PHASE_ALL_RED_B_TO_A;
        break;
    case PHASE_ALL_RED_B_TO_A:
        /* boundary_after_connector: apply a pending mode switch here too
         * (SC-01B: both boundaries independently check for one) - fixed
         * after a re-verification pass caught that this got dropped when
         * the RAILWAY_PREEMPTION handling below was removed. */
        if (fsm->mode_change_pending) {
            fsm->mode = fsm->pending_mode;
            fsm->mode_change_pending = 0;
        }
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_ACTIVE) {
            /* UC-08/SD-07 fix - see the matching check in
             * PHASE_ALL_RED_A_TO_B above for the full rationale. Unlike
             * RAILWAY_PREEMPTION (which never suppresses here, since
             * arterial must keep running during a crossing closure), an
             * override FOR the connector movement legitimately does need
             * to suppress this boundary's default arterial hand-back. */
            fsm->phase = (fsm->override_target_movement == (uint32_t)OVERRIDE_MOVEMENT_CONNECTOR)
                             ? PHASE_CONNECTOR_GREEN : PHASE_ARTERIAL_GREEN;
            break;
        }
        /* NEVER suppressed by RAILWAY_PREEMPTION - a railway crossing
         * only ever sits on a connector road (NU-05: RC1-RC3 cross R3-R5,
         * never R1/R2), so "toward-crossing" always means the connector
         * approach. Arterial (cross-traffic) service must continue
         * normally during pre-emption (CC-02: "compatible away-from-
         * crossing and cross-traffic movements may be prioritised";
         * SD-05: "continue non-conflicting movements" is unconditional).
         * Compliance-audit fix: this branch previously also held red
         * here, incorrectly suppressing arterial too. */
        fsm->phase = PHASE_ARTERIAL_GREEN;
        break;
    default:
        /* Unreachable given signal_phase_t's 6 values; fail safe-ish by
         * re-entering the cycle at a red phase rather than a green one. */
        fsm->phase = PHASE_ALL_RED_A_TO_B;
        break;
    }
    fsm->green_elapsed_ms = 0;
    if (fsm->phase == PHASE_ARTERIAL_GREEN && fsm->offset_apply_pending) {
        /* Safe-boundary point for TC-02/TC-03: this fresh green has shown
         * no time yet, so adjusting green_elapsed_ms here can never
         * truncate an in-progress phase - see lx_fsm_apply_offset_locked(). */
        fsm->offset_apply_pending = 0;
        lx_fsm_apply_offset_locked(fsm);
    }
    lx_signal_show_phase(fsm->self_id, fsm->phase);
}

/* --- lifecycle --------------------------------------------------------- */

void lx_fsm_init(lx_fsm_t *fsm, controller_id_t self_id)
{
    pthread_mutex_init(&fsm->lock, NULL);

    pthread_mutex_lock(&fsm->lock);
    fsm->self_id = self_id;
    /* Judgment call: no document specifies a cold-start mode; MODE_PEAK_
     * FIXED is chosen as the simpler, non-sensor-dependent baseline until
     * C1 sends MSG_SET_MODE (TL-04). */
    fsm->mode = MODE_PEAK_FIXED;
    fsm->mode_change_pending = 0;
    fsm->pending_mode = MODE_PEAK_FIXED;
    /* Judgment call: signal_phase_t has no IDLE value, so the cycle has
     * to start somewhere; PHASE_ARTERIAL_GREEN is the first phase in the
     * documented cycle order (SC-01B). */
    fsm->phase = PHASE_ARTERIAL_GREEN;
    fsm->green_elapsed_ms = 0;
    fsm->arterial_vehicle_demand = 0;
    fsm->connector_vehicle_demand = 0;
    memset(fsm->ped_latched, 0, sizeof(fsm->ped_latched));
    memset(fsm->ped_recall, 0, sizeof(fsm->ped_recall));
    fsm->ped_phase = PED_PHASE_NONE;
    fsm->ped_phase_elapsed_ms = 0;
    fsm->ped_serving_mask = 0;
    fsm->queue_warning_active = 0;
    fsm->drain_pending = 0;
    fsm->drain_active = 0;
    fsm->drain_extending = 0;
    fsm->drain_extension_total_ms = 0;
    fsm->supervisory = SUPERVISORY_NORMAL_OPERATION;
    fsm->override_substate = OVR_NONE;
    fsm->override_target_movement = 0;
    fsm->override_duration_ms = 0;
    fsm->override_remaining_ms = 0;
    fsm->ped_clearance_active = 0;
    /* Judgment call: not yet connected to C1 at process start-up;
     * lx_comm.c (not yet written) will update this once it exists. */
    fsm->link_state = LINK_DEGRADED_LOCAL;
    fsm->active_profile_id = 0;
    fsm->assigned_offset_ms = 0;
    fsm->offset_apply_pending = 0;
    fsm->faults = FAULT_NONE;
    lx_signal_show_phase(fsm->self_id, fsm->phase);
    pthread_mutex_unlock(&fsm->lock);
}

/* --- sensor-input setters (called by lx_sensor.c) ----------------------- */

void lx_fsm_set_arterial_vehicle_demand(lx_fsm_t *fsm, uint8_t present)
{
    pthread_mutex_lock(&fsm->lock);
    fsm->arterial_vehicle_demand = present ? 1u : 0u;
    pthread_mutex_unlock(&fsm->lock);
}

void lx_fsm_set_connector_vehicle_demand(lx_fsm_t *fsm, uint8_t present)
{
    pthread_mutex_lock(&fsm->lock);
    fsm->connector_vehicle_demand = present ? 1u : 0u;
    pthread_mutex_unlock(&fsm->lock);
}

/*
 * KNOWN LIMITATION (PA-03/UC-02 alt-flow 2.2, found by a compliance audit,
 * not yet closed): repeated presses for the same side correctly coalesce
 * into one latched request (idempotent - see below), but there is no
 * "stuck active beyond a diagnostic timeout" detection here, so
 * FAULT_PED_BUTTON_STUCK (sys_types.h) is never set by this file. A button
 * physically stuck closed is indistinguishable from a normal held/repeated
 * press in this simulation - closing this would need a real elapsed-time
 * measurement of how long a side has been (re-)latched without being
 * served, which isn't wired up. Left as documented future work, same
 * category as the already-accepted "stuck vehicle/train sensor" gaps.
 */
void lx_fsm_latch_pedestrian_request(lx_fsm_t *fsm, uint8_t side)
{
    pthread_mutex_lock(&fsm->lock);
    if (side < 4) {
        if (fsm->ped_serving_mask & (uint8_t)(1u << side)) {
            /* Already mid WALK/FLASHING_DONT_WALK for this side - see
             * ped_recall's doc comment in lx_fsm.h for why this can't
             * just be another ped_latched[side]=1 (it's already 1). */
            fsm->ped_recall[side] = 1;
        }
        fsm->ped_latched[side] = 1;
    }
    pthread_mutex_unlock(&fsm->lock);
}

void lx_fsm_set_queue_warning(lx_fsm_t *fsm, uint8_t active)
{
    pthread_mutex_lock(&fsm->lock);
    fsm->queue_warning_active = active ? 1u : 0u;
    pthread_mutex_unlock(&fsm->lock);
}

/*
 * Compliance-audit fix: originally this only OR'd the fault bit and
 * relied on lx_fsm_check_fault_locked() - called at the top of every
 * lx_fsm_on_*()/lx_fsm_on_phase_timer() - to notice it and transition to
 * FAULT_SAFE later. That is not a real dead-man's-switch: if the server
 * thread that runs those functions is the thing that's actually hung,
 * nothing would ever call check_fault_locked() again to notice the flag,
 * and lx_signal_apply_fault_safe() (only invoked from inside
 * lx_fsm_on_phase_timer()'s FAULT_SAFE branch) would never fire either.
 * A genuinely total hang would defeat the watchdog at the exact moment
 * it exists to catch.
 *
 * Fixed to match rlx_fsm_report_watchdog_trip()'s pattern: this function
 * is called FROM the watchdog thread itself (lx_watchdog.c), so it must
 * force the transition and the safe-output action directly here, under
 * its own lock acquisition - independent of whether the server thread
 * ever runs another event again.
 */
void lx_fsm_report_watchdog_trip(lx_fsm_t *fsm)
{
    pthread_mutex_lock(&fsm->lock);
    fsm->faults |= FAULT_WATCHDOG_TRIP;
    if (fsm->supervisory != SUPERVISORY_FAULT_SAFE) {
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
            lx_fsm_terminate_override_locked(fsm);
        }
        fsm->supervisory = SUPERVISORY_FAULT_SAFE;
        lx_signal_apply_fault_safe(fsm->self_id);
    }
    pthread_mutex_unlock(&fsm->lock);
}

/* --- verb handlers ------------------------------------------------------ */

/*
 * TC-02/TC-03: wall-clock realignment of the arterial-green start boundary
 * to `fsm->assigned_offset_ms` within the fixed 90 s PEAK_FIXED cycle
 * (LX_CYCLE_LENGTH_MS), so that Lx controllers started independently on
 * different Qnet nodes still produce a real green-wave without sharing any
 * IPC beyond the offset value itself.
 *
 * Compliance-audit fix: this used to be called directly from
 * lx_fsm_on_set_timing_profile(), correcting whichever PHASE_ARTERIAL_GREEN
 * happened to already be running - which could nudge (and in a bad case,
 * nearly zero out) the REMAINING time of a green phase currently being
 * shown, contradicting UC-03/SD-03's "applies its offset at a safe phase
 * boundary" and the general "never unsafely truncate an active phase or
 * clearance" postcondition. Fixed to be deferred: lx_fsm_on_set_timing_
 * profile() now only sets fsm->offset_apply_pending, and this function is
 * called from lx_fsm_advance_phase_locked() at the exact moment a FRESH
 * PHASE_ARTERIAL_GREEN begins (green_elapsed_ms already reset to 0, no
 * green time shown yet for this instance) - so the correction sets up the
 * upcoming green's start point instead of ever truncating one already in
 * progress, the same "wait for a safe boundary" pattern already used for
 * mode_change_pending (SC-01A/TL-04).
 *
 * Algorithm actually implemented (deliberately simple - see the
 * limitations below):
 *   1. Only runs when fsm->mode == MODE_PEAK_FIXED (TC-02/TC-03 only
 *      coordinate the fixed-cycle arterial chains; OFF_PEAK_SENSOR has no
 *      fixed cycle for an offset to be relative to - explicit scope
 *      decision) AND only when fsm->phase == PHASE_ARTERIAL_GREEN (always
 *      true at the one call site, kept as a defensive check).
 *   2. Reads wall-clock "now" via clock_gettime(CLOCK_REALTIME, ...) -
 *      CLOCK_REALTIME specifically, not CLOCK_MONOTONIC, because
 *      independently-started processes only share a common time
 *      reference through the epoch; a monotonic clock's zero point is
 *      per-process/per-boot and useless for this.
 *   3. Reconstructs, approximately, the wall-clock instant this
 *      just-started arterial-green phase began: `now - fsm->green_elapsed_ms`
 *      (green_elapsed_ms is 0 at the one call site, so this reduces to
 *      `now`, but the general form is kept in case a future caller applies
 *      this mid-phase again under some other safe condition).
 *   4. Compares that actual start (reduced modulo the cycle length)
 *      against the ideal start implied by assigned_offset_ms (also
 *      reduced modulo the cycle length), taking the signed shortest-path
 *      difference around the cycle.
 *   5. Nudges fsm->green_elapsed_ms by exactly that signed error: a phase
 *      that started too LATE has its remaining time shortened (so the
 *      *next* arterial-green start moves earlier, toward the plan); one
 *      that started too EARLY has its remaining time lengthened. This
 *      reuses the exact same tick-accumulator lx_fsm_on_phase_timer()
 *      already compares against LX_PEAK_ARTERIAL_GREEN_MS every tick, so
 *      no second timing mechanism is introduced - the correction is
 *      "spent" exactly once, right here, not re-applied every tick.
 *   6. Clamped so the correction itself can never make the phase appear
 *      to have already expired (which would fire an unrelated,
 *      un-cleared advance on the very next tick) - capped just under the
 *      fixed duration; the ordinary tick handler is still what actually
 *      fires the transition.
 *
 * Known, explicitly accepted limitations of this simpler PoC approach
 * (a fuller alternative is noted below for future work, but NOT
 * implemented here, to keep this pass bounded):
 *   - The correction only takes effect at the NEXT fresh PHASE_ARTERIAL_
 *     GREEN entry after a profile is accepted, never the one already
 *     running when SET_TIMING_PROFILE arrives - by design (see the
 *     compliance-audit fix note above). This means realignment can lag by
 *     up to one full cycle after a new profile is accepted. This is an
 *     accepted gap, not a bug: TC-05 already establishes that "the next
 *     valid timing profile re-establishes the ordinary offset" instead of
 *     a dedicated resynchronisation state machine.
 *   - This is a single nudge, not a converging control loop - if the
 *     local clock or a prior nudge left significant residual error, one
 *     correction may not fully close it in one cycle; repeated profile
 *     applications converge it further over time.
 *   - Does not compensate for a profile that arrives when the phase is
 *     already more than one full cycle stale/mid-transition in an
 *     unusual way (e.g. long FAULT_SAFE/RAILWAY_PREEMPTION outage) beyond
 *     what the modulo-cycle arithmetic naturally handles.
 *   - FUTURE WORK (not implemented): a more thorough approach would track
 *     a running per-tick wall-clock resync (comparing `now` against the
 *     planned boundary on every phase-timer tick, not just at profile-
 *     apply time) so drift occurring between SET_TIMING_PROFILE messages
 *     is continuously corrected rather than only at the moments C1
 *     happens to (re-)send a profile.
 */
static void lx_fsm_apply_offset_locked(lx_fsm_t *fsm)
{
    struct timespec ts;
    uint64_t        now_ms;
    uint32_t        target_phase_in_cycle;
    uint32_t        actual_start_phase_in_cycle;
    int32_t         error_ms;
    uint32_t        fixed_dur;

    if (fsm->mode != MODE_PEAK_FIXED || fsm->phase != PHASE_ARTERIAL_GREEN) {
        return;
    }
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return; /* no clock available - leave timing exactly as-is */
    }

    now_ms = (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000);
    target_phase_in_cycle = (uint32_t)(fsm->assigned_offset_ms % LX_CYCLE_LENGTH_MS);
    /* actual_start = now - green_elapsed_ms, all reduced mod cycle length;
     * add LX_CYCLE_LENGTH_MS before the final mod to stay non-negative
     * without relying on unsigned wraparound semantics being "correct". */
    actual_start_phase_in_cycle =
        (uint32_t)(((now_ms % LX_CYCLE_LENGTH_MS) + LX_CYCLE_LENGTH_MS -
                    (fsm->green_elapsed_ms % LX_CYCLE_LENGTH_MS)) % LX_CYCLE_LENGTH_MS);

    error_ms = (int32_t)actual_start_phase_in_cycle - (int32_t)target_phase_in_cycle;
    if (error_ms > (int32_t)(LX_CYCLE_LENGTH_MS / 2)) {
        error_ms -= (int32_t)LX_CYCLE_LENGTH_MS;
    } else if (error_ms < -(int32_t)(LX_CYCLE_LENGTH_MS / 2)) {
        error_ms += (int32_t)LX_CYCLE_LENGTH_MS;
    }

    if (error_ms > 0) {
        /* started too late -> shorten remaining green so the next start
         * comes sooner. */
        fsm->green_elapsed_ms += (uint32_t)error_ms;
    } else if (error_ms < 0) {
        /* started too early -> lengthen remaining green. */
        uint32_t delay = (uint32_t)(-error_ms);
        fsm->green_elapsed_ms = (delay < fsm->green_elapsed_ms) ? (fsm->green_elapsed_ms - delay) : 0;
    }

    /* Never let the correction itself appear to already satisfy (or
     * exceed) the fixed duration - that belongs to the ordinary tick
     * check next time lx_fsm_on_phase_timer() runs, not to this one-time
     * adjustment. */
    fixed_dur = lx_timer_peak_green_duration_ms(fsm->phase);
    if (fixed_dur > 0 && fsm->green_elapsed_ms >= fixed_dur) {
        fsm->green_elapsed_ms = fixed_dur - 1;
    }
}

void lx_fsm_on_set_timing_profile(lx_fsm_t *fsm, const set_timing_profile_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_FAULT_ACTIVE;
    } else if (payload->offset_ms >= LX_CYCLE_LENGTH_MS) {
        /*
         * PA-09: a safety-relevant sanity bound derivable directly from
         * the cycle length itself, with no further spec guidance needed -
         * an offset >= the cycle length is meaningless (it would just
         * alias to some smaller in-cycle offset anyway) and is rejected
         * outright rather than silently reduced modulo the cycle, so a
         * clearly-malformed profile is visible to C1 as a NACK instead of
         * being quietly reinterpreted.
         */
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_STALE_OR_UNSAFE_PROFILE;
    } else {
        fsm->active_profile_id = payload->profile_id;
        fsm->assigned_offset_ms = payload->offset_ms;
        /* TC-02/TC-03: make the accepted offset actually affect phase
         * timing, but only at the next safe boundary (the start of a
         * fresh PHASE_ARTERIAL_GREEN) - never mid-phase. See
         * lx_fsm_apply_offset_locked()'s doc comment for the exact
         * algorithm, scope (MODE_PEAK_FIXED only), and limitations. */
        fsm->offset_apply_pending = 1;
        reply->result = RESULT_ACK;
    }
    pthread_mutex_unlock(&fsm->lock);
}

void lx_fsm_on_set_mode(lx_fsm_t *fsm, const set_mode_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_FAULT_ACTIVE;
    } else if ((operating_mode_t)payload->mode == fsm->mode) {
        /*
         * Judgment call: SC-01A says reply ACK if "genuinely idle", but
         * signal_phase_t has no IDLE value - this FSM can never observe
         * an idle phase. The closest analogous no-op is a mode change
         * that requests the mode already in effect: applied immediately
         * (no boundary wait needed) rather than deferred.
         */
        fsm->mode_change_pending = 0;
        reply->result = RESULT_ACK;
    } else {
        /* SC-01A: a phase is (almost) always active, so defer to the
         * next ALL_RED boundary - see lx_fsm_advance_phase_locked(). */
        fsm->pending_mode = (operating_mode_t)payload->mode;
        fsm->mode_change_pending = 1;
        reply->result = RESULT_ACK_PENDING;
    }
    pthread_mutex_unlock(&fsm->lock);
}

void lx_fsm_on_request_override(lx_fsm_t *fsm, const request_override_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
        /*
         * Compliance-audit fix: SC-03A shows CENTRAL_OVERRIDE reachable
         * only from NORMAL_OPERATION via REQUEST_OVERRIDE; extending an
         * already-active/pending override is modelled exclusively through
         * SC-03B's RENEW_OVERRIDE. Without this check, a second
         * REQUEST_OVERRIDE silently overwrote the target/duration and
         * replied ACK, bypassing renewal's revalidation entirely.
         */
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_OUT_OF_RANGE; /* no reason code exists for
                                                    * "an override is already
                                                    * in flight - use RENEW_OVERRIDE
                                                    * instead"; this is the
                                                    * closest available fit. */
    } else if (payload->duration_ms == 0 || payload->duration_ms > LX_OVERRIDE_DURATION_CAP_MS) {
        /* PA-11: (0, 300000] ms. */
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_INVALID_DURATION;
    } else if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) {
        /* CC-02: cannot grant an override that conflicts with an active
         * railway pre-emption. */
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_RAILWAY_CONFLICT;
    } else if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_FAULT_ACTIVE;
    } else if (fsm->ped_clearance_active) {
        /*
         * SC-03B: a conflicting WALK/FLASHING_DONT_WALK sequence is
         * running - never truncate it. Queue the override and reply
         * ACK_PENDING; lx_fsm_on_phase_timer() re-validates and flips
         * this to OVR_ACTIVE once ped_clearance_active drops, which now
         * happens for real: lx_fsm_ped_service_tick_locked() sets it for
         * the full WALK+FLASHING_DONT_WALK duration and clears it the
         * instant that sequence completes.
         */
        fsm->override_substate = OVR_PENDING_CLEARANCE;
        fsm->override_target_movement = payload->target_movement;
        fsm->override_duration_ms = payload->duration_ms;
        fsm->override_remaining_ms = payload->duration_ms;
        fsm->supervisory = SUPERVISORY_CENTRAL_OVERRIDE;
        reply->result = RESULT_ACK_PENDING;
    } else {
        fsm->override_substate = OVR_ACTIVE;
        fsm->override_target_movement = payload->target_movement;
        fsm->override_duration_ms = payload->duration_ms;
        fsm->override_remaining_ms = payload->duration_ms;
        fsm->supervisory = SUPERVISORY_CENTRAL_OVERRIDE;
        reply->result = RESULT_ACK;
    }
    pthread_mutex_unlock(&fsm->lock);
}

void lx_fsm_on_renew_override(lx_fsm_t *fsm, const renew_override_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    if (fsm->supervisory != SUPERVISORY_CENTRAL_OVERRIDE || fsm->override_substate != OVR_ACTIVE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_UNKNOWN_TARGET;
    } else if (payload->extend_duration_ms > LX_OVERRIDE_DURATION_CAP_MS) {
        /* Same 300000 ms cap applies to the new extension (PA-11). Retain
         * the current expiry unchanged on rejection. */
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_INVALID_DURATION;
    } else {
        uint32_t new_duration = (payload->extend_duration_ms == 0)
                                     ? fsm->override_duration_ms
                                     : payload->extend_duration_ms;
        fsm->override_duration_ms = new_duration;
        fsm->override_remaining_ms = new_duration; /* restart expiry timer */
        reply->result = RESULT_ACK;
    }
    pthread_mutex_unlock(&fsm->lock);
}

void lx_fsm_on_cancel_override(lx_fsm_t *fsm, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);
    reply->reason = NACK_REASON_NONE;

    if (fsm->override_substate != OVR_PENDING_CLEARANCE && fsm->override_substate != OVR_ACTIVE) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_UNKNOWN_TARGET;
    } else {
        lx_fsm_terminate_override_locked(fsm); /* -> OVR_NONE, safe clearance, resume NORMAL_OPERATION */
        reply->result = RESULT_ACK;
    }
    pthread_mutex_unlock(&fsm->lock);
}

void lx_fsm_on_crossing_status(lx_fsm_t *fsm, const crossing_status_payload_t *payload, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);
    lx_fsm_check_fault_locked(fsm);

    /* FAULT_SAFE always wins - a crossing update while faulted is
     * recorded nowhere else in this FSM and does not change supervisory
     * authority (SC-03A). */
    if (fsm->supervisory != SUPERVISORY_FAULT_SAFE) {
        crossing_state_t state = (crossing_state_t)payload->state;

        if (state != CROSSING_OPEN) {
            if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE) {
                /* CC-02: cancel the override through its own safe
                 * clearance first, then suppress - never just drop it. */
                lx_fsm_terminate_override_locked(fsm);
            }
            fsm->supervisory = SUPERVISORY_RAILWAY_PREEMPTION;
            /* Actual suppression - holding the toward-crossing approach
             * red once its in-progress minimum green+yellow+all-red
             * completes, never truncated - happens in the ALL_RED
             * boundary check inside lx_fsm_advance_phase_locked(). */
        } else if (fsm->supervisory == SUPERVISORY_RAILWAY_PREEMPTION) {
            /* SC-03A: resume normal mode. Never auto-resumes an
             * interrupted override - it was already cancelled via safe
             * clearance above when pre-emption began. */
            fsm->supervisory = SUPERVISORY_NORMAL_OPERATION;
            /*
             * CC-03/UC-05 step 6-7: reopening with QUEUE_WARNING already
             * active arms exactly one drain episode, consumed at the next
             * PHASE_CONNECTOR_GREEN entry (lx_fsm_advance_phase_locked()).
             * Setting this only at this one RAILWAY_PREEMPTION ->
             * NORMAL_OPERATION transition edge (not on every tick, and not
             * re-armed by a later queue warning during ordinary
             * NORMAL_OPERATION) is what keeps this a once-per-episode
             * event rather than firing on every subsequent connector
             * green - see UC-05 alt flow 6.1 for the "no warning -> skip
             * drain entirely" counterpart, which falls out for free here
             * since drain_pending is simply never set.
             */
            if (fsm->queue_warning_active) {
                fsm->drain_pending = 1;
            }
        }
        /* else: NORMAL_OPERATION + CROSSING_OPEN is already the resting
         * state - no-op. */
    }

    /* RC-02: Lx only ever observes crossing status, it never rejects it. */
    reply->result = RESULT_ACK;
    reply->reason = NACK_REASON_NONE;
    pthread_mutex_unlock(&fsm->lock);
}

/* --- phase timer pulse handler ------------------------------------------ */

/*
 * Design choice: lx_main.c arms IPC_PULSE_PHASE_TIMER once, as a fixed
 * 100 ms periodic timer, and never re-arms it per phase (its own comment
 * flags per-phase re-arming with the next duration as the alternative
 * that was considered). Re-arming would mean cancelling/re-creating a
 * QNX timer on every single phase transition and handling a pulse that
 * races that re-arm - real bookkeeping for no benefit in a 100 ms-tick
 * PoC. Instead this function is a pure fixed-tick accumulator: it adds
 * LX_PHASE_TICK_MS to green_elapsed_ms every call and compares against
 * whichever threshold applies to the current phase/mode. This also means
 * the timer never needs to know the next phase's duration in advance.
 *
 * For the same PoC-simplicity reason, there is no now_ms/timestamp
 * parameter (no monotonic-clock helper exists in the shared headers yet)
 * - every caller ticks this at exactly the timer's own period, so
 * accumulating a constant per call is equivalent to and simpler than
 * passing a wall-clock reading.
 */
void lx_fsm_on_phase_timer(lx_fsm_t *fsm)
{
    pthread_mutex_lock(&fsm->lock);

    lx_fsm_check_fault_locked(fsm);

    if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) {
        /* TODO(lx_signal.c): apply the actual safe-state outputs
         * (all-red/dark per RC-06/PA-10). This FSM only owns state. */
        lx_signal_apply_fault_safe(fsm->self_id);
        pthread_mutex_unlock(&fsm->lock);
        return;
    }

    /*
     * UC-08/SD-07 fix: an active override now actually forces green onto
     * override_target_movement, not just bookkeeping/reporting - see the
     * SUPERVISORY_CENTRAL_OVERRIDE checks in lx_fsm_advance_phase_locked()
     * (which boundary picks the override's requested green) and the top of
     * the PHASE_ARTERIAL_GREEN/PHASE_CONNECTOR_GREEN cases below (which
     * hold that green instead of running the ordinary exit-check, for as
     * long as the override stays OVR_ACTIVE). override_movement_t
     * (sys_types.h) is the judgment-call encoding for target_movement -
     * arterial vs. connector, since no spec document defined one.
     */

    /*
     * Override expiry (SC-03B): elapsed-ms countdown relative to
     * acceptance/renewal rather than an absolute wall-clock deadline -
     * see the override_remaining_ms comment in lx_fsm.h.
     *
     * Compliance-audit fix: SC-03B's OVERRIDE_PENDING state explicitly
     * allows "expires before application" as an exit condition, so the
     * countdown must keep running while OVR_PENDING_CLEARANCE too, not
     * only OVR_ACTIVE - otherwise a request queued behind a pedestrian
     * clearance could never expire while waiting. A pending request that
     * expires is discarded without a second reply (the original
     * ACK_ACCEPTED_PENDING already covers it), same as an activated
     * override that expires is terminated without one.
     */
    if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE &&
        (fsm->override_substate == OVR_ACTIVE || fsm->override_substate == OVR_PENDING_CLEARANCE)) {
        if (fsm->override_remaining_ms <= LX_PHASE_TICK_MS) {
            lx_fsm_terminate_override_locked(fsm); /* same safe-clearance path as cancel */
        } else {
            fsm->override_remaining_ms -= LX_PHASE_TICK_MS;
        }
    }

    fsm->green_elapsed_ms += LX_PHASE_TICK_MS;

    /* SC-02/TL-05/TL-06/UC-02: independent of the phase switch below -
     * once a WALK/FDW sequence is running it must keep counting down even
     * if, in some edge case, the vehicle phase changes underneath it (see
     * the scope-limitation note on lx_fsm_ped_service_tick_locked()). */
    lx_fsm_ped_service_tick_locked(fsm);

    /*
     * SC-03B: re-validate a queued pending-clearance override once the
     * conflicting WALK/FDW sequence finishes. In this design any event
     * that would make the override newly invalid (a railway conflict, a
     * fault) already evicts supervisory out of SUPERVISORY_CENTRAL_
     * OVERRIDE proactively (see lx_fsm_on_crossing_status() and the
     * FAULT_SAFE check above) - so reaching this point still in
     * CENTRAL_OVERRIDE means the override is still valid by
     * construction, and "re-validate" reduces to simply activating it.
     * No second reply is sent - the original ACK_ACCEPTED_PENDING
     * already covers this.
     *
     * Verifier-audit fix: this must run AFTER lx_fsm_ped_service_tick_
     * locked() above, not before it - ped_clearance_active is only ever
     * cleared inside that call (at FDW completion), so checking it earlier
     * in the same tick read last tick's stale value, activating the
     * override one whole tick late (and, worse, letting the ordinary
     * PEAK_FIXED/OFF_PEAK_SENSOR exit-check below fire on the clearance-
     * completing tick before the override's hold guard had a chance to
     * engage - an avoidable extra half-cycle, not a safety issue, but a
     * real logic-ordering defect against this comment's own "re-validate
     * ... reduces to simply activating it" claim).
     */
    if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_PENDING_CLEARANCE) {
        if (!fsm->ped_clearance_active) {
            fsm->override_substate = OVR_ACTIVE;
        }
    }

    switch (fsm->phase) {
    case PHASE_ARTERIAL_YELLOW:
    case PHASE_CONNECTOR_YELLOW:
        if (fsm->green_elapsed_ms >= LX_YELLOW_MS) {
            lx_fsm_advance_phase_locked(fsm);
        }
        break;

    case PHASE_ALL_RED_A_TO_B:
    case PHASE_ALL_RED_B_TO_A:
        if (fsm->green_elapsed_ms >= LX_ALL_RED_MS) {
            lx_fsm_advance_phase_locked(fsm);
        }
        break;

    case PHASE_ARTERIAL_GREEN:
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_ACTIVE &&
            fsm->override_target_movement == (uint32_t)OVERRIDE_MOVEMENT_ARTERIAL) {
            /* UC-08/SD-07: hold this green for the override's duration -
             * override_remaining_ms (counted down above) is what actually
             * ends it, via lx_fsm_terminate_override_locked(). Skipping the
             * ordinary PEAK_FIXED/OFF_PEAK_SENSOR exit-check here is what
             * makes the override win over the normal cycle instead of
             * being silently overridden BY it. */
            break;
        }
        if (fsm->mode == MODE_PEAK_FIXED) {
            /* TL-02: fixed duration, no sensor influence. */
            if (fsm->green_elapsed_ms >= lx_timer_peak_green_duration_ms(fsm->phase)) {
                lx_fsm_advance_phase_locked(fsm);
            }
        } else {
            /* MODE_OFF_PEAK_SENSOR (TL-01/TL-03/DP-04/DP-06). Re-check
             * the exit guard every 4 s, not just once, so a connector
             * demand that arrives mid-green is never missed
             * (anti-starvation, DP-06). Because green_elapsed_ms
             * accumulates continuously, every multiple-of-4000 tick is
             * already exactly 4 s after the last one, whether or not the
             * previous check "extended" - no separate sub-window/reset
             * bookkeeping is needed beyond this modulo check. */
            if ((fsm->green_elapsed_ms % LX_EXTENSION_MS) == 0) {
                uint8_t own_demand   = (uint8_t)(fsm->arterial_vehicle_demand || lx_fsm_arterial_ped_compatible_locked(fsm));
                uint8_t other_demand = (uint8_t)(fsm->connector_vehicle_demand || lx_fsm_connector_ped_compatible_locked(fsm));

                /* requires_other_demand=1: DP-06 - arterial must not exit
                 * without a genuine pending connector request. */
                if (lx_timer_should_exit_green(fsm->green_elapsed_ms, own_demand, other_demand, 1u)) {
                    lx_fsm_advance_phase_locked(fsm);
                }
                /* else: extend - stay on ARTERIAL_GREEN. With no demand
                 * anywhere this also falls through to "stay", i.e. rest
                 * on green indefinitely, per spec (DP-04). */
            }
        }
        break;

    case PHASE_CONNECTOR_GREEN:
        if (fsm->supervisory == SUPERVISORY_CENTRAL_OVERRIDE && fsm->override_substate == OVR_ACTIVE &&
            fsm->override_target_movement == (uint32_t)OVERRIDE_MOVEMENT_CONNECTOR) {
            /* UC-08/SD-07: see the matching guard in PHASE_ARTERIAL_GREEN
             * above. Checked before the CC-03 drain logic below since the
             * two never legitimately overlap - a drain phase only exists
             * after a railway pre-emption, which always evicts any active
             * override first (see lx_fsm_on_crossing_status()) - but this
             * ordering keeps the override's priority explicit either way. */
            break;
        }
        /*
         * CC-03/UC-05/SD-05: once this connector green's ordinary exit
         * point has actually been reached (drain_extending latches true
         * the first time that happens, below), the drain state machine
         * takes over completely and the ordinary PEAK_FIXED/OFF_PEAK_
         * SENSOR duration/demand check is skipped outright - re-running
         * it every tick would, for OFF_PEAK_SENSOR, only re-evaluate at
         * its own every-4th-second checkpoints (a different cadence than
         * drain_extension_total_ms's), which would let drain ticks slip
         * through unevaluated. Keeping the two checks mutually exclusive
         * avoids that entirely.
         */
        if (fsm->drain_active && fsm->drain_extending) {
            if ((fsm->drain_extension_total_ms % LX_EXTENSION_MS) == 0) {
                if (!fsm->queue_warning_active || fsm->drain_extension_total_ms >= LX_DRAIN_MAX_EXTENSION_MS) {
                    /* UC-05 steps 9/9.1: warning cleared, or the 60 s cap
                     * was reached - resume ordinary phase selection. */
                    fsm->drain_active = 0;
                    fsm->drain_extending = 0;
                    fsm->drain_extension_total_ms = 0;
                    lx_fsm_advance_phase_locked(fsm);
                    break;
                }
                /* else: still active and under cap - fall through and
                 * grant one more LX_EXTENSION_MS increment. */
            }
            fsm->drain_extension_total_ms += LX_PHASE_TICK_MS;
            break;
        }

        if (fsm->mode == MODE_PEAK_FIXED) {
            if (fsm->green_elapsed_ms >= lx_timer_peak_green_duration_ms(fsm->phase)) {
                if (fsm->drain_active) {
                    /* This is the designated drain phase (armed in
                     * lx_fsm_advance_phase_locked() on entry) and has now
                     * reached its ordinary fixed-duration exit point -
                     * CC-03 extension takes over starting next tick
                     * instead of transitioning to yellow. */
                    fsm->drain_extending = 1;
                    fsm->drain_extension_total_ms = 0;
                } else {
                    lx_fsm_advance_phase_locked(fsm);
                }
            }
        } else {
            if ((fsm->green_elapsed_ms % LX_EXTENSION_MS) == 0) {
                uint8_t own_demand = (uint8_t)(fsm->connector_vehicle_demand || lx_fsm_connector_ped_compatible_locked(fsm));

                /* requires_other_demand=0: no anti-starvation counterpart
                 * needed here - unlike arterial, connector's exit is not
                 * gated on arterial having pending demand (arterial is
                 * the major road and gets green again next cycle
                 * regardless) - matches the spec's exit guard exactly. */
                if (lx_timer_should_exit_green(fsm->green_elapsed_ms, own_demand, 0u, 0u)) {
                    if (fsm->drain_active) {
                        fsm->drain_extending = 1;
                        fsm->drain_extension_total_ms = 0;
                    } else {
                        lx_fsm_advance_phase_locked(fsm);
                    }
                }
            }
        }
        break;

    default:
        break;
    }

    pthread_mutex_unlock(&fsm->lock);
}

/* --- status reporting / demo shim --------------------------------------- */

void lx_fsm_fill_status(const lx_fsm_t *fsm, status_report_payload_t *status)
{
    /*
     * fsm is declared const per the specified API, but the mutex still
     * has to be taken: lx_main.c's heartbeat pulse (lx_comm.c, not yet
     * written) runs on the same server thread as every lx_fsm_on_*()
     * call and must see a consistent snapshot. The mutex is plumbing, not
     * logically part of the read-only value being observed, so the cast
     * below is the same kind of "mutable via a back door" pattern a
     * mutable member would give in C++.
     */
    pthread_mutex_lock((pthread_mutex_t *)&fsm->lock);

    status->role              = (uint32_t)ROLE_INTERSECTION;
    status->mode              = (uint32_t)fsm->mode;
    status->signal_phase      = (uint32_t)fsm->phase;
    status->crossing_state    = 0; /* not meaningful for ROLE_INTERSECTION (ipc_msg.h) */
    status->supervisory_state = (uint32_t)fsm->supervisory;
    status->faults            = fsm->faults;
    /* UC-09 "sensor status" - packs the same demand flags lx_sensor.c's
     * setters populate into the wire-visible bitmask (sys_types.h). */
    status->sensor_status = SENSOR_NONE;
    if (fsm->arterial_vehicle_demand)  { status->sensor_status |= SENSOR_ARTERIAL_DEMAND; }
    if (fsm->connector_vehicle_demand) { status->sensor_status |= SENSOR_CONNECTOR_DEMAND; }
    if (fsm->ped_latched[0])           { status->sensor_status |= SENSOR_PED_LATCHED_SIDE_0; }
    if (fsm->ped_latched[1])           { status->sensor_status |= SENSOR_PED_LATCHED_SIDE_1; }
    if (fsm->ped_latched[2])           { status->sensor_status |= SENSOR_PED_LATCHED_SIDE_2; }
    if (fsm->ped_latched[3])           { status->sensor_status |= SENSOR_PED_LATCHED_SIDE_3; }
    if (fsm->queue_warning_active)     { status->sensor_status |= SENSOR_QUEUE_WARNING; }
    status->active_profile_id = fsm->active_profile_id;
    status->override_active   = (uint8_t)((fsm->override_substate == OVR_ACTIVE) ? 1u : 0u);
    /* status->link_state intentionally left untouched - see this
     * function's doc comment in lx_fsm.h. */

    pthread_mutex_unlock((pthread_mutex_t *)&fsm->lock);
}

void lx_fsm_on_request_fault_clear(lx_fsm_t *fsm, ipc_reply_t *reply)
{
    /* See the doc comment on this function's declaration in lx_fsm.h. */
    pthread_mutex_lock(&fsm->lock);
    reply->reason = NACK_REASON_NONE;
    fsm->faults = FAULT_NONE;
    if (fsm->supervisory == SUPERVISORY_FAULT_SAFE) {
        fsm->supervisory = SUPERVISORY_NORMAL_OPERATION;
    }
    reply->result = RESULT_ACK;
    pthread_mutex_unlock(&fsm->lock);
}
