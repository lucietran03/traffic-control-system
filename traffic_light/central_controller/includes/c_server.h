#ifndef C_SERVER_H
#define C_SERVER_H

#include "sys_types.h"
#include "ipc_msg.h"
#include "c_mode_eng.h"

/* Records an incoming status snapshot (from MSG_STATUS or the
 * .summary field of MSG_HEARTBEAT - both carry the same
 * status_report_payload_t shape) against the sender's slot in eng-
 * >controllers[]. No-op if sender_id doesn't map to a valid index
 * (c_mode_eng_controller_index() returns -1). */
void c_server_record_status(c_mode_eng_t *eng, controller_id_t sender_id,
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
 * uses - both wire verbs ultimately describe the same fact. */
void c_server_record_crossing_status(c_mode_eng_t *eng, controller_id_t sender_id,
                                      const crossing_status_payload_t *crossing_status);

#endif /* C_SERVER_H */
