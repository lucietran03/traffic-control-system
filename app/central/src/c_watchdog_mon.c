#include "c_watchdog_mon.h"

int c_watchdog_mon_tick(c_mode_eng_t *eng, controller_id_t *out_newly_unavailable)
{
    int i;
    int n = 0;

    for (i = 0; i < 9; i++) {
        eng->controllers[i].missed_heartbeat_ticks++;

        if (eng->controllers[i].missed_heartbeat_ticks == 3 &&
            eng->controllers[i].marked_unavailable == 0) {
            eng->controllers[i].marked_unavailable = 1;
            out_newly_unavailable[n++] = eng->controllers[i].id;
        }
    }

    return n;
}
