#ifndef C_OPERATOR_H
#define C_OPERATOR_H

#include <pthread.h>

#include "sys_types.h"
#include "qnet_utils.h"
#include "c_mode_eng.h"

/*
 * Operator console - stand-in for the real Control Room Operator UI
 * (Op actor in SD-03/SD-06/SD-07), same role for c_main.c that
 * lx_sensor.c's keyboard thread plays for simulated field sensors: reads
 * blocking stdin on its own dedicated thread (never the server or client
 * thread - see qnet_utils.h's "Threading pattern") and turns each
 * operator command into a c_comm.c send.
 *
 * c_operator_args_t lives in main()'s stack frame for the whole process
 * life, exactly like lx_main.c's lx_watchdog_args_t - main() blocks
 * forever in ipc_server_run(), so the pointers below stay valid for as
 * long as c_operator_reader_thread() runs.
 *
 * mode_eng_lock: c_mode_eng_t was originally touched by exactly one
 * thread (the server thread, sequentially through ipc_server_run()'s
 * MsgReceive() loop - see c_main.c's central_context_t doc comment,
 * which flagged a mutex as still-TODO). This operator thread is a SECOND
 * thread that now reads c_mode_eng_t (c_mode_eng_controller_index(),
 * c_mode_eng_validate_override_request(), c_mode_eng_next_profile_id(),
 * c_mode_eng_get_chain()) and writes small per-controller bookkeeping
 * fields (last_commanded_mode, last_applied_profile_id,
 * override_in_flight). mode_eng_lock is what makes that safe - c_main.c
 * takes the same lock around on_request()/on_pulse()'s own touches of
 * ctx->mode_eng. Held only for the short read-modify-write around a
 * single operator command; never across the non-blocking
 * ipc_client_post() call itself (c_comm.c's senders), which needs no
 * lock at all - they only ever touch a stack-local ipc_request_t.
 */
typedef struct {
    ipc_client_queue_t *client_queue;
    c_mode_eng_t        *mode_eng;
    pthread_mutex_t      *mode_eng_lock;
} c_operator_args_t;

/*
 * Entry point for the dedicated operator-console thread:
 *   pthread_create(&tid, NULL, c_operator_reader_thread, &args);
 *
 * Prints a help menu on start (and again on 'h'/'?', matching lx_sensor.c/
 * rlx_sensor.c's convention), then loops reading one command character
 * at a time plus whatever numeric arguments that command needs. 'q'
 * stops this thread only (server/client threads are unaffected, same as
 * lx_sensor.c's 'q'). Returns NULL either on 'q' or when stdin reaches
 * EOF/closes (mirrors lx_sensor_reader_thread()'s scanf-return-value
 * check).
 *
 * Command set (UC-03, UC-06 alt 7.1, UC-07, UC-08 - see this file's .c
 * for the exact per-command traceability comments):
 *   m = SET_MODE for an Lx                         (UC-07, SD-03 opt block)
 *   t = broadcast SET_TIMING_PROFILE for R1 or R2  (UC-03, SD-03)
 *   o = REQUEST_OVERRIDE (clear-route) for an Lx    (UC-08, SD-07)
 *   r = RENEW_OVERRIDE for an Lx                    (UC-08, SD-07)
 *   c = CANCEL_OVERRIDE for an Lx                   (UC-08, SD-07)
 *   f = REQUEST_FAULT_CLEAR for an RLx              (UC-06 alt 7.1, SD-06)
 *   h / ? = show the help menu again
 *   q = stop operator console (this thread only)
 */
void *c_operator_reader_thread(void *arg);

#endif /* C_OPERATOR_H */
