#ifndef RLX_FSM_H
#define RLX_FSM_H

#include <stdint.h>
#include <pthread.h>

#include "sys_types.h"
#include "ipc_msg.h"

/*
 * Railway-crossing FSM (STATE_CHARTS.md SC-04A/SC-04B; RC-01, RC-03, RC-04,
 * RC-05, RC-06, RC-09, RC-10, RC-11).
 *
 * This is pure state + data: it never calls MsgSend()/ipc_client_post()
 * itself (see app/shared/README.md "Threading pattern" - only the client
 * thread's queue is allowed to originate an outgoing request). rlx_main.c
 * owns all IPC; it calls into this FSM from its server-thread callbacks
 * (on_request/on_pulse) and reads back state via the getters below to
 * decide what, if anything, to send.
 *
 * Internal sub-states (RLX_OPEN..RLX_FAULT) are private to this header and
 * deliberately richer than the wire-visible crossing_state_t in
 * sys_types.h - see that file's own comment on why CLOSING/TRAIN_PRESENT/
 * OPENING/RECLOSING never cross a Qnet boundary.
 */

#define RLX_MAX_OCCUPANCY_WINDOWS 2

typedef struct {
    uint8_t   active;
    uint32_t  direction;
    uint32_t  remaining_ms;   /* counts down from 20000 (RC-04) once in TRAIN_PRESENT */
} rlx_occupancy_window_t;

typedef enum {
    RLX_OPEN = 0, RLX_WARNING, RLX_CLOSING, RLX_CLOSED,
    RLX_TRAIN_PRESENT, RLX_OPENING, RLX_RECLOSING, RLX_FAULT
} rlx_internal_state_t;

typedef struct {
    controller_id_t          self_id;
    rlx_internal_state_t     state;
    uint32_t                 state_elapsed_ms;   /* reset on every state entry; drives the 5s/15s/20s/etc thresholds */
    rlx_occupancy_window_t   windows[RLX_MAX_OCCUPANCY_WINDOWS];
    uint8_t                  active_window_count;
    fault_flags_t            faults;
    uint8_t                  fault_report_pending;
    connectivity_state_t     link_state;
    pthread_mutex_t          lock;
} rlx_fsm_t;

/* One-time setup. Starts in RLX_OPEN with no active occupancy windows. */
void rlx_fsm_init(rlx_fsm_t *fsm, controller_id_t self_id);

/* Demo/test-only entry point until rlx_sensor.c exists: simulates a
 * TRAIN_APPROACHING(direction) sensor event. Not part of the Qnet
 * contract - this exists so the crossing FSM can be exercised/demoed
 * before real sensor input is wired up. direction: 0 or 1 (RC-01's two
 * rail directions). */
void rlx_fsm_simulate_train_approaching(rlx_fsm_t *fsm, uint32_t direction);

/*
 * Handles MSG_REQUEST_FAULT_CLEAR (RC-09/RC-10). This is the ONLY thing a
 * cross-node request from Central may do to this FSM - RLx never accepts a
 * raw gate/flasher/signal command from Central. Fills *reply with
 * RESULT_ACK (fault cleared, both gate sensors re-confirmed OPEN),
 * RESULT_NACK/NACK_REASON_FAULT_ACTIVE (sensors still disagree - fault
 * stays latched), or RESULT_NACK/NACK_REASON_UNKNOWN_TARGET (state is not
 * currently RLX_FAULT, nothing to clear).
 */
void rlx_fsm_on_fault_clear(rlx_fsm_t *fsm, ipc_reply_t *reply);

/* Called every 1000ms by a periodic tick armed in rlx_main.c (reuse
 * IPC_PULSE_RAILWAY_WARNING for the recurring 1 s tick that drives ALL of
 * this FSM's internal timing - both the warning/closing chain and the
 * occupancy countdown; see rlx_fsm.c for why IPC_PULSE_RAILWAY_OCCUPANCY
 * is not used as a second pulse here). */
void rlx_fsm_on_tick(rlx_fsm_t *fsm);

/* Derives the wire-visible crossing_state_t from the internal state per
 * the mapping documented in rlx_fsm.c. */
crossing_state_t rlx_fsm_get_crossing_state(const rlx_fsm_t *fsm);

/* Returns 1 and clears the flag if a fault just occurred since the last
 * call, else 0. Atomic under fsm->lock, so rlx_main.c can independently
 * fire off MSG_FAULT_REPORT (via ipc_client_post) and apply local safe
 * outputs in parallel (RC-10 - neither is sequentially gated on the
 * other). */
uint8_t rlx_fsm_take_fault_report_pending(rlx_fsm_t *fsm);

/* Fills role=ROLE_RAILWAY, crossing_state, and faults into *status. Caller
 * owns link_state and everything else (mode/signal_phase/supervisory_state
 * are not meaningful for ROLE_RAILWAY - see ipc_msg.h) and is responsible
 * for actually sending it. */
void rlx_fsm_fill_status(const rlx_fsm_t *fsm, status_report_payload_t *status);

/* PA-10: called by rlx_watchdog.c when the main loop appears to have
 * stalled (no tick observed for too long). Transitions the crossing
 * directly into the FAULT state via the existing enter_fault() path (NOT
 * just an fsm->faults bit like the intersection side - a railway crossing
 * hang must immediately hold the crossing in its safe state, not wait
 * for the next tick to notice a bit was set, since rlx_fsm_on_tick()
 * itself might be the thing that's hung). */
void rlx_fsm_report_watchdog_trip(rlx_fsm_t *fsm);

#endif /* RLX_FSM_H */
