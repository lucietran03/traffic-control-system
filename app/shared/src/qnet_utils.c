#include "qnet_utils.h"

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
