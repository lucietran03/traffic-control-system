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

#ifndef C_HMI_H
#define C_HMI_H

#include "c_mode_eng.h"

// Renders the current network-wide status table to the terminal once per second.
void c_hmi_render(const c_mode_eng_t *eng);

#endif /* C_HMI_H */
