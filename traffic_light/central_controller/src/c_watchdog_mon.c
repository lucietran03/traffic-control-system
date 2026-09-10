#include "c_watchdog_mon.h"
#include "c_logger.h"

void c_watchdog_mon_tick(c_mode_eng_t *eng)
{
    int i;

    for (i = 0; i < 9; i++) {
        eng->controllers[i].missed_heartbeat_ticks++;

        if (eng->controllers[i].missed_heartbeat_ticks == 3 &&
            eng->controllers[i].marked_unavailable == 0) {
            eng->controllers[i].marked_unavailable = 1;
            c_logger_log("Controller %d marked UNAVAILABLE - missed 3 consecutive heartbeats (PA-07)",
                         (int)eng->controllers[i].id);
        }
    }
}
