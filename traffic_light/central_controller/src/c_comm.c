#include <string.h>

#include "c_comm.h"
#include "c_logger.h"

/*
 * Implementation notes
 * ---------------------
 * Every c_comm_send_*()/c_comm_broadcast_timing_profile() function below
 * follows the exact same four-step shape as lx_comm.c's
 * lx_comm_send_heartbeat(): memset() an ipc_request_t to zero (hdr is
 * left zeroed - ipc_client_post() "clears req.hdr to a safe (non-_IO_*)
 * value" itself per its documented contract in qnet_utils.h, so this file
 * never touches hdr), fill verb/sender_id/target_id/payload, then call
 * ipc_client_post(). sender_id is always CTRL_C1 - only Central ever
 * calls into this file.
 *
 * timestamp_ms is left at 0 on every outgoing request here, matching the
 * placeholder convention already established by lx_comm.c/rlx_comm.c:
 * no monotonic-clock helper exists anywhere in this codebase yet (a
 * known, confirmed gap), and 0 is simply what every other sender already
 * writes rather than fabricating a value nobody else provides.
 */

static const char *verb_name(uint32_t verb)
{
    switch ((msg_type_t)verb) {
    case MSG_SET_TIMING_PROFILE:  return "SET_TIMING_PROFILE";
    case MSG_SET_MODE:            return "SET_MODE";
    case MSG_REQUEST_OVERRIDE:    return "REQUEST_OVERRIDE";
    case MSG_RENEW_OVERRIDE:      return "RENEW_OVERRIDE";
    case MSG_CANCEL_OVERRIDE:     return "CANCEL_OVERRIDE";
    case MSG_REQUEST_FAULT_CLEAR: return "REQUEST_FAULT_CLEAR";
    default:                      return "UNKNOWN_VERB";
    }
}

static const char *result_name(uint32_t result)
{
    switch ((msg_result_t)result) {
    case RESULT_ACK:         return "ACK";
    case RESULT_ACK_PENDING: return "ACK_PENDING";
    case RESULT_NACK:        return "NACK";
    case RESULT_ERROR:       return "ERROR";
    default:                 return "UNKNOWN_RESULT";
    }
}

static const char *nack_reason_name(uint32_t reason)
{
    switch ((nack_reason_t)reason) {
    case NACK_REASON_NONE:                    return "NONE";
    case NACK_REASON_INVALID_DURATION:        return "INVALID_DURATION";
    case NACK_REASON_RAILWAY_CONFLICT:        return "RAILWAY_CONFLICT";
    case NACK_REASON_PEDESTRIAN_ACTIVE:       return "PEDESTRIAN_ACTIVE";
    case NACK_REASON_FAULT_ACTIVE:            return "FAULT_ACTIVE";
    case NACK_REASON_STALE_OR_UNSAFE_PROFILE: return "STALE_OR_UNSAFE_PROFILE";
    case NACK_REASON_OUT_OF_RANGE:            return "OUT_OF_RANGE";
    case NACK_REASON_UNKNOWN_TARGET:          return "UNKNOWN_TARGET";
    default:                                  return "UNKNOWN_REASON";
    }
}

/*
 * Single reply handler shared by all six senders below.
 *
 * Deliberately stateless: ctx is always NULL and this function never
 * touches c_mode_eng_t. ipc_reply_handler_t's documented contract (see
 * qnet_utils.h) is that it "Runs on the CLIENT thread, never on the
 * server thread" - c_operator.c's command handlers and c_main.c's
 * on_request()/on_pulse() already form a two-writer relationship on
 * ctx->mode_eng (server thread + operator thread), serialised by
 * central_context_t.mode_eng_lock (see c_main.c). Making this reply
 * handler a THIRD writer, from yet another thread, on a fire-and-forget
 * callback whose timing relative to a later operator command is
 * unspecified, would reopen exactly that hazard. Logging only (via
 * c_logger_log(), which owns no state this file cares about) keeps this
 * file's concurrency story identical to lx_comm.c's on_heartbeat_reply().
 */
static void on_command_reply(controller_id_t target_id, const ipc_request_t *original_req,
                              const ipc_reply_t *reply, int send_ok, void *ctx)
{
    (void)ctx;

    if (!send_ok) {
        c_logger_log("C1: %s to %d send failed (peer unreachable or send error)",
                     verb_name(original_req->verb), (int)target_id);
        return;
    }

    if ((msg_result_t)reply->result == RESULT_NACK) {
        c_logger_log("C1: %s to %d -> NACK reason=%s",
                     verb_name(original_req->verb), (int)target_id, nack_reason_name(reply->reason));
    } else {
        c_logger_log("C1: %s to %d -> %s",
                     verb_name(original_req->verb), (int)target_id, result_name(reply->result));
    }
}

void c_comm_send_set_mode(ipc_client_queue_t *q, controller_id_t target, operating_mode_t mode)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb         = MSG_SET_MODE;
    req.sender_id    = (uint32_t)CTRL_C1;
    req.target_id    = (uint32_t)target;
    req.timestamp_ms = 0;
    req.payload.mode.mode = (uint32_t)mode;

    if (ipc_client_post(q, target, &req, on_command_reply, NULL) != 0) {
        c_logger_log("C1: SET_MODE to %d dropped - outgoing queue full or stopping", (int)target);
    }
}

void c_comm_send_timing_profile(ipc_client_queue_t *q, controller_id_t target,
                                 uint32_t profile_id, uint32_t offset_ms)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb         = MSG_SET_TIMING_PROFILE;
    req.sender_id    = (uint32_t)CTRL_C1;
    req.target_id    = (uint32_t)target;
    req.timestamp_ms = 0;
    req.payload.timing_profile.profile_id = profile_id;
    req.payload.timing_profile.offset_ms  = offset_ms;

    if (ipc_client_post(q, target, &req, on_command_reply, NULL) != 0) {
        c_logger_log("C1: SET_TIMING_PROFILE to %d dropped - outgoing queue full or stopping", (int)target);
    }
}

void c_comm_broadcast_timing_profile(ipc_client_queue_t *q, const c_arterial_offset_t *chain,
                                      int chain_len, uint32_t profile_id)
{
    /* R1_CHAIN/R2_CHAIN (c_mode_eng.c) are both length 3 today; 8 is
     * generous headroom against a future longer chain, not a spec value. */
    ipc_request_t requests[8];
    int i;
    int n;

    if (chain == NULL || chain_len <= 0 || chain_len > (int)(sizeof(requests) / sizeof(requests[0]))) {
        c_logger_log("C1: SET_TIMING_PROFILE broadcast aborted - invalid chain (len=%d)", chain_len);
        return;
    }

    /* c_mode_eng_build_timing_profile() fills verb/sender_id/target_id/
     * payload for every chain member (it memset()s each entry itself) -
     * this loop only has to post what it already built. */
    n = c_mode_eng_build_timing_profile(profile_id, chain, chain_len, requests);
    for (i = 0; i < n; i++) {
        controller_id_t target = (controller_id_t)requests[i].target_id;

        if (ipc_client_post(q, target, &requests[i], on_command_reply, NULL) != 0) {
            c_logger_log("C1: SET_TIMING_PROFILE (profile %u) to %d dropped - outgoing queue full or stopping",
                         (unsigned)profile_id, (int)target);
        }
    }
}

void c_comm_send_request_override(ipc_client_queue_t *q, controller_id_t target,
                                   uint32_t override_type, uint32_t target_movement, uint32_t duration_ms)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb         = MSG_REQUEST_OVERRIDE;
    req.sender_id    = (uint32_t)CTRL_C1;
    req.target_id    = (uint32_t)target;
    req.timestamp_ms = 0;
    req.payload.override_request.override_type   = override_type;
    req.payload.override_request.target_movement  = target_movement;
    req.payload.override_request.duration_ms      = duration_ms;

    if (ipc_client_post(q, target, &req, on_command_reply, NULL) != 0) {
        c_logger_log("C1: REQUEST_OVERRIDE to %d dropped - outgoing queue full or stopping", (int)target);
    }
}

void c_comm_send_renew_override(ipc_client_queue_t *q, controller_id_t target, uint32_t extend_duration_ms)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb         = MSG_RENEW_OVERRIDE;
    req.sender_id    = (uint32_t)CTRL_C1;
    req.target_id    = (uint32_t)target;
    req.timestamp_ms = 0;
    req.payload.override_renew.extend_duration_ms = extend_duration_ms;

    if (ipc_client_post(q, target, &req, on_command_reply, NULL) != 0) {
        c_logger_log("C1: RENEW_OVERRIDE to %d dropped - outgoing queue full or stopping", (int)target);
    }
}

void c_comm_send_cancel_override(ipc_client_queue_t *q, controller_id_t target)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb         = MSG_CANCEL_OVERRIDE;
    req.sender_id    = (uint32_t)CTRL_C1;
    req.target_id    = (uint32_t)target;
    req.timestamp_ms = 0;
    /* No payload on the wire - see ipc_msg.h. */

    if (ipc_client_post(q, target, &req, on_command_reply, NULL) != 0) {
        c_logger_log("C1: CANCEL_OVERRIDE to %d dropped - outgoing queue full or stopping", (int)target);
    }
}

void c_comm_send_request_fault_clear(ipc_client_queue_t *q, controller_id_t target)
{
    ipc_request_t req;

    memset(&req, 0, sizeof(req));
    req.verb         = MSG_REQUEST_FAULT_CLEAR;
    req.sender_id    = (uint32_t)CTRL_C1;
    req.target_id    = (uint32_t)target;
    req.timestamp_ms = 0;
    /* No payload on the wire - see ipc_msg.h. */

    if (ipc_client_post(q, target, &req, on_command_reply, NULL) != 0) {
        c_logger_log("C1: REQUEST_FAULT_CLEAR to %d dropped - outgoing queue full or stopping", (int)target);
    }
}
