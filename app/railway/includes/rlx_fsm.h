/*
    RMIT University Vietnam
    Course: EEET2588 Real-Time System Engineering
    Semester: 2026-2
    Author: Team QNX
    Member: Tran Dong Nghi - s3914633
			Le Hung - s4061665
			Hoang Minh Thang - s3999925
    Assessment: 2 - Project Implementation 
    Due date: 18/09/2026
*/

#ifndef RLX_FSM_H
#define RLX_FSM_H

#include <stdint.h>
#include <pthread.h>

#include "sys_types.h"
#include "ipc_msg.h"

// Pure state and data manager for the railway-crossing FSM that delegates all IPC handling to the main node.
#define RLX_MAX_OCCUPANCY_WINDOWS 2

typedef struct {
    uint8_t   active;
    uint32_t  direction;
    // Counts down from 20,000 milliseconds once a train is present.
    uint32_t  remaining_ms;
} rlx_occupancy_window_t;

typedef enum {
    RLX_OPEN = 0, RLX_WARNING, RLX_CLOSING, RLX_CLOSED,
    RLX_TRAIN_PRESENT, RLX_OPENING, RLX_RECLOSING, RLX_FAULT
} rlx_internal_state_t;

typedef struct {
    controller_id_t          self_id;
    rlx_internal_state_t     state;
    // Resets on state entry to drive threshold timing evaluations.
    uint32_t                 state_elapsed_ms;
    rlx_occupancy_window_t   windows[RLX_MAX_OCCUPANCY_WINDOWS];
    uint8_t                  active_window_count;
    fault_flags_t            faults;
    uint8_t                  fault_report_pending;
    connectivity_state_t     link_state;
    // Tracks unacknowledged heartbeat replies to evaluate Central connection degradation.
    uint32_t                 missed_heartbeat_acks;
    pthread_mutex_t          lock;
} rlx_fsm_t;

// Initializes the railway FSM to the OPEN state without active occupancy window.
void rlx_fsm_init(rlx_fsm_t *fsm, controller_id_t self_id);

// Simulates a train approaching event for testing before real sensor integration.
void rlx_fsm_simulate_train_approaching(rlx_fsm_t *fsm, uint32_t direction);

// Safely evaluates and replies to a MSG_REQUEST_FAULT_CLEAR command from Central.
void rlx_fsm_on_fault_clear(rlx_fsm_t *fsm, ipc_reply_t *reply);

// Processes the recurring 1-second tick to drive warning, closing, and occupancy countdown logic.
void rlx_fsm_on_tick(rlx_fsm_t *fsm);

// Translates the internal FSM state into the externally visible wire crossing state.
crossing_state_t rlx_fsm_get_crossing_state(const rlx_fsm_t *fsm);

// Atomically checks and clears the fault report flag to safely trigger an IPC transmission.
uint8_t rlx_fsm_take_fault_report_pending(rlx_fsm_t *fsm);

// Populates a status report payload with railway-specific state, fault, and link details.
void rlx_fsm_fill_status(const rlx_fsm_t *fsm, status_report_payload_t *status);

// Updates the connectivity state based on a heartbeat ACK and determines degradation or reconnection.
int rlx_fsm_on_heartbeat_result(rlx_fsm_t *fsm, int acked);

// Triggers an immediate FAULT state transition when the watchdog detects a stalled main loop.
void rlx_fsm_report_watchdog_trip(rlx_fsm_t *fsm);

#endif /* RLX_FSM_H */