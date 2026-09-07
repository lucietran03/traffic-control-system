#ifndef QNET_UTILS_H
#define QNET_UTILS_H

#include <stdint.h>
#include <time.h>
#include <sys/neutrino.h>
#include "sys_types.h"
#include "ipc_msg.h"

/*
 * Qnet attach-point naming convention.
 *
 * Every node calls ipc_attach() exactly once at startup, registering
 * itself as TRAFFIC_NAME_PREFIX "/" <suffix>, where <suffix> is the
 * lowercase form of its own controller_id_t (see ipc_attach_name()), in
 * the GLOBAL Qnet namespace (NAME_FLAG_ATTACH_GLOBAL - see ipc_attach()
 * in qnet_utils.c). Every peer that needs to reach it uses the same
 * string via the client queue below. One channel per node serves every
 * message type it receives - ipc_request_t's verb field disambiguates,
 * so there is no separate channel per verb.
 *
 * Total attach points: 10 - one per controller_id_t value except
 * CTRL_UNKNOWN (C1, L1-L6, RL1-RL3).
 *
 * Resolving which physical/virtual Qnet node a given attach point
 * actually lives on (single machine vs. 2-3 VMs, per
 * docs/QNX_DEPLOYMENT_RUN_GUIDE.md) IS a deployment-time decision, not a
 * compile-time constant - qnet_utils.c cannot know it while being
 * compiled. It is handled at runtime instead: the client side
 * (ipc_client_thread_main(), via an internal build_open_path() helper in
 * qnet_utils.c) consults the TRAFFIC_NODE_MAP environment variable, a
 * comma-separated "<suffix>=<qnet-nodename>" list parsed once and cached.
 * A suffix absent from TRAFFIC_NODE_MAP (including the common case of the
 * variable being unset entirely) resolves as "same node as the caller",
 * which is byte-for-byte the lookup this codebase always did - so
 * existing single-machine/same-node deployments need no configuration
 * change at all. A suffix present in the map instead builds
 * "/net/<nodename>/dev/name/global/" TRAFFIC_NAME_PREFIX "/" <suffix> -
 * see app/shared/README.md's "Qnet attach-point names" section and
 * docs/QNX_DEPLOYMENT_RUN_GUIDE.md for the full deployment walkthrough,
 * and qnet_utils.c's build_open_path()/node_map_load() for the
 * implementation and the QNX API details it relies on.
 */
#define TRAFFIC_NAME_PREFIX "traffic"

/* Returns the attach-point suffix for a controller (e.g. "c1", "l3",
 * "rl2"), or NULL for CTRL_UNKNOWN / an out-of-range value. */
const char *ipc_attach_name(controller_id_t id);

/* Wraps name_attach() with TRAFFIC_NAME_PREFIX "/" <suffix>, registered
 * in the GLOBAL Qnet namespace (NAME_FLAG_ATTACH_GLOBAL) so other nodes
 * can reach it via "/net/<this-node>/dev/name/global/...". Still resolves
 * from a plain same-node name_open() with no /net/ prefix, exactly as a
 * local-only attach would - see qnet_utils.c's ipc_attach() for why.
 * Returns the channel id (chid) to pass to ipc_server_run()/
 * ipc_timer_arm(), or -1. */
int ipc_attach(controller_id_t self_id);

/*
 * Threading pattern (see app/shared/README.md "Threading pattern" for the
 * full justification, citing Lecture02 slide 28): every node runs the
 * SAME two dedicated threads for IPC, never more, never fewer -
 *
 *   1. Server thread: calls ipc_attach() once, then ipc_server_run() in a
 *      loop for the rest of the process's life. NEVER calls MsgSend().
 *   2. Client thread: calls ipc_client_thread_main() in a loop for the
 *      rest of the process's life. This is the ONLY code in the process
 *      allowed to call MsgSend() (via ipc_client_post()'s queue).
 *
 * A third thread (main(), the node's own FSM/business logic) owns
 * whatever local state the server/client threads touch, protected by
 * that node's own mutex - qnet_utils does not own or lock application
 * state, only the IPC plumbing.
 */

/* Pulse codes multiplexed on every node's own channel, alongside the
 * kernel's _PULSE_CODE_DISCONNECT. Values start at _PULSE_CODE_MINAVAIL,
 * exactly as Lecture/lap_5/Lab_05_Task3a.c does - codes below that are
 * reserved by QNX. Add new timer purposes at the end; never renumber. */
enum {
    IPC_PULSE_PHASE_TIMER = _PULSE_CODE_MINAVAIL, /* Lx: yellow/all-red/green step, 4 s demand-extension recheck (TL-01, TL-03) */
    IPC_PULSE_HEARTBEAT_TICK,                     /* every node: 1 s heartbeat cadence (PA-07) */
    IPC_PULSE_RAILWAY_WARNING,                    /* RLx: 45 s warning-to-arrival budget (RC-03) */
    IPC_PULSE_RAILWAY_OCCUPANCY                   /* RLx: 20 s per-direction occupancy window (RC-04); re-arm per active window */
};

/* Arms a timer (periodic if period_ms > 0, one-shot if period_ms == 0)
 * that delivers `pulse_code` to `chid` - the same channel returned by
 * ipc_attach() / passed to ipc_server_run(). Mirrors
 * Lecture/lap_5/Lab_05_Task3a.c's SIGEV_PULSE setup exactly. Re-arming an
 * existing timer: call again with the same *out_timer_id already set is
 * NOT supported here - use timer_settime() directly on *out_timer_id for
 * that (see rlx_timer.c/lx_timer.c, which is where multiple overlapping
 * RC-04 occupancy windows or a changing phase duration get re-armed). */
int ipc_timer_arm(int chid, int pulse_code, uint32_t initial_ms, uint32_t period_ms, timer_t *out_timer_id);

/* --- server thread --- */

/* Invoked by ipc_server_run() for every real message; must fill *reply.
 * Runs on the server thread - must not block (no MsgSend(), no waiting
 * on the client queue's reply). Whatever shared state it touches must be
 * protected by the caller's own mutex. */
typedef void (*ipc_request_handler_t)(const ipc_request_t *req, ipc_reply_t *reply, void *ctx);

/* Invoked by ipc_server_run() for every pulse (rcvid == 0), including
 * _PULSE_CODE_DISCONNECT and every IPC_PULSE_* code armed via
 * ipc_timer_arm(). Same non-blocking constraint as on_request. */
typedef void (*ipc_pulse_handler_t)(int code, void *ctx);

/* Runs forever on the calling thread: MsgReceive() -> dispatch -> for a
 * real message, on_request() fills *reply and this function MsgReply()s
 * it; for a pulse, on_pulse() runs and nothing is replied. Handles
 * _IO_CONNECT/_IO_BASE.._IO_MAX itself (see Lab_06_Task1a_server.c) so
 * callers never see those. Returns -1 only on a genuine MsgReceive()
 * error; otherwise loops until the process exits. */
int ipc_server_run(int chid, ipc_request_handler_t on_request, ipc_pulse_handler_t on_pulse, void *ctx);

/* --- client thread / outgoing queue --- */

typedef struct ipc_client_queue ipc_client_queue_t;

/* Invoked by ipc_client_thread_main() after a send completes (or fails).
 * Runs on the CLIENT thread, never on the server thread. reply is NULL
 * when send_ok is 0 (name_open()/MsgSend() failed - peer unreachable). */
typedef void (*ipc_reply_handler_t)(controller_id_t target_id, const ipc_request_t *original_req,
                                     const ipc_reply_t *reply, int send_ok, void *ctx);

ipc_client_queue_t *ipc_client_queue_create(void);

/* Signals the client thread to stop once the queue drains. Join the
 * thread before calling this, then this, then free nothing else - it
 * releases the queue itself. */
void ipc_client_queue_destroy(ipc_client_queue_t *q);

/* Non-blocking: copies req, clears req.hdr to a safe (non-_IO_*) value,
 * and enqueues it for target_id. Returns immediately - 0 on success, -1
 * if the queue is full or being destroyed. Safe to call from the server
 * thread's on_pulse/on_request handlers or from the node's own FSM
 * thread; this is the ONLY sanctioned way to originate an outgoing
 * request anywhere in a node (see app/shared/README.md). */
int ipc_client_post(ipc_client_queue_t *q, controller_id_t target_id, const ipc_request_t *req,
                     ipc_reply_handler_t on_reply, void *ctx);

/* Entry point for the dedicated client thread:
 *   pthread_create(&tid, NULL, ipc_client_thread_main, queue);
 * Drains the queue, performing the actual blocking name_open()/
 * MsgSend()/name_close() for each job - this is the ONLY thread in the
 * process that calls MsgSend(). Before each name_open(), resolves
 * target_id's path via TRAFFIC_NODE_MAP (same-node by default; see the
 * TRAFFIC_NAME_PREFIX doc comment above) - this is the only place that
 * lookup happens, callers of ipc_client_post() never see it. Returns
 * when ipc_client_queue_destroy() has been called and the queue is
 * empty. */
void *ipc_client_thread_main(void *queue);

#endif /* QNET_UTILS_H */
