#include "qnet_utils.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dispatch.h>
#include <sys/neutrino.h>
#include <sys/netmgr.h>

/* Index == controller_id_t value; keep in sync with sys_types.h. */
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

const char *ipc_attach_name(controller_id_t id)
{
    if (id < 0 || id >= CTRL_UNKNOWN) {
        return NULL;
    }
    return ATTACH_SUFFIX[id];
}

static int build_path(controller_id_t id, char *path, size_t path_size)
{
    const char *suffix = ipc_attach_name(id);
    if (suffix == NULL) {
        return -1;
    }
    snprintf(path, path_size, "%s/%s", TRAFFIC_NAME_PREFIX, suffix);
    return 0;
}

/* --- cross-node resolution: TRAFFIC_NODE_MAP env var -------------------
 *
 * Which physical/virtual Qnet node hosts which controller_id_t is a
 * deployment-time decision (see docs/QNX_DEPLOYMENT_RUN_GUIDE.md's
 * single/dual/tri-host topologies) - qnet_utils.c cannot know it at
 * compile time. TRAFFIC_NODE_MAP lets the deployer supply it at process
 * start without recompiling: a comma-separated list of
 * "<suffix>=<qnet-nodename>" pairs, e.g.
 *
 *   TRAFFIC_NODE_MAP="c1=VM_x86_Target01,l1=VM_x86_Target02,l2=VM_x86_Target02,\
 *l3=VM_x86_Target02,l4=VM_x86_Target02,l5=VM_x86_Target02,l6=VM_x86_Target02,\
 *rl1=VM_x86_Target03,rl2=VM_x86_Target03,rl3=VM_x86_Target03"
 *
 * matching the Case-1/Case-3 "one VM per role" topology in the deployment
 * guide (repeat the same node name for every Lx/RLx suffix that shares a
 * host). <suffix> must exactly match ipc_attach_name()'s output (lowercase
 * "c1".."rl3"). Any suffix NOT present in the map keeps today's behavior:
 * plain same-node name_open(), no /net/ prefix. This means an unset (or
 * absent) TRAFFIC_NODE_MAP reproduces the exact pre-existing single-node
 * behavior for every target - zero risk to the current same-machine test
 * setup.
 *
 * Parsed lazily, once, on first resolve_node() call (pthread_once) and
 * cached in node_map[] for the life of the process - only
 * ipc_client_thread_main()'s single dedicated thread ever calls
 * resolve_node(), so there is no real race, but pthread_once costs
 * nothing and documents the "read once" intent explicitly.
 */

#define TRAFFIC_NODE_ENV      "TRAFFIC_NODE_MAP"
#define TRAFFIC_NODE_NAME_MAX 64
#define TRAFFIC_NODE_MAP_BUF  512

typedef struct {
    int  set;
    char node[TRAFFIC_NODE_NAME_MAX];
} node_map_entry_t;

static node_map_entry_t node_map[CTRL_UNKNOWN];
static pthread_once_t   node_map_once = PTHREAD_ONCE_INIT;

/* Reverse lookup against the same ATTACH_SUFFIX table build_path() reads,
 * so the env var's keys and the wire attach-point suffixes can never
 * drift apart - there is still exactly one place (ATTACH_SUFFIX) that
 * knows the suffix strings. */
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

static void node_map_load(void)
{
    char        buf[TRAFFIC_NODE_MAP_BUF];
    const char *env;
    char       *entry;

    env = getenv(TRAFFIC_NODE_ENV);
    if (env == NULL || env[0] == '\0') {
        return; /* Not configured: node_map[] stays all-zero, so
                  * resolve_node() returns NULL for every id and
                  * build_open_path() falls back to the exact same-node
                  * path it always built. */
    }

    /* Copy into a fixed local buffer and hand-roll the split instead of
     * strdup()/strtok_r() - avoids depending on a POSIX feature-test
     * macro being defined for this translation unit just to get a
     * heap-duplicate/reentrant-tokenize helper. Silently truncates an
     * absurdly long value (drops trailing entries, never crashes). */
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
            /* Unknown suffix or empty node name: skip this entry only,
             * keep parsing the rest of the list rather than aborting. */
        }

        entry = (comma != NULL) ? comma + 1 : NULL;
    }
}

/* Returns the configured Qnet node name for id, or NULL if TRAFFIC_NODE_MAP
 * is unset/doesn't mention id - callers must treat NULL as "same node". */
static const char *resolve_node(controller_id_t id)
{
    pthread_once(&node_map_once, node_map_load);
    if (id < 0 || id >= CTRL_UNKNOWN || !node_map[id].set) {
        return NULL;
    }
    return node_map[id].node;
}

/* Client-side path builder: same TRAFFIC_NAME_PREFIX "/" <suffix> string
 * build_path() produces for the attach side, optionally wrapped with the
 * Qnet "/net/<node>/dev/name/global/" prefix when TRAFFIC_NODE_MAP names
 * a node for `id`. Never duplicates the naming convention - always calls
 * build_path() first and only adds a prefix in front of its result. */
static int build_open_path(controller_id_t id, char *path, size_t path_size)
{
    char        local_path[32];
    const char *node;

    if (build_path(id, local_path, sizeof(local_path)) == -1) {
        return -1;
    }

    node = resolve_node(id);
    if (node == NULL) {
        /* Default / unconfigured: byte-for-byte what this codebase did
         * before this change - plain same-node name_open(path, 0), no
         * /net/ prefix. This is also why it stays correct even though
         * ipc_attach() below now registers with NAME_FLAG_ATTACH_GLOBAL
         * instead of the old flags-0 (local-only) call: per QNX Neutrino
         * name-service semantics, a name registered in the GLOBAL
         * namespace is still found by a plain, prefix-less name_open()
         * from a process on the SAME node - global vs. local only
         * changes whether *other* Qnet nodes can see the name, not
         * whether the owning node can still see its own name the old
         * way. So single-machine / same-node testing is unaffected.
         */
        snprintf(path, path_size, "%s", local_path);
    } else {
        /* Cross-node: "/net/<nodename>/dev/name/global/<path>" is QNX's
         * standard Qnet path for reaching a name registered with
         * NAME_FLAG_ATTACH_GLOBAL on another node - the same
         * "/net/<nodename>/dev/name/<namespace>/<name>" shape
         * Lecture/lap_6/Lab_06_Task1b_client_602.c hardcodes for the
         * LOCAL namespace ("/net/VM_x86_Target01/dev/name/local/thang"),
         * with "local" swapped for "global" to match the namespace
         * ipc_attach() actually registers into. <nodename> always comes
         * from TRAFFIC_NODE_MAP, never guessed or hardcoded here. */
        snprintf(path, path_size, "/net/%s/dev/name/global/%s", node, local_path);
    }
    return 0;
}

int ipc_attach(controller_id_t self_id)
{
    char path[32];
    name_attach_t *attach;

    if (build_path(self_id, path, sizeof(path)) == -1) {
        return -1;
    }

    /* NAME_FLAG_ATTACH_GLOBAL (was: flags 0, i.e. local-only) registers
     * this name under /dev/name/global instead of /dev/name/local, which
     * is what makes it reachable from other Qnet nodes at all via
     * "/net/<this-node>/dev/name/global/<path>" (see build_open_path()).
     * Per QNX Neutrino's name_attach() semantics this does NOT remove
     * the name from being found locally too - a same-node name_open()
     * with no /net/ prefix still resolves it exactly as before, so this
     * is additive, not a behavior change, for every node that never sets
     * TRAFFIC_NODE_MAP. NAME_FLAG_ATTACH_GLOBAL's exact numeric value is
     * not something this file hardcodes - it comes from the real QNX
     * <sys/neutrino.h> on-target; only the host syntax-check stub
     * (tools/host_syntax_stubs/sys/neutrino.h, used by `make check-syntax`)
     * needs to fake a value for it, and that stub file says so inline. */
    /* Try NAME_FLAG_ATTACH_GLOBAL first (if global name server 'gns' is running).
     * If gns is not running (default on standard QNX VMs), gracefully fall back
     * to local namespace (flags = 0), which registers under /dev/name/local
     * and is also accessible via Qnet (/net/<node>/dev/name/local/...). */
    attach = name_attach(NULL, path, NAME_FLAG_ATTACH_GLOBAL);
    if (attach == NULL) {
        attach = name_attach(NULL, path, 0);
    }
    if (attach == NULL) {
        return -1;
    }
    return attach->chid;
}

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

int ipc_server_run(int chid, ipc_request_handler_t on_request, ipc_pulse_handler_t on_pulse, void *ctx)
{
    ipc_request_t msg;
    ipc_reply_t   reply;
    int           rcvid;

    for (;;) {
        rcvid = MsgReceive(chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (rcvid == 0) {
            /* Pulse: kernel-generated (_PULSE_CODE_DISCONNECT) or one of
             * our own IPC_PULSE_* timer codes. Never MsgReply() a pulse. */
            if (on_pulse != NULL) {
                on_pulse(msg.hdr.code, ctx);
            }
            continue;
        }

        /* Real message: handle the two reserved ranges before touching
         * our own payload, exactly as Lab_06_Task1a_server.c does. */
        if (msg.hdr.type == _IO_CONNECT) {
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

/* --- client queue: fixed-capacity ring buffer, mutex + condvar -------- */

#define IPC_CLIENT_QUEUE_CAPACITY 16

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

void ipc_client_queue_destroy(ipc_client_queue_t *q)
{
    if (q == NULL) {
        return;
    }
    /* Caller must have already pthread_join()'d the client thread - see
     * qnet_utils.h. Safe to tear down the sync primitives unconditionally. */
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    free(q);
}

int ipc_client_post(ipc_client_queue_t *q, controller_id_t target_id, const ipc_request_t *req,
                     ipc_reply_handler_t on_reply, void *ctx)
{
    int tail;

    if (q == NULL || req == NULL) {
        return -1;
    }

    pthread_mutex_lock(&q->lock);

    if (q->stopping || q->count == IPC_CLIENT_QUEUE_CAPACITY) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }

    tail = (q->head + q->count) % IPC_CLIENT_QUEUE_CAPACITY;
    q->jobs[tail].target_id        = target_id;
    q->jobs[tail].req              = *req;
    q->jobs[tail].req.hdr.type     = 0; /* keep out of the _IO_* reserved range */
    q->jobs[tail].req.hdr.subtype  = 0;
    q->jobs[tail].on_reply         = on_reply;
    q->jobs[tail].reply_ctx        = ctx;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

/* Big enough for "/net/" + a 63-char TRAFFIC_NODE_MAP node name +
 * "/dev/name/global/" + the longest build_path() output ("traffic/rl1",
 * 11 bytes) + a NUL, with headroom - see build_open_path(). The plain
 * same-node case ("traffic/rl1") only ever needs a few bytes of this. */
#define IPC_OPEN_PATH_MAX 160

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

        /* The blocking name_open()/MsgSend() happens ONLY on this
         * dedicated thread - never on the server thread (ipc_server_run())
         * and never on whichever thread called ipc_client_post(). */
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

        if (job.on_reply != NULL) {
            job.on_reply(job.target_id, &job.req, send_ok ? &reply : NULL, send_ok, job.reply_ctx);
        }
    }

    return NULL;
}
