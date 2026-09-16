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
#include <stdio.h>
#include <pthread.h>
#include "rlx_gate.h"

// Simulates hardware gate motion and completion times to enforce the confirmation-before-proceed invariant.

// Manages internal lock acquisition for gate logic independently of the FSM lock.
static pthread_mutex_t g_gate_lock = PTHREAD_MUTEX_INITIALIZER;

typedef enum { GATE_IDLE, GATE_MOVING_CLOSE, GATE_MOVING_OPEN } gate_motion_t;

static gate_motion_t g_motion;
static uint32_t      g_remaining_ms;
static uint8_t       g_fail_this_motion;
static uint8_t       g_demo_fault_armed;
static uint8_t       g_confirmed_closed;
static uint8_t       g_confirmed_open;

// Initializes the gate simulation state, ensuring that gates start in a confirmed open state matching the FSM's initial state.
void rlx_gate_init(void)
{
    pthread_mutex_lock(&g_gate_lock);
    g_motion = GATE_IDLE;
    g_remaining_ms = 0;
    g_fail_this_motion = 0;
    g_demo_fault_armed = 0;
    g_confirmed_closed = 0;
    // Gates default to the confirmed open state matching the FSM's initialization[cite: 44].
    g_confirmed_open = 1;   
    pthread_mutex_unlock(&g_gate_lock);
}

// Commands the gates to close, simulating motion and resetting confirmation flags. The FSM will monitor for confirmation or timeout.
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

// Called by the FSM's recurring tick to decrement gate motion timers and set confirmation flags when motion completes.
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

// Called by the FSM's recurring tick to decrement gate motion timers and set confirmation flags when motion completes.
void rlx_gate_on_tick(void)
{
    pthread_mutex_lock(&g_gate_lock);
    if (g_motion != GATE_IDLE) {
        if (g_remaining_ms > 1000u) {
            g_remaining_ms -= 1000u;
        } else {
            // Gate motion finishes on this tick; handles simulated faults or sets confirmation flag.
            if (g_fail_this_motion) {
                printf("RLx: gate FAILED TO CONFIRM (simulated fault) - rlx_fsm.c's own deadline will raise FAULT_GATE_CONFIRM_MISSING\n");
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

// Returns the current confirmed closed state of the gates, ensuring thread-safe access to the gate logic.
uint8_t rlx_gate_poll_closed(void)
{
    uint8_t v;
    pthread_mutex_lock(&g_gate_lock);
    v = g_confirmed_closed;
    pthread_mutex_unlock(&g_gate_lock);
    return v;
}

// Returns the current confirmed open state of the gates, ensuring thread-safe access to the gate logic.
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
    // Arms the gate to fail its next motion confirmation for testing the fault timeout.
    pthread_mutex_lock(&g_gate_lock);
    g_demo_fault_armed = 1;
    pthread_mutex_unlock(&g_gate_lock);
    printf("[DEMO] Next gate motion armed to fail confirmation (RC-06 fault path)\n");
}

void rlx_gate_force_confirmed_open(void)
{
    // Simulates physical gate repair to forcibly set a confirmed open state.
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