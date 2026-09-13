#ifndef C_SERVER_H
#define C_SERVER_H

#include "sys_types.h"
#include "ipc_msg.h"
#include "c_mode_eng.h"

/* Records an incoming status snapshot (from MSG_STATUS or the
 * .summary field of MSG_HEARTBEAT - both carry the same
 * status_report_payload_t shape) against the sender's slot in eng-
 * >controllers[]. No-op if sender_id doesn't map to a valid index
 * (c_mode_eng_controller_index() returns -1).
 *
 * PA-08: returns 1 if this call just cleared marked_unavailable from 1 to
 * 0 - i.e. this is the first inbound message from a controller C1 had
 * previously declared UNAVAILABLE (c_watchdog_mon_tick()), a genuine
 * reconnect edge - 0 otherwise (including the invalid-index case). The
 * caller (c_main.c's on_request()) logs this under console_io_lock, the
 * same pattern already used for the UNAVAILABLE-side transition. */
int c_server_record_status(c_mode_eng_t *eng, controller_id_t sender_id,
                            const status_report_payload_t *status, uint64_t received_timestamp_ms);

/* Placeholder handling for MSG_FAULT_REPORT - c_controller_view_t has no
 * fault-history field yet, so there's nothing to record here. The event
 * is already logged (persisted + timestamped) by c_main.c's
 * MSG_FAULT_REPORT case via c_logger_log() right after this call - this
 * function intentionally does nothing until storage is added. */
void c_server_record_fault_report(c_mode_eng_t *eng, controller_id_t sender_id,
                                   const fault_report_payload_t *fault_report);

/* Records a standalone MSG_CROSSING_STATUS report (from an RLx, RC-02)
 * into the same last_reported_crossing_state field c_server_record_status
 * uses - both wire verbs ultimately describe the same fact. Same
 * reconnect-edge return-value contract as c_server_record_status() above. */
int c_server_record_crossing_status(c_mode_eng_t *eng, controller_id_t sender_id,
                                     const crossing_status_payload_t *crossing_status);

#endif /* C_SERVER_H */
