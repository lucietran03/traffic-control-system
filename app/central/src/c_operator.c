#include <stdio.h>

#include "c_operator.h"
#include "c_comm.h"
#include "c_logger.h"

// Implements the dedicated operator console thread for blocking stdin command processing.

enum { READ_OK = 1, READ_BAD = 0, READ_EOF = -1 };

// Safely reads numeric input while temporarily releasing the console lock to prevent freezing.
static int read_long(pthread_mutex_t *console_io_lock, const char *prompt, long *out)
{
    int rc;

    printf("%s", prompt);
    fflush(stdout);

    pthread_mutex_unlock(console_io_lock);
    rc = scanf("%ld", out);
    pthread_mutex_lock(console_io_lock);

    if (rc == EOF) {
        return READ_EOF;
    }
    if (rc != 1) {
        int c;
        printf("c_operator: not a number - command aborted\n");
        while ((c = getchar()) != '\n' && c != EOF) {}
        return READ_BAD;
    }
    return READ_OK;
}

// Validates operator-entered IDs before casting them to controller_id_t.
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

// Private helper to log Central's immediate pre-check rejections for override requests.
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

// Displays the operator console command menu.
static void print_help(void)
{
    printf("C1 operator console:\n");
    printf("  m = SET_MODE for an Lx                        (UC-07 / SD-03)\n");
    printf("  t = broadcast SET_TIMING_PROFILE for R1 or R2  (UC-03 / SD-03)\n");
    printf("  o = REQUEST_OVERRIDE (clear-route) for an Lx   (UC-08 / SD-07)\n");
    printf("  r = RENEW_OVERRIDE for an Lx                   (UC-08 / SD-07)\n");
    printf("  c = CANCEL_OVERRIDE for an Lx                  (UC-08 / SD-07)\n");
    printf("  f = REQUEST_FAULT_CLEAR for an Lx or RLx       (UC-06 alt 7.1 / SD-06, SC-03A)\n");
    printf("  d = force a simulated hour (demo DP-01/DP-02 peak-hour switching on demand)\n");
    printf("  a = resume automatic (real clock) peak-hour switching\n");
    printf("  h or ? = show this help                        q = stop operator console (this thread only)\n");
}

// Handles the SET_MODE operator command, prompting for Lx and mode, then sending the request.
static void handle_set_mode(c_operator_args_t *args)
{
    long n;
    controller_id_t target;
    operating_mode_t mode;
    int idx;

    if (read_long(args->console_io_lock, "  Lx number (1-6): ", &n) != READ_OK) return;
    if (!parse_lx(n, &target)) { printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n); return; }
    
    if (read_long(args->console_io_lock, "  mode (0=PEAK_FIXED, 1=OFF_PEAK_SENSOR): ", &n) != READ_OK) return;
    if (n != MODE_PEAK_FIXED && n != MODE_OFF_PEAK_SENSOR) { printf("c_operator: %ld is not a valid mode (0 or 1) - command aborted\n", n); return; }
    mode = (operating_mode_t)n;

    pthread_mutex_lock(args->mode_eng_lock);
    idx = c_mode_eng_controller_index(target);
    if (idx >= 0) {
        args->mode_eng->controllers[idx].last_commanded_mode = mode;
    }
    pthread_mutex_unlock(args->mode_eng_lock);

    c_logger_log("Operator: SET_MODE(target=%d, mode=%s) submitted", (int)target, mode_name(mode));
    c_comm_send_set_mode(args->client_queue, target, mode);
}

// Handles the SET_TIMING_PROFILE operator command, prompting for chain selection, then broadcasting the request.
static void handle_timing_profile(c_operator_args_t *args)
{
    long n;
    const c_arterial_offset_t *chain;
    int chain_len;
    uint32_t profile_id;
    int i;

    if (read_long(args->console_io_lock, "  chain (1=R1 L1/L3/L5, 2=R2 L2/L4/L6): ", &n) != READ_OK) return;
    if (n != C_ARTERIAL_CHAIN_R1 && n != C_ARTERIAL_CHAIN_R2) { printf("c_operator: %ld is not a valid chain (1 or 2) - command aborted\n", n); return; }

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

// Handles the REQUEST_OVERRIDE operator command, prompting for Lx, movement, and duration, then sending the request.
static void handle_request_override(c_operator_args_t *args)
{
    long n;
    controller_id_t target;
    request_override_payload_t payload;
    nack_reason_t reason;
    int idx;
    int accepted;

    if (read_long(args->console_io_lock, "  Lx number (1-6): ", &n) != READ_OK) return;
    if (!parse_lx(n, &target)) { printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n); return; }

    if (read_long(args->console_io_lock, "  target movement (0=arterial, 1=connector): ", &n) != READ_OK) return;
    if (n != (long)OVERRIDE_MOVEMENT_ARTERIAL && n != (long)OVERRIDE_MOVEMENT_CONNECTOR) {
        printf("c_operator: %ld is not a valid target movement (0 or 1) - command aborted\n", n);
        return;
    }
    payload.target_movement = (uint32_t)n;
    
    if (read_long(args->console_io_lock, "  duration_ms (1-300000): ", &n) != READ_OK) return;
    payload.override_type = (uint32_t)OVERRIDE_CLEAR_ROUTE;
    payload.duration_ms    = (uint32_t)n;

    pthread_mutex_lock(args->mode_eng_lock);
    accepted = c_mode_eng_validate_override_request(target, &payload, &reason);
    if (accepted) {
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

// Handles the RENEW_OVERRIDE operator command, prompting for Lx and extension duration, then sending the request.
static void handle_renew_override(c_operator_args_t *args)
{
    long n;
    controller_id_t target;
    uint32_t extend_duration_ms;
    int idx;
    int in_flight = 0;

    if (read_long(args->console_io_lock, "  Lx number (1-6): ", &n) != READ_OK) return;
    if (!parse_lx(n, &target)) { printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n); return; }
    if (read_long(args->console_io_lock, "  extend_duration_ms (0 = keep original duration): ", &n) != READ_OK) return;
    
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

// Handles the CANCEL_OVERRIDE operator command, prompting for Lx, then sending the request.
static void handle_cancel_override(c_operator_args_t *args)
{
    long n;
    controller_id_t target;
    int idx;

    if (read_long(args->console_io_lock, "  Lx number (1-6): ", &n) != READ_OK) return;
    if (!parse_lx(n, &target)) { printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n); return; }

    pthread_mutex_lock(args->mode_eng_lock);
    idx = c_mode_eng_controller_index(target);
    if (idx >= 0) {
        args->mode_eng->controllers[idx].override_in_flight = 0;
    }
    pthread_mutex_unlock(args->mode_eng_lock);

    c_logger_log("Operator: CANCEL_OVERRIDE(target=%d) submitted", (int)target);
    c_comm_send_cancel_override(args->client_queue, target);
}

// Handles the REQUEST_FAULT_CLEAR operator command, prompting for Lx or RLx, then sending the request.
static void handle_request_fault_clear(c_operator_args_t *args)
{
    long n;
    long node_type;
    controller_id_t target;

    if (read_long(args->console_io_lock, "  Node type (0=intersection Lx, 1=railway RLx): ", &node_type) != READ_OK) return;
    
    if (node_type == 0) {
        if (read_long(args->console_io_lock, "  Lx number (1-6): ", &n) != READ_OK) return;
        if (!parse_lx(n, &target)) { printf("c_operator: %ld is not a valid Lx (1-6) - command aborted\n", n); return; }
    } else if (node_type == 1) {
        if (read_long(args->console_io_lock, "  RLx number (1-3): ", &n) != READ_OK) return;
        if (!parse_rlx(n, &target)) { printf("c_operator: %ld is not a valid RLx (1-3) - command aborted\n", n); return; }
    } else {
        printf("c_operator: %ld is not a valid node type (0 or 1) - command aborted\n", node_type);
        return;
    }

    c_logger_log("Operator: REQUEST_FAULT_CLEAR(target=%d) submitted", (int)target);
    c_comm_send_request_fault_clear(args->client_queue, target);
}

// Handles the DEMO_HOUR operator command, prompting for a simulated hour, then forcing the mode and broadcasting it.
static void handle_demo_hour(c_operator_args_t *args)
{
    long n;
    operating_mode_t mode;

    if (read_long(args->console_io_lock, "  simulated hour to force (0-23): ", &n) != READ_OK) return;
    if (n < 0 || n > 23) { printf("c_operator: %ld is not a valid hour (0-23) - command aborted\n", n); return; }

    pthread_mutex_lock(args->mode_eng_lock);
    args->mode_eng->demo_hour = (uint8_t)n;
    args->mode_eng->demo_hour_override_active = 1;
    mode = c_mode_eng_select_mode(args->mode_eng, (uint8_t)n);
    args->mode_eng->last_auto_mode       = mode;
    args->mode_eng->last_auto_mode_valid = 1;
    c_mode_eng_mark_all_lx_commanded(args->mode_eng, mode);
    pthread_mutex_unlock(args->mode_eng_lock);

    c_logger_log("Operator: demo hour forced to %ld -> schedule implies mode=%s, broadcasting to all Lx",
                 n, mode_name(mode));
    c_comm_broadcast_set_mode(args->client_queue, mode);
}

static void handle_resume_automatic(c_operator_args_t *args)
{
    pthread_mutex_lock(args->mode_eng_lock);
    args->mode_eng->demo_hour_override_active = 0;
    pthread_mutex_unlock(args->mode_eng_lock);

    c_logger_log("Operator: resuming automatic (real clock) peak-hour switching");
    printf("c_operator: resuming automatic peak-hour switching (takes effect within 1s)\n");
}

// Dedicated thread function for the operator console.
void *c_operator_reader_thread(void *arg)
{
    c_operator_args_t *args = (c_operator_args_t *)arg;
    char input;

    print_help();

    for (;;) {
        if (scanf(" %c", &input) != 1) {
            break;
        }

        switch (input) {
        case 'm': // SET_MODE
            pthread_mutex_lock(args->console_io_lock);
            handle_set_mode(args);
            pthread_mutex_unlock(args->console_io_lock);
            break;
        case 't': // SET_TIMING_PROFILE
            pthread_mutex_lock(args->console_io_lock);
            handle_timing_profile(args);
            pthread_mutex_unlock(args->console_io_lock);
            break;
        case 'o': // REQUEST_OVERRIDE
            pthread_mutex_lock(args->console_io_lock);
            handle_request_override(args);
            pthread_mutex_unlock(args->console_io_lock);
            break;
        case 'r': // RENEW_OVERRIDE
            pthread_mutex_lock(args->console_io_lock);
            handle_renew_override(args);
            pthread_mutex_unlock(args->console_io_lock);
            break;
        case 'c': // CANCEL_OVERRIDE
            pthread_mutex_lock(args->console_io_lock);
            handle_cancel_override(args);
            pthread_mutex_unlock(args->console_io_lock);
            break;
        case 'f': // REQUEST_FAULT_CLEAR
            pthread_mutex_lock(args->console_io_lock);
            handle_request_fault_clear(args);
            pthread_mutex_unlock(args->console_io_lock);
            break;
        case 'd': // DEMO_HOUR
            pthread_mutex_lock(args->console_io_lock);
            handle_demo_hour(args);
            pthread_mutex_unlock(args->console_io_lock);
            break;
        case 'a': // RESUME_AUTOMATIC
            pthread_mutex_lock(args->console_io_lock);
            handle_resume_automatic(args);
            pthread_mutex_unlock(args->console_io_lock);
            break;
        case 'h': // HELP
        case '?':
            pthread_mutex_lock(args->console_io_lock);
            print_help();
            pthread_mutex_unlock(args->console_io_lock);
            break;
        case 'q': // QUIT
            printf("c_operator: stopping operator console (this thread only)\n");
            return NULL;
        default:
            printf("c_operator: ignored key '%c'\n", input);
            break;
        }
    }

    return NULL;
}