#include <stdio.h>

#include "c_operator.h"
#include "c_comm.h"
#include "c_logger.h"

/*
 * Operator console - implementation.
 *
 * Input handling follows lx_sensor.c's style (blocking scanf(), a printed
 * help menu, ignore-and-continue on an unrecognised command) but each
 * command here needs one or two numeric arguments on top of the command
 * letter (a target controller number, a mode, a movement code, a
 * duration...), unlike lx_sensor.c's single-keypress commands. read_long()
 * below is the shared helper for that: it prints a prompt, reads one
 * integer, and distinguishes three outcomes so a mistyped number can
 * never leave stray characters in stdin for the next scanf() call to trip
 * over (a classic pitfall of mixing "%c" and "%d" reads on the same
 * stream) or spin the loop forever.
 */

/* Outcome codes for read_long(). */
enum { READ_OK = 1, READ_BAD = 0, READ_EOF = -1 };

static int read_long(const char *prompt, long *out)
{
    int rc;

    printf("%s", prompt);
    fflush(stdout);
    rc = scanf("%ld", out);

    if (rc == EOF) {
        return READ_EOF;
    }
    if (rc != 1) {
        int c;

        printf("c_operator: not a number - command aborted\n");
        /* Discard the rest of the offending line so it can't be
         * misread as the next prompt's input. */
        while ((c = getchar()) != '\n' && c != EOF) {
            /* discard */
        }
        return READ_BAD;
    }
    return READ_OK;
}

/* Validates operator-entered controller numbers before they ever become a
 * controller_id_t - same security/correctness boundary as lx_main.c's
 * parse_self_id(), applied here to untrusted keyboard input instead of
 * argv[1]. Returns 1 and fills *out on success, 0 (leaving *out
 * untouched) if n is out of range. */
static int parse_lx(long n, controller_id_t *out)
{
    if (n < 1 || n > 6) {
        return 0;
    }
    *out = (controller_id_t)(CTRL_L1 + (n - 1));
    return 1;
}

static int parse_rlx(long n, controller_id_t *out)
{
    if (n < 1 || n > 3) {
        return 0;
    }
    *out = (controller_id_t)(CTRL_RL1 + (n - 1));
    return 1;
}

static const char *mode_name(operating_mode_t mode)
{
    return (mode == MODE_PEAK_FIXED) ? "PEAK_FIXED" : "OFF_PEAK_SENSOR";
}

/* Only needed here for logging Central's OWN pre-check rejection of a
 * REQUEST_OVERRIDE (c_mode_eng_validate_override_request() returning 0) -
 * a distinct, smaller vocabulary use from c_comm.c's own
 * verb_name()/result_name()/nack_reason_name() (which log what the
 * TARGET controller replied). Kept as its own small static copy rather
 * than exposing c_comm.c's private helpers, since the two call sites log
 * different things for different reasons. */
static const char *nack_reason_name(nack_reason_t reason)
{
    switch (reason) {
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

static void print_help(void)
{
    printf("C1 operator console:\n");
    printf("  m = SET_MODE for an Lx                        (UC-07 / SD-03)\n");
    printf("  t = broadcast SET_TIMING_PROFILE for R1 or R2  (UC-03 / SD-03)\n");
    printf("  o = REQUEST_OVERRIDE (clear-route) for an Lx   (UC-08 / SD-07)\n");
    printf("  r = RENEW_OVERRIDE for an Lx                   (UC-08 / SD-07)\n");
    printf("  c = CANCEL_OVERRIDE for an Lx                  (UC-08 / SD-07)\n");
    printf("  f = REQUEST_FAULT_CLEAR for an Lx or RLx       (UC-06 alt 7.1 / SD-06, SC-03A)\n");
    printf("  h or ? = show this help                        q = stop operator console (this thread only)\n");
}

/* --- per-command handlers ------------------------------------------- */

/* UC-07 main flow: "operator selects a target controller and enters the
 * requested mode"; SD-03's "opt operator requests a mode change" block. */
static void handle_set_mode(c_operator_args_t *args)
{
    long n;
    controller_id_t target;
    operating_mode_t mode;
    int idx;

    if (read_long("  Lx number (1-6): ", &n) != READ_OK) {
        return;
    }
    if (!parse_lx(n, &target)) {
        printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n);
        return;
    }
    if (read_long("  mode (0=PEAK_FIXED, 1=OFF_PEAK_SENSOR): ", &n) != READ_OK) {
        return;
    }
    if (n != MODE_PEAK_FIXED && n != MODE_OFF_PEAK_SENSOR) {
        printf("c_operator: %ld is not a valid mode (0 or 1) - command aborted\n", n);
        return;
    }
    mode = (operating_mode_t)n;

    /* c_mode_eng_t's last_commanded_mode is pure "what C1 last told this
     * controller" bookkeeping (see c_mode_eng.h) - it does not depend on
     * any inbound report, so it is safe to set here, under the lock,
     * regardless of whether the Lx ultimately ACKs or NACKs. */
    pthread_mutex_lock(args->mode_eng_lock);
    idx = c_mode_eng_controller_index(target);
    if (idx >= 0) {
        args->mode_eng->controllers[idx].last_commanded_mode = mode;
    }
    pthread_mutex_unlock(args->mode_eng_lock);

    c_logger_log("Operator: SET_MODE(target=%d, mode=%s) submitted", (int)target, mode_name(mode));
    c_comm_send_set_mode(args->client_queue, target, mode);
}

/* UC-03 main flow steps 1-2: "the operator submits the validated arterial
 * coordination profile to Central" / "Central supplies each relevant
 * intersection with its assigned timing offset" - one broadcast call
 * covers the whole chain (TC-01..05), never a single controller. */
static void handle_timing_profile(c_operator_args_t *args)
{
    long n;
    const c_arterial_offset_t *chain;
    int chain_len;
    uint32_t profile_id;
    int i;

    if (read_long("  chain (1=R1 L1/L3/L5, 2=R2 L2/L4/L6): ", &n) != READ_OK) {
        return;
    }
    if (n != C_ARTERIAL_CHAIN_R1 && n != C_ARTERIAL_CHAIN_R2) {
        printf("c_operator: %ld is not a valid chain (1 or 2) - command aborted\n", n);
        return;
    }

    pthread_mutex_lock(args->mode_eng_lock);
    chain = c_mode_eng_get_chain((c_arterial_chain_id_t)n, &chain_len);
    if (chain == NULL) {
        pthread_mutex_unlock(args->mode_eng_lock);
        printf("c_operator: internal error resolving chain %ld - command aborted\n", n);
        return;
    }
    profile_id = c_mode_eng_next_profile_id(args->mode_eng);
    for (i = 0; i < chain_len; i++) {
        int idx = c_mode_eng_controller_index(chain[i].id);
        if (idx >= 0) {
            args->mode_eng->controllers[idx].last_applied_profile_id = profile_id;
        }
    }
    pthread_mutex_unlock(args->mode_eng_lock);

    c_logger_log("Operator: SET_TIMING_PROFILE broadcast (chain=%s, profile_id=%u) submitted",
                 (n == C_ARTERIAL_CHAIN_R1) ? "R1" : "R2", (unsigned)profile_id);
    c_comm_broadcast_timing_profile(args->client_queue, chain, chain_len, profile_id);
}

/* UC-08 main flow / SD-07 steps 1-2: "the operator selects the target
 * through-movement and override duration" / REQUEST_OVERRIDE(CLEAR_ROUTE,
 * target, duration). Runs the request through
 * c_mode_eng_validate_override_request() first (PA-11 surface check) -
 * per that function's own doc comment, a request Central rejects outright
 * is never even forwarded to the Lx. */
static void handle_request_override(c_operator_args_t *args)
{
    long n;
    controller_id_t target;
    request_override_payload_t payload;
    nack_reason_t reason;
    int idx;
    int accepted;

    if (read_long("  Lx number (1-6): ", &n) != READ_OK) {
        return;
    }
    if (!parse_lx(n, &target)) {
        printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n);
        return;
    }
    if (read_long("  target movement (0=arterial, 1=connector): ", &n) != READ_OK) {
        return;
    }
    if (n != (long)OVERRIDE_MOVEMENT_ARTERIAL && n != (long)OVERRIDE_MOVEMENT_CONNECTOR) {
        /* Re-audit finding: this used to store n verbatim - sys_types.h's
         * override_movement_t doc comment says only these two values are
         * ever placed in target_movement. Lx's guards only ever test
         * "== OVERRIDE_MOVEMENT_CONNECTOR" (see lx_fsm.c), so an
         * out-of-range value silently fell back to arterial instead of
         * being rejected here at the source. */
        printf("c_operator: %ld is not a valid target movement (0 or 1) - command aborted\n", n);
        return;
    }
    payload.target_movement = (uint32_t)n;
    if (read_long("  duration_ms (1-300000): ", &n) != READ_OK) {
        return;
    }
    payload.override_type = (uint32_t)OVERRIDE_CLEAR_ROUTE;
    payload.duration_ms    = (uint32_t)n;

    pthread_mutex_lock(args->mode_eng_lock);
    accepted = c_mode_eng_validate_override_request(target, &payload, &reason);
    if (accepted) {
        /* Optimistic, Central-side-only bookkeeping (c_mode_eng.h:
         * "whether an override is still outstanding so C1 doesn't issue
         * a second REQUEST_OVERRIDE ... on top of one already
         * pending/active"). Not synchronised with the Lx's own eventual
         * ACK/NACK - c_server.c owns the authoritative
         * last_reported_override_active field once the Lx reports back
         * via STATUS/HEARTBEAT, but updating that on this path is out of
         * this module's scope (c_server.c is not one of the files owned
         * here). Cleared optimistically by handle_cancel_override()
         * below.
         */
        idx = c_mode_eng_controller_index(target);
        if (idx >= 0) {
            args->mode_eng->controllers[idx].override_in_flight = 1;
        }
    }
    pthread_mutex_unlock(args->mode_eng_lock);

    if (!accepted) {
        c_logger_log("Operator: REQUEST_OVERRIDE(target=%d, movement=%u, duration_ms=%u) rejected by Central "
                     "pre-check, reason=%s - not forwarded to the controller",
                     (int)target, (unsigned)payload.target_movement, (unsigned)payload.duration_ms,
                     nack_reason_name(reason));
        return;
    }

    c_logger_log("Operator: REQUEST_OVERRIDE(target=%d, movement=%u, duration_ms=%u) submitted",
                 (int)target, (unsigned)payload.target_movement, (unsigned)payload.duration_ms);
    c_comm_send_request_override(args->client_queue, target, payload.override_type,
                                  payload.target_movement, payload.duration_ms);
}

/* UC-08/SD-07 "operator requests a renewal before expiry":
 * RENEW_OVERRIDE(extend_duration_ms). BR-7: "A renewal request is
 * independently validated and may be rejected with NACK without
 * affecting the existing expiry time" - all of that revalidation is the
 * target Lx's job (lx_fsm_on_renew_override()), not Central's, so this
 * handler does not pre-validate; it only warns (non-fatally) if Central's
 * own bookkeeping does not show an override in flight for this target. */
static void handle_renew_override(c_operator_args_t *args)
{
    long n;
    controller_id_t target;
    uint32_t extend_duration_ms;
    int idx;
    int in_flight = 0;

    if (read_long("  Lx number (1-6): ", &n) != READ_OK) {
        return;
    }
    if (!parse_lx(n, &target)) {
        printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n);
        return;
    }
    if (read_long("  extend_duration_ms (0 = keep original duration): ", &n) != READ_OK) {
        return;
    }
    extend_duration_ms = (uint32_t)n;

    pthread_mutex_lock(args->mode_eng_lock);
    idx = c_mode_eng_controller_index(target);
    if (idx >= 0) {
        in_flight = args->mode_eng->controllers[idx].override_in_flight;
    }
    pthread_mutex_unlock(args->mode_eng_lock);

    if (!in_flight) {
        printf("c_operator: warning - Central has no override recorded in flight for %d; "
               "sending anyway, the controller has the authoritative answer\n", (int)target);
    }

    c_logger_log("Operator: RENEW_OVERRIDE(target=%d, extend_duration_ms=%u) submitted",
                 (int)target, (unsigned)extend_duration_ms);
    c_comm_send_renew_override(args->client_queue, target, extend_duration_ms);
}

/* UC-08/SD-07 "operator cancels active override": CANCEL_OVERRIDE. */
static void handle_cancel_override(c_operator_args_t *args)
{
    long n;
    controller_id_t target;
    int idx;

    if (read_long("  Lx number (1-6): ", &n) != READ_OK) {
        return;
    }
    if (!parse_lx(n, &target)) {
        printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n);
        return;
    }

    pthread_mutex_lock(args->mode_eng_lock);
    idx = c_mode_eng_controller_index(target);
    if (idx >= 0) {
        args->mode_eng->controllers[idx].override_in_flight = 0;
    }
    pthread_mutex_unlock(args->mode_eng_lock);

    c_logger_log("Operator: CANCEL_OVERRIDE(target=%d) submitted", (int)target);
    c_comm_send_cancel_override(args->client_queue, target);
}

/* UC-06 alt-flow 7.1/SD-06 "operator requests fault clearance after
 * repair": REQUEST_FAULT_CLEAR. Can target either a railway controller
 * (RC-09: "Central may request fault clearance but may not actuate
 * railway equipment" - RLx re-verifies live gate state itself) or an
 * intersection controller (SC-03A - test-plan finding: lx_fsm previously
 * had no way to recover from FAULT_SAFE short of a process restart; now
 * wired up symmetrically via lx_fsm_on_request_fault_clear()). */
static void handle_request_fault_clear(c_operator_args_t *args)
{
    long n;
    long node_type;
    controller_id_t target;

    if (read_long("  Node type (0=intersection Lx, 1=railway RLx): ", &node_type) != READ_OK) {
        return;
    }
    if (node_type == 0) {
        if (read_long("  Lx number (1-6): ", &n) != READ_OK) {
            return;
        }
        if (!parse_lx(n, &target)) {
            printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n);
            return;
        }
    } else if (node_type == 1) {
        if (read_long("  RLx number (1-3): ", &n) != READ_OK) {
            return;
        }
        if (!parse_rlx(n, &target)) {
            printf("c_operator: %ld is not a valid RLx (1-3) - command aborted\n", n);
            return;
        }
    } else {
        printf("c_operator: %ld is not a valid node type (0 or 1) - command aborted\n", node_type);
        return;
    }

    c_logger_log("Operator: REQUEST_FAULT_CLEAR(target=%d) submitted", (int)target);
    c_comm_send_request_fault_clear(args->client_queue, target);
}

/* --- reader thread ---------------------------------------------------- */

void *c_operator_reader_thread(void *arg)
{
    c_operator_args_t *args = (c_operator_args_t *)arg;
    char input;

    print_help();

    for (;;) {
        if (scanf(" %c", &input) != 1) {
            /* EOF or stdin closed - stop this thread rather than spin,
             * same convention as lx_sensor_reader_thread()/
             * rlx_sensor_reader_thread(). */
            break;
        }

        switch (input) {
        case 'm':
            handle_set_mode(args);
            break;
        case 't':
            handle_timing_profile(args);
            break;
        case 'o':
            handle_request_override(args);
            break;
        case 'r':
            handle_renew_override(args);
            break;
        case 'c':
            handle_cancel_override(args);
            break;
        case 'f':
            handle_request_fault_clear(args);
            break;
        case 'h':
        case '?':
            print_help();
            break;
        case 'q':
            printf("c_operator: stopping operator console (this thread only)\n");
            return NULL;
        default:
            printf("c_operator: ignored key '%c'\n", input);
            break;
        }
    }

    return NULL;
}
