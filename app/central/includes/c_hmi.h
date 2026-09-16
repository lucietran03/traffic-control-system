#ifndef C_HMI_H
#define C_HMI_H

#include "c_mode_eng.h"

// Renders the current network-wide status table to the terminal once per second.
void c_hmi_render(const c_mode_eng_t *eng);

#endif /* C_HMI_H */
