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

#include "c_server.h"

// Implements the synchronous request handlers for incoming status, heartbeat, fault, and crossing reports.
int c_server_record_status(c_mode_eng_t *eng, controller_id_t sender_id,
                            const status_report_payload_t *status, uint64_t received_timestamp_ms)
{
    int idx = c_mode_eng_controller_index(sender_id);
    int was_unavailable;

    if (idx < 0) {
        return 0;
    }
    was_unavailable = eng->controllers[idx].marked_unavailable;
    eng->controllers[idx].missed_heartbeat_ticks = 0;
    eng->controllers[idx].marked_unavailable     = 0;

    eng->controllers[idx].last_reported_mode              = status->mode;
    eng->controllers[idx].last_reported_signal_phase       = status->signal_phase;
    eng->controllers[idx].last_reported_crossing_state     = status->crossing_state;
    eng->controllers[idx].last_reported_supervisory_state  = status->supervisory_state;
    eng->controllers[idx].last_reported_faults             = status->faults;
    eng->controllers[idx].last_reported_sensor_status      = status->sensor_status;
    eng->controllers[idx].last_reported_active_profile_id  = status->active_profile_id;
    eng->controllers[idx].last_reported_override_active    = status->override_active;
    eng->controllers[idx].last_reported_link_state         = status->link_state;
    eng->controllers[idx].last_seen_timestamp_ms           = received_timestamp_ms;

    return was_unavailable;
}

// Placeholder for fault history recording; logging is handled directly in the request dispatcher.
void c_server_record_fault_report(c_mode_eng_t *eng, controller_id_t sender_id,
                                   const fault_report_payload_t *fault_report)
{
    (void)eng;
    (void)sender_id;
    (void)fault_report;
}

// Implements the synchronous request handler for incoming crossing status reports.
int c_server_record_crossing_status(c_mode_eng_t *eng, controller_id_t sender_id,
                                     const crossing_status_payload_t *crossing_status)
{
    int idx = c_mode_eng_controller_index(sender_id);
    int was_unavailable;

    if (idx < 0) {
        return 0;
    }
    was_unavailable = eng->controllers[idx].marked_unavailable;
    eng->controllers[idx].missed_heartbeat_ticks = 0;
    eng->controllers[idx].marked_unavailable     = 0;

    eng->controllers[idx].last_reported_crossing_state = crossing_status->state;

    return was_unavailable;
}