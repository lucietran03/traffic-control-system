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
#include <string.h>

#include "rlx_sensor.h"
#include "rlx_gate.h"
#include "ipc_msg.h"

// Dedicated blocking thread mapping simulated keyboard input to real-time train approach events and demo testing operations.

static void print_help(void)
{
    printf("RLx sensor keys:\n");
    printf("  0 = TRAIN_APPROACHING, direction 0         1 = TRAIN_APPROACHING, direction 1\n");
    printf("  x = arm next gate motion to FAIL confirmation (RC-06 demo)\n");
    printf("  r = simulate gate mechanism physically repaired/confirmed OPEN (RC-09/RC-10 fault-clear demo)\n");
    printf("  f = DEMO-ONLY local fault-clear trigger (bypasses the real MSG_REQUEST_FAULT_CLEAR path)\n");
    printf("  h or ? = show this help                    q = stop keyboard input (this thread only)\n");
}

void *rlx_sensor_reader_thread(void *arg)
{
    rlx_fsm_t *fsm = (rlx_fsm_t *)arg;
    char       input;

    print_help();

    while (scanf(" %c", &input) == 1) {
        switch (input) {
        case '0': // Simulates a train approaching from direction 0
            rlx_fsm_simulate_train_approaching(fsm, 0);
            break;
        case '1': // Simulates a train approaching from direction 1
            rlx_fsm_simulate_train_approaching(fsm, 1);
            break;
        case 'x': // Arms the gate to fail its next motion confirmation
            rlx_gate_arm_demo_fault();
            break;
        case 'r': // Simulates physical gate repair to forcibly set a confirmed open state
            rlx_gate_force_confirmed_open();
            break;
        case 'f': {
            ipc_reply_t reply;
            memset(&reply, 0, sizeof(reply));
            printf("[rlx_sensor] DEMO-ONLY local fault-clear trigger (RC-09 requires Central in production)\n");
            rlx_fsm_on_fault_clear(fsm, &reply);
            printf("[rlx_sensor] fault-clear result=%u reason=%u\n", (unsigned)reply.result, (unsigned)reply.reason);
            break;
        }
        case 'h': // Help
        case '?':
            print_help();
            break;
        case 'q': // Quit the sensor reader thread
            return NULL;
        default:
            printf("[rlx_sensor] ignored key '%c'\n", input);
            break;
        }
    }

    return NULL;
}