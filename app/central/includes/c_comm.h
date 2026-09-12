#ifndef C_COMM_H
#define C_COMM_H

#include <stdint.h>
#include <pthread.h>

#include "sys_types.h"
#include "ipc_msg.h"
#include "qnet_utils.h"
#include "c_mode_eng.h"

/*
 * Central's OUTGOING command builders (UC-03, UC-06 alt 7.1, UC-07, UC-08;
 * SD-03, SD-06, SD-07). Mirrors lx_comm.c/rlx_comm.c in spirit - build one
 * ipc_request_t on the stack, memset() it to zero, fill verb/sender_id/
 * target_id/payload, then hand it to ipc_client_post() - but every
 * function here originates a COMMAND (C1 -> Lx/RLx), the opposite
 * direction of lx_comm.c's/rlx_comm.c's own heartbeat/status senders.
 *
 * Per app/shared/README.md "Threading pattern" and qnet_utils.h's
 * documented ipc_client_post() contract, these are safe to call from any
 * thread - c_operator.c (the operator-console thread) is the only caller
 * today, but nothing here assumes that.
 *
 * Every sender is fire-and-forget from the caller's point of view:
 * ipc_client_post() returns immediately, and the eventual ACK/
 * ACK_PENDING/NACK(reason)/ERROR (or a "send failed" if the peer could
 * not be reached at all) is logged asynchronously, on the CLIENT thread,
 * by the shared on_command_reply() handler in c_comm.c via
 * c_logger_log(). None of that logging touches c_mode_eng_t - see
 * c_comm.c's doc comment on on_command_reply() for why that matters.
 */

/* Must be called once from main(), right after ctx.console_io_lock is
 * pthread_mutex_init()'d and before the client thread starts, so that
 * on_command_reply() (c_comm.c, runs on the CLIENT thread) can serialise
 * its own c_logger_log() calls against c_hmi_render()'s 1 Hz status table
 * and c_operator.c's interactive prompts - every c_comm_send_*()/
 * broadcast function below can complete asynchronously at any time, from
 * a thread neither c_main.c's on_pulse() nor c_operator.c's reader thread
 * coordinates with otherwise, so without this it reopens exactly the
 * terminal-splicing race console_io_lock exists to close. */
void c_comm_set_console_io_lock(pthread_mutex_t *lock);

/* UC-07/SD-03 (the "operator requests a mode change" opt block):
 * MSG_SET_MODE(target, mode). */
void c_comm_send_set_mode(ipc_client_queue_t *q, controller_id_t target, operating_mode_t mode);

/* DP-01/DP-02 peak-hour auto-switch + demo aid (c_mode_eng_auto_check(),
 * c_operator.c's 'd'/'a' commands): sends MSG_SET_MODE(mode) to every
 * L1-L6 in turn (never RLx, which has no operating_mode_t of its own).
 * Mirrors c_comm_broadcast_timing_profile()'s shape - one call, whole
 * chain, rather than the caller looping over c_comm_send_set_mode() itself. */
void c_comm_broadcast_set_mode(ipc_client_queue_t *q, operating_mode_t mode);

/* Single-target MSG_SET_TIMING_PROFILE(target, profile_id, offset_ms).
 * Used internally by c_comm_broadcast_timing_profile() below; exposed
 * separately in case a future caller needs to (re)send just one chain
 * member's profile (e.g. after that one controller alone NACKed). */
void c_comm_send_timing_profile(ipc_client_queue_t *q, controller_id_t target,
                                 uint32_t profile_id, uint32_t offset_ms);

/*
 * UC-03/SD-03 main flow step 2 ("Central supplies EACH relevant
 * intersection with its assigned timing offset") and TC-01..05: builds
 * one MSG_SET_TIMING_PROFILE per chain entry via
 * c_mode_eng_build_timing_profile() and posts all of them - never just
 * one. chain/chain_len normally come straight from
 * c_mode_eng_get_chain(); profile_id normally comes from
 * c_mode_eng_next_profile_id(). No-op (with a c_logger_log() line) if
 * chain is NULL or chain_len is out of range.
 */
void c_comm_broadcast_timing_profile(ipc_client_queue_t *q, const c_arterial_offset_t *chain,
                                      int chain_len, uint32_t profile_id);

/* UC-08/SD-07 step 2: MSG_REQUEST_OVERRIDE(CLEAR_ROUTE, target_movement,
 * duration_ms). target_movement is an override_movement_t value
 * (sys_types.h: OVERRIDE_MOVEMENT_ARTERIAL=0/OVERRIDE_MOVEMENT_CONNECTOR=1)
 * carried as a plain uint32_t on the wire per ipc_msg.h's wire-format
 * rules. Caller (c_operator.c) is responsible for running the request
 * through c_mode_eng_validate_override_request() first (PA-11 surface
 * validation) - this function does not validate, it only sends. */
void c_comm_send_request_override(ipc_client_queue_t *q, controller_id_t target,
                                   uint32_t override_type, uint32_t target_movement, uint32_t duration_ms);

/* UC-08/SD-07 "operator requests a renewal before expiry":
 * MSG_RENEW_OVERRIDE(extend_duration_ms). extend_duration_ms == 0 means
 * "renew for the original duration" (see renew_override_payload_t). */
void c_comm_send_renew_override(ipc_client_queue_t *q, controller_id_t target, uint32_t extend_duration_ms);

/* UC-08/SD-07 "operator cancels active override": MSG_CANCEL_OVERRIDE.
 * No payload on the wire - the envelope's sender_id/target_id fully
 * identify which active override is being cancelled (see ipc_msg.h). */
void c_comm_send_cancel_override(ipc_client_queue_t *q, controller_id_t target);

/* UC-06 alt-flow 7.1/SD-06 "operator requests fault clearance after
 * repair": MSG_REQUEST_FAULT_CLEAR. No payload on the wire - same reason
 * as CANCEL_OVERRIDE above. target must be a railway controller
 * (CTRL_RL1..CTRL_RL3); this function does not check that itself (see
 * c_operator.c's parse_rlx()). */
void c_comm_send_request_fault_clear(ipc_client_queue_t *q, controller_id_t target);

#endif /* C_COMM_H */
