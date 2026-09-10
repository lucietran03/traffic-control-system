#ifndef C_HMI_H
#define C_HMI_H

#include "c_mode_eng.h"

/* Renders the current network-wide status to the terminal (the brief
 * explicitly permits plain text, no GUI required). Called once per
 * second from c_main.c's IPC_PULSE_HEARTBEAT_TICK case, right after
 * c_watchdog_mon_tick() so availability reflects that tick's result. */
void c_hmi_render(const c_mode_eng_t *eng);

#endif /* C_HMI_H */
