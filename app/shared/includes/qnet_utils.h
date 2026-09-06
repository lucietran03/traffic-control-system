#ifndef QNET_UTILS_H
#define QNET_UTILS_H

#include "sys_types.h"

/*
 * Qnet attach-point naming convention.
 *
 * Every node calls name_attach() exactly once at startup, registering
 * itself as TRAFFIC_NAME_PREFIX "/" <suffix>, where <suffix> is the
 * lowercase form of its own controller_id_t (see ipc_attach_name()).
 * Every peer that needs to reach it calls name_open() on that same
 * string. One channel per node serves every message type it receives -
 * ipc_msg.h's msg_type_t field disambiguates, so there is no separate
 * channel per verb.
 *
 * Total attach points: 10 - one per controller_id_t value except
 * CTRL_UNKNOWN (C1, L1-L6, RL1-RL3).
 *
 * This only defines the LOCAL name (e.g. "traffic/c1"). Resolving which
 * physical Qnet node that name lives on (single machine vs. 2-3 VMs, per
 * docs/QNX_DEPLOYMENT_RUN_GUIDE.md) is a deployment-time decision, not a
 * compile-time constant, and is out of scope for this header.
 */
#define TRAFFIC_NAME_PREFIX "traffic"

/* Returns the attach-point suffix for a controller (e.g. "c1", "l3",
 * "rl2"), or NULL for CTRL_UNKNOWN / an out-of-range value. Callers
 * prefix it with TRAFFIC_NAME_PREFIX "/" before passing it to
 * name_attach()/name_open(). */
const char *ipc_attach_name(controller_id_t id);

#endif /* QNET_UTILS_H */
