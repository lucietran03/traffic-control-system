#include <stdio.h>

#include "lx_fsm.h"
#include "lx_sensor.h"

/*
 * Lx simulated sensor input - implementation.
 *
 * Stand-in for real vehicle/pedestrian/queue-warning hardware sensors:
 * reads single keypresses from stdin and forwards each one to the
 * matching lx_fsm_set_*()/lx_fsm_latch_*() setter (see lx_fsm.h's
 * "sensor-input setters" section). Runs on its own thread (started by
 * lx_main.c) because reading stdin blocks - see this file's header
 * comment for why it cannot share the server or client thread.
 */

static void lx_sensor_print_help(void)
{
    printf("Lx sensor keys:\n");
    printf("  a = arterial approach: vehicle present     A = arterial approach: vehicle clears\n");
    printf("  c = connector approach: vehicle present    C = connector approach: vehicle clears\n");
    printf("  1 = pedestrian button, side 0              2 = pedestrian button, side 1\n");
    printf("  3 = pedestrian button, side 2              4 = pedestrian button, side 3\n");
    printf("  w = queue-warning asserted (CC-01)         W = queue-warning cleared\n");
    printf("  h or ? = show this help                    q = stop keyboard input (this thread only)\n");
}

void *lx_sensor_reader_thread(void *arg)
{
    lx_fsm_t *fsm = (lx_fsm_t *)arg;
    char      input;

    lx_sensor_print_help();

    for (;;) {
        if (scanf(" %c", &input) != 1) {
            /* EOF or stdin closed - stop this thread rather than spin. */
            break;
        }

        switch (input) {
        case 'a':
            lx_fsm_set_arterial_vehicle_demand(fsm, 1);
            break;
        case 'A':
            lx_fsm_set_arterial_vehicle_demand(fsm, 0);
            break;
        case 'c':
            lx_fsm_set_connector_vehicle_demand(fsm, 1);
            break;
        case 'C':
            lx_fsm_set_connector_vehicle_demand(fsm, 0);
            break;
        case '1':
            lx_fsm_latch_pedestrian_request(fsm, 0);
            break;
        case '2':
            lx_fsm_latch_pedestrian_request(fsm, 1);
            break;
        case '3':
            lx_fsm_latch_pedestrian_request(fsm, 2);
            break;
        case '4':
            lx_fsm_latch_pedestrian_request(fsm, 3);
            break;
        case 'w':
            lx_fsm_set_queue_warning(fsm, 1);
            break;
        case 'W':
            lx_fsm_set_queue_warning(fsm, 0);
            break;
        case 'h':
        case '?':
            lx_sensor_print_help();
            break;
        case 'q':
            printf("Lx sensor: stopping keyboard input (this thread only)\n");
            return NULL;
        default:
            printf("Lx sensor: ignored key '%c'\n", input);
            break;
        }
    }

    return NULL;
}
