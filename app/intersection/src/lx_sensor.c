#include <stdio.h>

#include "lx_fsm.h"
#include "lx_sensor.h"

// Translates simulated keypress events into the corresponding FSM sensor or demand assertions.

// Displays a help message listing the available sensor input keys and their effects.
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

// Thread function that continuously reads keyboard input to simulate sensor events, updating the FSM state accordingly.
void *lx_sensor_reader_thread(void *arg)
{
    lx_fsm_t *fsm = (lx_fsm_t *)arg;
    char      input;

    lx_sensor_print_help();

    for (;;) {
        if (scanf(" %c", &input) != 1) {
            break;
        }

        switch (input) {
        case 'a': // Arterial vehicle demand present
            lx_fsm_set_arterial_vehicle_demand(fsm, 1);
            break;
        case 'A': // Arterial vehicle demand cleared
            lx_fsm_set_arterial_vehicle_demand(fsm, 0);
            break;
        case 'c': // Connector vehicle demand present
            lx_fsm_set_connector_vehicle_demand(fsm, 1);
            break;
        case 'C': // Connector vehicle demand cleared
            lx_fsm_set_connector_vehicle_demand(fsm, 0);
            break;
        case '1': // Pedestrian button pressed on side 0
            lx_fsm_latch_pedestrian_request(fsm, 0);
            break;
        case '2': // Pedestrian button pressed on side 1
            lx_fsm_latch_pedestrian_request(fsm, 1);
            break;
        case '3': // Pedestrian button pressed on side 2
            lx_fsm_latch_pedestrian_request(fsm, 2);
            break;
        case '4': // Pedestrian button pressed on side 3
            lx_fsm_latch_pedestrian_request(fsm, 3);
            break;
        case 'w': // Queue warning asserted
            lx_fsm_set_queue_warning(fsm, 1);
            break;
        case 'W': // Queue warning cleared
            lx_fsm_set_queue_warning(fsm, 0);
            break;
        case 'h': // Help
        case '?':
            lx_sensor_print_help();
            break;
        case 'q': // Quit sensor input thread
            printf("Lx sensor: stopping keyboard input (this thread only)\n");
            return NULL;
        default:
            printf("Lx sensor: ignored key '%c'\n", input);
            break;
        }
    }

    return NULL;
}