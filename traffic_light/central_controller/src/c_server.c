#include "c_server.h"

void c_server_record_status(c_mode_eng_t *eng, controller_id_t sender_id,
                             const status_report_payload_t *status, uint64_t received_timestamp_ms)
{
    int idx = c_mode_eng_controller_index(sender_id);

    if (idx < 0) {
        return;
    }
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
}

void c_server_record_fault_report(c_mode_eng_t *eng, controller_id_t sender_id,
                                   const fault_report_payload_t *fault_report)
{
    /* Audit fix: this used to printf() the report itself, duplicating
     * c_main.c's c_logger_log() call for the same event (one unpersisted
     * stdout line, one persisted+timestamped line, both firing back to
     * back). No storage exists yet (see doc comment in c_server.h) and
     * the actual logging now happens once, at the call site in
     * c_main.c's MSG_FAULT_REPORT case - this function intentionally
     * does nothing until c_controller_view_t gets a place to keep fault
     * history. */
    (void)eng;
    (void)sender_id;
    (void)fault_report;
}

void c_server_record_crossing_status(c_mode_eng_t *eng, controller_id_t sender_id,
                                      const crossing_status_payload_t *crossing_status)
{
    int idx = c_mode_eng_controller_index(sender_id);

    if (idx < 0) {
        return;
    }
    eng->controllers[idx].missed_heartbeat_ticks = 0;
    eng->controllers[idx].marked_unavailable     = 0;

    eng->controllers[idx].last_reported_crossing_state = crossing_status->state;
}
