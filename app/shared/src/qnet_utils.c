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

#include "qnet_utils.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <sys/netmgr.h>

// Valid suffix mappings aligned with controller_id_t indices.
static const char *const ATTACH_SUFFIX[CTRL_UNKNOWN] = {
    [CTRL_C1]  = "c1",
    [CTRL_L1]  = "l1",
    [CTRL_L2]  = "l2",
    [CTRL_L3]  = "l3",
    [CTRL_L4]  = "l4",
    [CTRL_L5]  = "l5",
    [CTRL_L6]  = "l6",
    [CTRL_RL1] = "rl1",
    [CTRL_RL2] = "rl2",
    [CTRL_RL3] = "rl3"
};

// Returns the lowercase attach-point suffix for a given controller ID, or NULL if invalid.
const char *ipc_attach_name(controller_id_t id)
{
    if (id < 0 || id >= CTRL_UNKNOWN) {
        return NULL;
    }
    return ATTACH_SUFFIX[id];
}

// Formats the base local attach path (e.g., traffic/c1).
static int build_path(controller_id_t id, char *path, size_t path_size)
{
    const char *suffix = ipc_attach_name(id);
    if (suffix == NULL) {
        return -1;
    }
    snprintf(path, path_size, "%s/%s", TRAFFIC_NAME_PREFIX, suffix);
    return 0;
}

// --- cross-node resolution: TRAFFIC_NODE_MAP env var -------------------

#define TRAFFIC_NODE_ENV      "TRAFFIC_NODE_MAP"
#define TRAFFIC_NODE_NAME_MAX 64 // Maximum length of a node name 
#define TRAFFIC_NODE_MAP_BUF  512 // Maximum length of the TRAFFIC_NODE_MAP env var

typedef struct {
    int  set; 
    char node[TRAFFIC_NODE_NAME_MAX];
} node_map_entry_t;

static node_map_entry_t node_map[CTRL_UNKNOWN];
static pthread_once_t   node_map_once = PTHREAD_ONCE_INIT;

// Parses ID from a given string suffix length.
static controller_id_t suffix_to_id(const char *suffix, size_t len)
{
    int i;
    for (i = 0; i < CTRL_UNKNOWN; i++) {
        if (ATTACH_SUFFIX[i] != NULL
            && strlen(ATTACH_SUFFIX[i]) == len
            && strncmp(ATTACH_SUFFIX[i], suffix, len) == 0) {
            return (controller_id_t)i;
        }
    }
    return CTRL_UNKNOWN;
}

// Reads TRAFFIC_NODE_MAP to map logical suffixes to physical Qnet node locations.
static void node_map_load(void)
{
    char        buf[TRAFFIC_NODE_MAP_BUF];
    const char *env;
    char       *entry;

    env = getenv(TRAFFIC_NODE_ENV);
    if (env == NULL || env[0] == '\0') {
        return; 
    }

    strncpy(buf, env, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    entry = buf;
    while (entry != NULL && *entry != '\0') {
        char  *comma = strchr(entry, ',');
        char  *eq;

        if (comma != NULL) {
            *comma = '\0';
        }

        eq = strchr(entry, '=');
        if (eq != NULL && eq != entry && eq[1] != '\0') {
            controller_id_t id = suffix_to_id(entry, (size_t)(eq - entry));
            if (id != CTRL_UNKNOWN) {
                strncpy(node_map[id].node, eq + 1, TRAFFIC_NODE_NAME_MAX - 1);
                node_map[id].node[TRAFFIC_NODE_NAME_MAX - 1] = '\0';
                node_map[id].set = 1;
            }
        }
        entry = (comma != NULL) ? comma + 1 : NULL;
    }
}

// Retrieves resolved node name for a specific controller ID, defaulting to NULL (local).
static const char *resolve_node(controller_id_t id)
{
    pthread_once(&node_map_once, node_map_load);
    if (id < 0 || id >= CTRL_UNKNOWN || !node_map[id].set) {
        return NULL;
    }
    return node_map[id].node;
}

// Builds the final Qnet name path to connect globally or defaults locally if unspecified.
static int build_open_path(controller_id_t id, char *path, size_t path_size)
{
    char        local_path[32];
    const char *node;

    if (build_path(id, local_path, sizeof(local_path)) == -1) {
        return -1;
    }

    node = resolve_node(id);
    if (node == NULL) { // Local node: "/dev/name/global/<path>" is reachable via Qnet as "/net/<this-node>/dev/name/global/<path>".
        snprintf(path, path_size, "%s", local_path);
    } else { // Remote node: "/net/<node>/dev/name/global/<path>" is the correct Qnet path.
        snprintf(path, path_size, "/net/%s/dev/name/global/%s", node, local_path);
    }
    return 0;
}

// Attaches the node in the global namespace (or falls back to local on failure).
int ipc_attach(controller_id_t self_id)
{
    char path[32];
    name_attach_t *attach;

    if (build_path(self_id, path, sizeof(path)) == -1) {
        return -1;
    }

    // First try to attach globally, then fall back to local if that fails.
    attach = name_attach(NULL, path, NAME_FLAG_ATTACH_GLOBAL);
    if (attach == NULL) {
        attach = name_attach(NULL, path, 0);
    }
    if (attach == NULL) {
        return -1;
    }
    return attach->chid;
}

// Configures and initializes a SIGEV_PULSE timer to trigger at a specific cadence.
int ipc_timer_arm(int chid, int pulse_code, uint32_t initial_ms, uint32_t period_ms, timer_t *out_timer_id)
{
    struct sigevent    event;
    struct itimerspec  spec;
    struct sched_param th_param;
    int                coid;

    coid = ConnectAttach(ND_LOCAL_NODE, 0, chid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1) {
        return -1;
    }

    pthread_getschedparam(pthread_self(), NULL, &th_param);

    event.sigev_notify   = SIGEV_PULSE;
    event.sigev_coid     = coid;
    event.sigev_priority = th_param.sched_curpriority;
    event.sigev_code     = pulse_code;

    if (timer_create(CLOCK_REALTIME, &event, out_timer_id) == -1) {
        ConnectDetach(coid);
        return -1;
    }

    spec.it_value.tv_sec     = initial_ms / 1000;
    spec.it_value.tv_nsec    = (long)(initial_ms % 1000) * 1000000L;
    spec.it_interval.tv_sec  = period_ms / 1000;
    spec.it_interval.tv_nsec = (long)(period_ms % 1000) * 1000000L;

    if (timer_settime(*out_timer_id, 0, &spec, NULL) == -1) {
        timer_delete(*out_timer_id);
        ConnectDetach(coid);
        return -1;
    }

    return 0;
}

// Indefinite server loop for receiving, delegating, and replying to messages and pulses.
int ipc_server_run(int chid, ipc_request_handler_t on_request, ipc_pulse_handler_t on_pulse, void *ctx)
{
    ipc_request_t msg;
    ipc_reply_t   reply;
    int           rcvid;

    for (;;) {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1) { // Interrupted by a signal
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (rcvid == 0) { // Pulse received
            if (on_pulse != NULL) {
                on_pulse(msg.hdr.code, ctx);
            }
            continue;
        }

        if (msg.hdr.type == _IO_CONNECT) { // Connection request
            MsgReply(rcvid, EOK, NULL, 0);
            continue;
        }
        if (msg.hdr.type > _IO_BASE && msg.hdr.type <= _IO_MAX) {
            MsgError(rcvid, ENOSYS);
            continue;
        }

        memset(&reply, 0, sizeof(reply));
        if (on_request != NULL) {
            on_request(&msg, &reply, ctx);
        }
        MsgReply(rcvid, EOK, &reply, sizeof(reply));
    }
}

// --- client queue: fixed-capacity ring buffer, mutex + condvar -------- //

#define IPC_CLIENT_QUEUE_CAPACITY 16

// Context container for tracking outgoing requests in the queue.
typedef struct {
    controller_id_t      target_id;
    ipc_request_t        req;
    ipc_reply_handler_t  on_reply;
    void                 *reply_ctx;
} ipc_client_job_t;

struct ipc_client_queue {
    ipc_client_job_t jobs[IPC_CLIENT_QUEUE_CAPACITY];
    int              head;
    int              count;
    pthread_mutex_t  lock;
    pthread_cond_t   not_empty;
    int              stopping;
};

// Initialises and provisions a new IPC client message queue.
ipc_client_queue_t *ipc_client_queue_create(void)
{
    ipc_client_queue_t *q = calloc(1, sizeof(*q));
    if (q == NULL) {
        return NULL;
    }
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    return q;
}

// Cleans up the queue locks/vars once threads have fully joined.
void ipc_client_queue_destroy(ipc_client_queue_t *q)
{
    if (q == NULL) {
        return;
    }
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    free(q); // Free the allocated memory for the queue
}

// Enqueues a message structure for processing by the client thread.
int ipc_client_post(ipc_client_queue_t *q, controller_id_t target_id, const ipc_request_t *req, ipc_reply_handler_t on_reply, void *ctx)
{
    int tail;

    if (q == NULL || req == NULL) {
        return -1;
    }

    pthread_mutex_lock(&q->lock);

    // If the queue is stopping or full, reject the request.
    if (q->stopping || q->count == IPC_CLIENT_QUEUE_CAPACITY) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }

    // Enqueue the job at the tail of the ring buffer.
    tail = (q->head + q->count) % IPC_CLIENT_QUEUE_CAPACITY;
    q->jobs[tail].target_id        = target_id;
    q->jobs[tail].req              = *req;
    q->jobs[tail].req.hdr.type     = 0; // keep out of the _IO_* reserved range 
    q->jobs[tail].req.hdr.subtype  = 0;
    q->jobs[tail].on_reply         = on_reply;
    q->jobs[tail].reply_ctx        = ctx;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

#define IPC_OPEN_PATH_MAX 160

// Continuously digests the message queue via a blocking name_open/MsgSend cycle.
void *ipc_client_thread_main(void *arg)
{
    ipc_client_queue_t *q = (ipc_client_queue_t *)arg;
    ipc_client_job_t    job;
    ipc_reply_t          reply;
    char                 path[IPC_OPEN_PATH_MAX];
    int                  coid;
    int                  send_ok;

    for (;;) {
        pthread_mutex_lock(&q->lock);
        while (q->count == 0 && !q->stopping) {
            pthread_cond_wait(&q->not_empty, &q->lock);
        }
        if (q->count == 0 && q->stopping) {
            pthread_mutex_unlock(&q->lock);
            break;
        }
        job = q->jobs[q->head];
        q->head = (q->head + 1) % IPC_CLIENT_QUEUE_CAPACITY;
        q->count--;
        pthread_mutex_unlock(&q->lock);

        // Attempt to open the target node and send the request.
        send_ok = 0;
        if (build_open_path(job.target_id, path, sizeof(path)) == 0) {
            coid = name_open(path, 0);
            if (coid == -1 && strstr(path, "/dev/name/global/") != NULL) {
                char alt_path[IPC_OPEN_PATH_MAX];
                char *g = strstr(path, "/dev/name/global/");
                snprintf(alt_path, sizeof(alt_path), "%.*s/dev/name/local/%s",
                         (int)(g - path), path, g + strlen("/dev/name/global/"));
                coid = name_open(alt_path, 0);
            }
            if (coid != -1) {
                send_ok = (MsgSend(coid, &job.req, sizeof(job.req), &reply, sizeof(reply)) != -1);
                name_close(coid);
            }
        }

        // Notify the caller of the result.
        if (job.on_reply != NULL) {
            job.on_reply(job.target_id, &job.req, send_ok ? &reply : NULL, send_ok, job.reply_ctx);
        }
    }

    return NULL;
}
