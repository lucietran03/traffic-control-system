#include <stdio.h>
#include <string.h>

#include "rlx_sensor.h"
#include "rlx_gate.h"
#include "ipc_msg.h"

/*
 * Demo/keyboard-driven stand-in for real train-approach sensors (RC-01).
 * Runs as its own dedicated thread (see rlx_sensor.h) because reading
 * stdin blocks - it must never share the server thread (ipc_server_run())
 * or the client thread (ipc_client_thread_main()).
 */

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
        case '0':
            rlx_fsm_simulate_train_approaching(fsm, 0);
            break;
        case '1':
            rlx_fsm_simulate_train_approaching(fsm, 1);
            break;
        case 'x':
            rlx_gate_arm_demo_fault();
            break;
        case 'r':
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
        case 'h':
        case '?':
            print_help();
            break;
        case 'q':
            return NULL;
        default:
            printf("[rlx_sensor] ignored key '%c'\n", input);
            break;
        }
    }

    return NULL;
}
