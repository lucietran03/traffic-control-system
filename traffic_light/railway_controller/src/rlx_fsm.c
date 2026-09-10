#include <stdio.h>
#include <pthread.h>

#include "rlx_fsm.h"
#include "rlx_timer.h"
#include "rlx_gate.h"
#include "rlx_signal.h"

/*
 * Railway-crossing FSM implementation (STATE_CHARTS.md SC-04A/SC-04B;
 * RC-01, RC-03, RC-04, RC-05, RC-06, RC-09, RC-10, RC-11).
 *
 * Timing durations/thresholds and the occupancy-countdown arithmetic live
 * in rlx_timer.h/.c, not here: this file owns WHICH crossing state is
 * active and WHAT happens on a transition; rlx_timer owns HOW LONG each
 * step lasts and the underflow-safe countdown primitive.
 *
 * Timing design: rlx_main.c arms exactly ONE recurring 1 s pulse
 * (IPC_PULSE_RAILWAY_WARNING, reused as a generic tick rather than its
 * originally-named single purpose) and calls rlx_fsm_on_tick() once per
 * pulse. That single tick drives every threshold in this file - the 5 s
 * warning-to-closing step, the 15 s closing/opening deadlines, and the
 * 20 s occupancy-window countdowns - via state_elapsed_ms and each
 * window's remaining_ms. IPC_PULSE_RAILWAY_OCCUPANCY is intentionally NOT
 * armed as a second pulse: a single 1 s tick is simpler and produces
 * identical behaviour to arming/re-arming a second timer per active
 * window, since all this code needs is "one more second has passed".
 *
 * No real gate hardware exists for this PoC, so gate motion/confirmation
 * is simulated by rlx_gate.c: rlx_gate_command_close()/rlx_gate_command_open()
 * start a timed motion, and rlx_gate_poll_closed()/rlx_gate_poll_open()
 * only read back "confirmed" once that motion completes (and can be armed,
 * via rlx_gate_arm_demo_fault(), to never confirm at all - the RC-06 demo
 * fault path). This file never sets a gate-confirmed flag directly; it
 * only commands motion and polls rlx_gate.c's result.
 */

/* --- internal helpers (caller already holds fsm->lock) ----------------- */

static crossing_state_t map_to_crossing_state(rlx_internal_state_t state)
{
    switch (state) {
    case RLX_OPEN:
        return CROSSING_OPEN;
    case RLX_WARNING:
    case RLX_CLOSING:
    case RLX_RECLOSING:
        /* Compliance-audit fix: RECLOSING is the same "gates commanded
         * down, not yet sensor-confirmed" condition as CLOSING (just
         * entered from OPENING instead of from WARNING) - report it the
         * same way instead of jumping straight to CROSSING_CLOSED before
         * closure is actually confirmed. */
        return CROSSING_WARNING;
    case RLX_CLOSED:
    case RLX_TRAIN_PRESENT:
    case RLX_OPENING:
        return CROSSING_CLOSED;
    case RLX_FAULT:
    default:
        return CROSSING_FAULT;
    }
}

static uint8_t gates_confirmed_closed(void)
{
    return rlx_gate_poll_closed();
}

static uint8_t gates_confirmed_open(void)
{
    return rlx_gate_poll_open();
}

/*
 * Registers (or refreshes) an occupancy window for `direction`.
 * RC-01 only ever defines two rail directions, so at most
 * RLX_MAX_OCCUPANCY_WINDOWS (2) distinct windows can legitimately exist;
 * a repeated TRAIN_APPROACHING for a direction that already has an active
 * window is treated as a re-confirmation (refresh remaining_ms) rather
 * than a new window - this isn't spelled out explicitly in the spec and
 * is a judgment call to keep the slot bookkeeping simple.
 */
static void register_window(rlx_fsm_t *fsm, uint32_t direction, uint32_t initial_remaining_ms)
{
    int i;

    for (i = 0; i < RLX_MAX_OCCUPANCY_WINDOWS; i++) {
        if (fsm->windows[i].active && fsm->windows[i].direction == direction) {
            fsm->windows[i].remaining_ms = initial_remaining_ms;
            return;
        }
    }
    for (i = 0; i < RLX_MAX_OCCUPANCY_WINDOWS; i++) {
        if (!fsm->windows[i].active) {
            fsm->windows[i].active = 1;
            fsm->windows[i].direction = direction;
            fsm->windows[i].remaining_ms = initial_remaining_ms;
            fsm->active_window_count++;
            return;
        }
    }
    /* Both slots already hold the two distinct directions RC-01 models -
     * nothing more to register. Ignored defensively. */
}

static void enter_fault(rlx_fsm_t *fsm, fault_flags_t fault_bit)
{
    /* PA-10/RC-10: a fault must force gates DOWN, not leave them wherever
     * they happened to be (audit fix - this used to only latch the fault
     * flag and print a "gates held as-is" message, so a fault raised while
     * OPEN or mid-OPENING left the crossing physically unprotected).
     * rlx_gate_command_close() is safe to call unconditionally: it just
     * (re)starts a close motion, which is a no-op in outcome if gates are
     * already closed/closing. */
    rlx_gate_command_close();
    rlx_signal_show_fault(fault_bit);
    fsm->state = RLX_FAULT;
    fsm->state_elapsed_ms = 0;
    fsm->faults |= fault_bit;
    fsm->fault_report_pending = 1;
}

/* Shared by CLOSING and RECLOSING: both resolve the same way (RC-03/RC-06). */
static void check_closing_or_reclosing_complete(rlx_fsm_t *fsm)
{
    if (gates_confirmed_closed()) {
        int i;

        /* RC-06 invariant checked directly here, not inferred from
         * elapsed time: PROCEED is granted only because both gate
         * sensors currently read closed. */
        fsm->state = RLX_CLOSED;
        fsm->state_elapsed_ms = 0;
        for (i = 0; i < RLX_MAX_OCCUPANCY_WINDOWS; i++) {
            if (fsm->windows[i].active) {
                rlx_signal_show_train_proceed(fsm->windows[i].direction);
            }
        }
    } else if (fsm->state_elapsed_ms >= RLX_CLOSING_DEADLINE_MS) {
        enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING);
    }
}

static void check_gate_contradiction_closed(rlx_fsm_t *fsm)
{
    if (!gates_confirmed_closed()) {
        enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING);
    }
}

static void check_opening_complete(rlx_fsm_t *fsm)
{
    if (gates_confirmed_open()) {
        rlx_signal_show_flashers_off();
        fsm->state = RLX_OPEN;
        fsm->state_elapsed_ms = 0;
    } else if (fsm->state_elapsed_ms >= RLX_OPENING_DEADLINE_MS) {
        enter_fault(fsm, FAULT_GATE_CONFIRM_MISSING);
    }
}

static void enter_closing(rlx_fsm_t *fsm)
{
    rlx_gate_command_close();
    fsm->state = RLX_CLOSING;
    fsm->state_elapsed_ms = 0;
}

static void enter_reclosing(rlx_fsm_t *fsm, uint32_t direction)
{
    rlx_signal_show_reclosing();
    register_window(fsm, direction, 0);
    rlx_gate_command_close();
    fsm->state = RLX_RECLOSING;
    fsm->state_elapsed_ms = 0;
}

static void enter_train_present(rlx_fsm_t *fsm)
{
    int i;

    /* Every window still waiting on the placeholder "expected arrival"
     * threshold (remaining_ms == 0, registered while in WARNING/CLOSING/
     * CLOSED) starts its real 20s RC-04 countdown now. Windows registered
     * directly via a TRAIN_APPROACHING self-loop while already in
     * TRAIN_PRESENT are given remaining_ms = RLX_OCCUPANCY_WINDOW_MS at
     * registration time instead (see rlx_fsm_simulate_train_approaching),
     * so this loop leaves those untouched. */
    for (i = 0; i < RLX_MAX_OCCUPANCY_WINDOWS; i++) {
        if (fsm->windows[i].active && fsm->windows[i].remaining_ms == 0) {
            fsm->windows[i].remaining_ms = RLX_OCCUPANCY_WINDOW_MS;
        }
    }
    fsm->state = RLX_TRAIN_PRESENT;
    fsm->state_elapsed_ms = 0;
}

static void enter_opening(rlx_fsm_t *fsm)
{
    rlx_signal_show_train_stop();
    rlx_gate_command_open();
    fsm->state = RLX_OPENING;
    fsm->state_elapsed_ms = 0;
}

/* --- public API ---------------------------------------------------------- */

void rlx_fsm_init(rlx_fsm_t *fsm, controller_id_t self_id)
{
    int i;

    /* Verifier-audit fix: initialize the mutex first, matching
     * lx_fsm_init()'s ordering - harmless either way since this always
     * runs single-threaded before pthread_create(), but consistent
     * ordering avoids the question ever needing to be re-asked. */
    pthread_mutex_init(&fsm->lock, NULL);

    fsm->self_id = self_id;
    fsm->state = RLX_OPEN;
    fsm->state_elapsed_ms = 0;
    for (i = 0; i < RLX_MAX_OCCUPANCY_WINDOWS; i++) {
        fsm->windows[i].active = 0;
        fsm->windows[i].direction = 0;
        fsm->windows[i].remaining_ms = 0;
    }
    fsm->active_window_count = 0;
    /* Crossing starts OPEN: gate confirmation state itself lives in
     * rlx_gate.c (see rlx_gate_init(), called separately from rlx_main.c's
     * main()), not in this struct. */
    fsm->faults = FAULT_NONE;
    fsm->fault_report_pending = 0;
    fsm->link_state = LINK_CENTRAL_CONNECTED;
}

void rlx_fsm_simulate_train_approaching(rlx_fsm_t *fsm, uint32_t direction)
{
    pthread_mutex_lock(&fsm->lock);

    switch (fsm->state) {
    case RLX_OPEN:
        rlx_signal_show_flashers_on(direction);
        register_window(fsm, direction, 0);
        fsm->state = RLX_WARNING;
        fsm->state_elapsed_ms = 0;
        break;

    case RLX_WARNING:
    case RLX_CLOSING:
    case RLX_RECLOSING:
        /* Self-loop: additional direction approaching during the same
         * warning/closing/reclosing step. Elapsed timer keeps running. */
        register_window(fsm, direction, 0);
        break;

    case RLX_CLOSED:
        register_window(fsm, direction, 0);
        check_gate_contradiction_closed(fsm);
        if (fsm->state == RLX_CLOSED && gates_confirmed_closed()) {
            rlx_signal_show_train_proceed(direction);
        }
        break;

    case RLX_TRAIN_PRESENT:
        /* Registered directly with a live countdown - the crossing is
         * already occupied, so there's no "expected arrival" wait. */
        register_window(fsm, direction, RLX_OCCUPANCY_WINDOW_MS);
        check_gate_contradiction_closed(fsm);
        if (fsm->state == RLX_TRAIN_PRESENT && gates_confirmed_closed()) {
            rlx_signal_show_train_proceed(direction);
        }
        break;

    case RLX_OPENING:
        enter_reclosing(fsm, direction);
        break;

    case RLX_FAULT:
        /* Latched until rlx_fsm_on_fault_clear() succeeds (RC-09/RC-10);
         * approach events are ignored while faulted. */
        break;

    default:
        break;
    }

    pthread_mutex_unlock(&fsm->lock);
}

void rlx_fsm_on_fault_clear(rlx_fsm_t *fsm, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);

    if (fsm->state != RLX_FAULT) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_UNKNOWN_TARGET;
    } else {
        /* Re-checks rlx_gate.c's live confirmation state at the moment of
         * the clear request, not a value cached earlier in this function
         * (RC-10: Central's request never bypasses live verification). */
        if (gates_confirmed_open()) {
            fsm->state = RLX_OPEN;
            fsm->state_elapsed_ms = 0;
            fsm->faults = FAULT_NONE;
            /* Crossing is verified safe and idle again: drop any stale
             * occupancy bookkeeping left over from before the fault. */
            fsm->active_window_count = 0;
            fsm->windows[0].active = 0;
            fsm->windows[1].active = 0;
            reply->result = RESULT_ACK;
            reply->reason = NACK_REASON_NONE;
        } else {
            reply->result = RESULT_NACK;
            reply->reason = NACK_REASON_FAULT_ACTIVE;
        }
    }

    pthread_mutex_unlock(&fsm->lock);
}

void rlx_fsm_on_tick(rlx_fsm_t *fsm)
{
    int i;

    pthread_mutex_lock(&fsm->lock);

    /* Exactly one gate tick per FSM tick, unconditional of state. */
    rlx_gate_on_tick();

    switch (fsm->state) {
    case RLX_OPEN:
        /* No timer running while fully open and idle. */
        break;

    case RLX_WARNING:
        fsm->state_elapsed_ms += 1000u;
        /*
         * Compliance-audit fix: a "stuck active beyond diagnostic
         * timeout" check used to sit here comparing state_elapsed_ms
         * against RLX_WARNING_DIAGNOSTIC_TIMEOUT_MS (60s) - but since
         * RLX_WARNING_TO_CLOSING_MS (5s) always fires first and resets
         * state_elapsed_ms via enter_closing(), state_elapsed_ms can
         * never reach 60s while still in RLX_WARNING. It was dead code
         * under every input, not just an edge case. RC-11's "stuck
         * active" scenario means the physical TRAIN_APPROACHING sensor
         * line stays continuously asserted, which this discrete simulated
         * event (rlx_fsm_simulate_train_approaching(), a demo shim - no
         * real rlx_sensor.c exists yet) cannot represent; a real
         * implementation belongs in rlx_sensor.c once it reads an actual
         * continuous sensor line, not here.
         */
        if (fsm->state_elapsed_ms >= RLX_WARNING_TO_CLOSING_MS) {
            enter_closing(fsm);
        }
        break;

    case RLX_CLOSING:
    case RLX_RECLOSING:
        fsm->state_elapsed_ms += 1000u;
        check_closing_or_reclosing_complete(fsm);
        break;

    case RLX_CLOSED:
        fsm->state_elapsed_ms += 1000u;
        check_gate_contradiction_closed(fsm);
        if (fsm->state == RLX_CLOSED &&
            fsm->state_elapsed_ms >= RLX_EXPECTED_ARRIVAL_MS &&
            gates_confirmed_closed()) {
            enter_train_present(fsm);
        }
        break;

    case RLX_TRAIN_PRESENT:
        check_gate_contradiction_closed(fsm);
        if (fsm->state == RLX_TRAIN_PRESENT) {
            for (i = 0; i < RLX_MAX_OCCUPANCY_WINDOWS; i++) {
                if (!fsm->windows[i].active) {
                    continue;
                }
                if (rlx_timer_tick_window(&fsm->windows[i].remaining_ms, 1000u)) {
                    fsm->windows[i].active = 0;
                    fsm->active_window_count--;
                }
            }
            /* RC-04 invariant: reopening fires only when the COUNT of
             * active windows reaches zero, never on a single window's
             * expiry alone while another remains active. */
            if (fsm->active_window_count == 0) {
                enter_opening(fsm);
            }
        }
        break;

    case RLX_OPENING:
        fsm->state_elapsed_ms += 1000u;
        check_opening_complete(fsm);
        break;

    case RLX_FAULT:
        /* Latched until rlx_fsm_on_fault_clear() succeeds. */
        break;

    default:
        break;
    }

    pthread_mutex_unlock(&fsm->lock);
}

crossing_state_t rlx_fsm_get_crossing_state(const rlx_fsm_t *fsm)
{
    crossing_state_t result;

    pthread_mutex_lock((pthread_mutex_t *)&fsm->lock);
    result = map_to_crossing_state(fsm->state);
    pthread_mutex_unlock((pthread_mutex_t *)&fsm->lock);

    return result;
}

uint8_t rlx_fsm_take_fault_report_pending(rlx_fsm_t *fsm)
{
    uint8_t pending;

    pthread_mutex_lock(&fsm->lock);
    pending = fsm->fault_report_pending;
    fsm->fault_report_pending = 0;
    pthread_mutex_unlock(&fsm->lock);

    return pending;
}

void rlx_fsm_fill_status(const rlx_fsm_t *fsm, status_report_payload_t *status)
{
    pthread_mutex_lock((pthread_mutex_t *)&fsm->lock);
    status->role = ROLE_RAILWAY;
    status->crossing_state = (uint32_t)map_to_crossing_state(fsm->state);
    status->faults = fsm->faults;
    pthread_mutex_unlock((pthread_mutex_t *)&fsm->lock);
}

void rlx_fsm_report_watchdog_trip(rlx_fsm_t *fsm)
{
    pthread_mutex_lock(&fsm->lock);
    if (fsm->state != RLX_FAULT) {
        enter_fault(fsm, FAULT_WATCHDOG_TRIP);
    }
    pthread_mutex_unlock(&fsm->lock);
}
