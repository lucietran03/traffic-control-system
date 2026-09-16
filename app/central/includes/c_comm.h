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

#ifndef C_COMM_H
#define C_COMM_H

#include <stdint.h>
#include <pthread.h>

#include "sys_types.h"
#include "ipc_msg.h"
#include "qnet_utils.h"
#include "c_mode_eng.h"

// Central's outgoing command builders for safe, asynchronous cross-node IPC.

// Initializes the console IO mutex to safely serialize asynchronous command logging.
void c_comm_set_console_io_lock(pthread_mutex_t *lock);

// Sends a mode change request to a specific target controller.
void c_comm_send_set_mode(ipc_client_queue_t *q, controller_id_t target, operating_mode_t mode);

// Broadcasts a mode change command to all local intersection controllers.
void c_comm_broadcast_set_mode(ipc_client_queue_t *q, operating_mode_t mode);

// Sends a specific timing profile and green-wave offset to a single target.
void c_comm_send_timing_profile(ipc_client_queue_t *q, controller_id_t target,
                                 uint32_t profile_id, uint32_t offset_ms);


// Broadcasts assigned timing profiles and offsets to all intersections in an arterial chain.
void c_comm_broadcast_timing_profile(ipc_client_queue_t *q, const c_arterial_offset_t *chain, int chain_len, uint32_t profile_id);

// Submits a manual routing override request to a specific controller.
void c_comm_send_request_override(ipc_client_queue_t *q, controller_id_t target,
                                   uint32_t override_type, uint32_t target_movement, uint32_t duration_ms);

// Renews an active routing override before it expires.
void c_comm_send_renew_override(ipc_client_queue_t *q, controller_id_t target, uint32_t extend_duration_ms);

// Cancels an active override for a specific controller.
void c_comm_send_cancel_override(ipc_client_queue_t *q, controller_id_t target);

// Requests a fault clearance on a specific railway or intersection controller.
void c_comm_send_request_fault_clear(ipc_client_queue_t *q, controller_id_t target);

#endif /* C_COMM_H */