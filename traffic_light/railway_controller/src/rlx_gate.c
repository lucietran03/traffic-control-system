#include <stdio.h>
#include <pthread.h>
#include "rlx_gate.h"

/*
 * Simulated gate motion (RC-03/RC-06). No real hardware exists for this
 * PoC, so this module is the closest available exercise of the "gates
 * take real time to travel, and confirmation can genuinely fail" behaviour
 * that RC-06's confirmation-before-PROCEED invariant depends on. It
 * replaces rlx_fsm.c's previous "optimistic instant confirmation"
 * placeholder.
 *
 * Locking: most calls into this module happen while rlx_fsm.c already
 * holds its own fsm->lock (via rlx_fsm_on_tick()/enter_closing()/etc.),
 * but rlx_gate_arm_demo_fault() is called directly from rlx_sensor.c's
 * keyboard thread with NO fsm lock held. This module therefore keeps its
 * own internal lock, taken/released around every public function. This is
 * always the innermost/leaf lock - it never calls back into anything that
 * takes fsm->lock - so there is no lock-ordering hazard.
 */

static pthread_mutex_t g_gate_lock = PTHREAD_MUTEX_INITIALIZER;

typedef enum { GATE_IDLE, GATE_MOVING_CLOSE, GATE_MOVING_OPEN } gate_motion_t;

static gate_motion_t g_motion;
static uint32_t      g_remaining_ms;
static uint8_t       g_fail_this_motion;
static uint8_t       g_demo_fault_armed;
static uint8_t       g_confirmed_closed;
static uint8_t       g_confirmed_open;

void rlx_gate_init(void)
{
    pthread_mutex_lock(&g_gate_lock);
    g_motion = GATE_IDLE;
    g_remaining_ms = 0;
    g_fail_this_motion = 0;
    g_demo_fault_armed = 0;
    g_confirmed_closed = 0;
    g_confirmed_open = 1;   /* matches rlx_fsm_init()'s RLX_OPEN resting state - gates start open */
    pthread_mutex_unlock(&g_gate_lock);
}

void rlx_gate_command_close(void)
{
    pthread_mutex_lock(&g_gate_lock);
    printf("RLx: commanding gates DOWN (simulated motion, %u ms)\n", (unsigned)RLX_GATE_MOTION_MS);
    g_motion = GATE_MOVING_CLOSE;
    g_remaining_ms = RLX_GATE_MOTION_MS;
    g_confirmed_closed = 0;
    g_confirmed_open = 0;
    g_fail_this_motion = g_demo_fault_armed;
    g_demo_fault_armed = 0;
    pthread_mutex_unlock(&g_gate_lock);
}

void rlx_gate_command_open(void)
{
    pthread_mutex_lock(&g_gate_lock);
    printf("RLx: commanding gates UP (simulated motion, %u ms)\n", (unsigned)RLX_GATE_MOTION_MS);
    g_motion = GATE_MOVING_OPEN;
    g_remaining_ms = RLX_GATE_MOTION_MS;
    g_confirmed_closed = 0;
    g_confirmed_open = 0;
    g_fail_this_motion = g_demo_fault_armed;
    g_demo_fault_armed = 0;
    pthread_mutex_unlock(&g_gate_lock);
}

void rlx_gate_on_tick(void)
{
    pthread_mutex_lock(&g_gate_lock);
    if (g_motion != GATE_IDLE) {
        if (g_remaining_ms > 1000u) {
            g_remaining_ms -= 1000u;
        } else {
            /* Motion complete this tick. */
            if (g_fail_this_motion) {
                printf("RLx: gate FAILED TO CONFIRM (simulated fault) - rlx_fsm.c's own deadline will raise FAULT_GATE_CONFIRM_MISSING\n");
                /* Leave both confirmed flags at 0 - rlx_fsm.c's existing
                 * RLX_CLOSING_DEADLINE_MS/RLX_OPENING_DEADLINE_MS checks
                 * will fire the fault; this module does not raise faults
                 * itself. */
            } else if (g_motion == GATE_MOVING_CLOSE) {
                g_confirmed_closed = 1;
            } else {
                g_confirmed_open = 1;
            }
            g_motion = GATE_IDLE;
            g_remaining_ms = 0;
        }
    }
    pthread_mutex_unlock(&g_gate_lock);
}

uint8_t rlx_gate_poll_closed(void)
{
    uint8_t v;
    pthread_mutex_lock(&g_gate_lock);
    v = g_confirmed_closed;
    pthread_mutex_unlock(&g_gate_lock);
    return v;
}

uint8_t rlx_gate_poll_open(void)
{
    uint8_t v;
    pthread_mutex_lock(&g_gate_lock);
    v = g_confirmed_open;
    pthread_mutex_unlock(&g_gate_lock);
    return v;
}

void rlx_gate_arm_demo_fault(void)
{
    pthread_mutex_lock(&g_gate_lock);
    g_demo_fault_armed = 1;
    pthread_mutex_unlock(&g_gate_lock);
    printf("[DEMO] Next gate motion armed to fail confirmation (RC-06 fault path)\n");
}

void rlx_gate_force_confirmed_open(void)
{
    pthread_mutex_lock(&g_gate_lock);
    g_motion = GATE_IDLE;
    g_remaining_ms = 0;
    g_fail_this_motion = 0;
    g_demo_fault_armed = 0;
    g_confirmed_closed = 0;
    g_confirmed_open = 1;
    pthread_mutex_unlock(&g_gate_lock);
    printf("[DEMO] Gate mechanism simulated as physically repaired - now confirmed OPEN (RC-09/RC-10 fault-clear demo path)\n");
}
