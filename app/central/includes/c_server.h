#ifndef C_SERVER_H
#define C_SERVER_H

#include "sys_types.h"
#include "ipc_msg.h"
#include "c_mode_eng.h"

// Updates internal state from a controller status report and detects network reconnects.
int c_server_record_status(c_mode_eng_t *eng, controller_id_t sender_id,
                            const status_report_payload_t *status, uint64_t received_timestamp_ms);

// Placeholder for recording incoming fault reports into controller history.
void c_server_record_fault_report(c_mode_eng_t *eng, controller_id_t sender_id,
                                   const fault_report_payload_t *fault_report);

// Updates the tracked crossing state from an incoming railway status report.
int c_server_record_crossing_status(c_mode_eng_t *eng, controller_id_t sender_id,
                                     const crossing_status_payload_t *crossing_status);

#endif /* C_SERVER_H */