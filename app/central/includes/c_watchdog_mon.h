#ifndef C_WATCHDOG_MON_H
#define C_WATCHDOG_MON_H

#include "c_mode_eng.h"

// Ticks the network watchdog to track missed heartbeats and flag unavailable controllers.
int c_watchdog_mon_tick(c_mode_eng_t *eng, controller_id_t *out_newly_unavailable);

#endif /* C_WATCHDOG_MON_H */