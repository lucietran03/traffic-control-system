#include <stdio.h>
#include <pthread.h>

#include "rlx_fsm.h"
#include "rlx_timer.h"
#include "rlx_gate.h"
#include "rlx_signal.h"

// Railway FSM implementation owning state transitions while relying on rlx_timer for durations and rlx_gate for hardware abstraction.

static crossing_state_t map_to_crossing_state(rlx_internal_state_t state)
{
    switch (state) {
    case RLX_OPEN:
        return CROSSING_OPEN; // The crossing is fully open with no active train approaches or gate closures.
    case RLX_WARNING:
    case RLX_CLOSING:
    case RLX_RECLOSING:
        return CROSSING_WARNING; // Shared warning state for both closing and reclosing phases, as the crossing is not yet fully secured.
    case RLX_CLOSED:
    case RLX_TRAIN_PRESENT:
    case RLX_OPENING:
        return CROSSING_CLOSED; // The crossing is closed, either due to active train presence or during the opening phase after a train has passed.
    case RLX_FAULT:
    default:
        return CROSSING_FAULT; // The crossing is in a fault state, indicating a problem with the system.
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

// Registers or refreshes occupancy windows for up to two track directions without duplicating active tracking slots.
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
    // Defensively ignores out-of-bounds directions if both tracking slots are occupied.
}

static void enter_fault(rlx_fsm_t *fsm, fault_flags_t fault_bit)
{
    // Forces gates down on faults and triggers safe fault outputs regardless of current gate position.
    rlx_gate_command_close();
    rlx_signal_show_fault(fault_bit);
    fsm->state = RLX_FAULT;
    fsm->state_elapsed_ms = 0;
    fsm->faults |= fault_bit;
    fsm->fault_report_pending = 1;
}

// Shared evaluator for CLOSING and RECLOSING states resolving to CLOSED upon gate confirmation.
static void check_closing_or_reclosing_complete(rlx_fsm_t *fsm)
{
    if (gates_confirmed_closed()) {
        int i;
        // Enforces the invariant requiring gate-confirmed closure before granting proceed signals[cite: 43].
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

// Shared evaluator for OPENING state resolving to OPEN upon gate confirmation.
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

// Transitions to CLOSING state, commanding gates down and resetting the elapsed timer.
static void enter_closing(rlx_fsm_t *fsm)
{
    rlx_gate_command_close();
    fsm->state = RLX_CLOSING;
    fsm->state_elapsed_ms = 0;
}

// Transitions to RECLOSING state, commanding gates down and resetting the elapsed timer.
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
    // Initializes 20s occupancy countdowns for unassigned windows and sets train present state.
    for (i = 0; i < RLX_MAX_OCCUPANCY_WINDOWS; i++) {
        if (fsm->windows[i].active && fsm->windows[i].remaining_ms == 0) {
            fsm->windows[i].remaining_ms = RLX_OCCUPANCY_WINDOW_MS;
        }
    }
    fsm->state = RLX_TRAIN_PRESENT;
    fsm->state_elapsed_ms = 0;
}

// Transitions to OPENING state, commanding gates up and resetting the elapsed timer.
static void enter_opening(rlx_fsm_t *fsm)
{
    rlx_signal_show_train_stop();
    rlx_gate_command_open();
    fsm->state = RLX_OPENING;
    fsm->state_elapsed_ms = 0;
}

void rlx_fsm_init(rlx_fsm_t *fsm, controller_id_t self_id)
{
    int i;

    // Initializes FSM lock and sets the initial state to OPEN with no active occupancy windows.
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
    // Gate confirmation relies on rlx_gate.c, separate from FSM startup.
    fsm->faults = FAULT_NONE;
    fsm->fault_report_pending = 0;
    // Starts disconnected and waits for the first heartbeat ACK to flip to connected status.
    fsm->link_state = LINK_DEGRADED_LOCAL;
    fsm->missed_heartbeat_acks = 0;
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
        // Self-loop handles additional approaches during warning/closing/reclosing steps while continuing the elapsed timer.
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
        // Direct occupancy assignment since the crossing is already occupied with no warning wait.
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
        // Ignores approach events while faulted.
        break;

    default:
        break;
    }

    pthread_mutex_unlock(&fsm->lock);
}

// Processes a fault-clear request, verifying gate confirmation before resetting the FSM to OPEN and clearing occupancy tracking.
void rlx_fsm_on_fault_clear(rlx_fsm_t *fsm, ipc_reply_t *reply)
{
    pthread_mutex_lock(&fsm->lock);

    if (fsm->state != RLX_FAULT) {
        reply->result = RESULT_NACK;
        reply->reason = NACK_REASON_UNKNOWN_TARGET;
    } else {
        // Live-verifies gate mechanism is open to safely clear faults, resetting occupancy tracking.
        if (gates_confirmed_open()) {
            fsm->state = RLX_OPEN;
            fsm->state_elapsed_ms = 0;
            fsm->faults = FAULT_NONE;
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

// Processes the recurring 1s tick for warning, closing, and occupancy countdowns, advancing state transitions as appropriate.
void rlx_fsm_on_tick(rlx_fsm_t *fsm)
{
    int i;

    pthread_mutex_lock(&fsm->lock);

    // Single recurring tick driving the warning, closing, and occupancy timing chains.
    rlx_gate_on_tick();

    switch (fsm->state) {
    case RLX_OPEN:
        break;

    case RLX_WARNING:
        fsm->state_elapsed_ms += 1000u;
        // Advances warning states to closing exactly after the 5s allowance since the continuous diagnostic timeout is a known placeholder.
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
            // Triggers reopening strictly when all active occupancy windows expire.
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
        // Faults remain latched until explicitly cleared.
        break;

    default:
        break;
    }

    pthread_mutex_unlock(&fsm->lock);
}

// Returns the current crossing state derived from the FSM's internal state, ensuring thread-safe access.
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

    // Atomically reads and clears the fault report flag for safe independent IPC reporting.
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
    status->link_state = (uint32_t)fsm->link_state;
    pthread_mutex_unlock((pthread_mutex_t *)&fsm->lock);
}

int rlx_fsm_on_heartbeat_result(rlx_fsm_t *fsm, int acked)
{
    int transition = 0;

    // Heartbeat processing callback determining local degradation or Central reconnections.
    pthread_mutex_lock(&fsm->lock);
    if (acked) {
        if (fsm->link_state != LINK_CENTRAL_CONNECTED) {
            fsm->link_state = LINK_CENTRAL_CONNECTED;
            transition = 2;
        }
        fsm->missed_heartbeat_acks = 0;
    } else {
        if (fsm->missed_heartbeat_acks < 0xFFFFFFFFu) {
            fsm->missed_heartbeat_acks++;
        }
        if (fsm->missed_heartbeat_acks >= 3 && fsm->link_state == LINK_CENTRAL_CONNECTED) {
            fsm->link_state = LINK_DEGRADED_LOCAL;
            transition = 1;
        }
    }
    pthread_mutex_unlock(&fsm->lock);

    return transition;
}

void rlx_fsm_report_watchdog_trip(rlx_fsm_t *fsm)
{
    // Directly triggers a fault via watchdog if the FSM loop stalls.
    pthread_mutex_lock(&fsm->lock);
    if (fsm->state != RLX_FAULT) {
        enter_fault(fsm, FAULT_WATCHDOG_TRIP);
    }
    pthread_mutex_unlock(&fsm->lock);
}