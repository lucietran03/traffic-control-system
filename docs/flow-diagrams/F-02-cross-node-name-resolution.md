# F-02 — Cross-Node Qnet Name Resolution (TRAFFIC_NODE_MAP): Function-Level Flow

## What this feature does

This is infrastructure, not one of the 10 formally-specified use cases in `usecase.md` — it
underlies every one of them once a deployment spans more than one physical/virtual Qnet
node. `ipc_client_post()` callers (C1's `c_comm.c`, every Lx's/RLx's FSM/comm glue) only ever
name a `controller_id_t` target; they never know or care which Qnet node that target's
process actually runs on. Something still has to turn `CTRL_L3` into either a plain
same-node `name_open("traffic/l3", 0)` (single-machine testing, the historical/default
behavior) or a Qnet cross-node path like
`/net/VM_x86_Target02/dev/name/global/traffic/l3` (the multi-VM topologies in
`docs/QNX_DEPLOYMENT_RUN_GUIDE.md`) — without recompiling anything, since which suffix lives
on which node is a deployment-time decision. `TRAFFIC_NODE_MAP` is that mechanism: an
environment variable read once per process, parsed into an in-memory table, and consulted on
every outgoing client-thread `name_open()`.

## Entry point

There is no explicit "read `TRAFFIC_NODE_MAP` at startup" call anywhere — `main()` in
`c_main.c`/`lx_main.c`/`rlx_main.c` never touches it. The variable is read **lazily, on
demand, the first time it is needed** — not eagerly at process start, and not on every call
to `ipc_client_post()`.

Concretely: `resolve_node()` (`app/shared/src/qnet_utils.c`, around line 151) opens with
`pthread_once(&node_map_once, node_map_load)` (around line 153). The very first invocation of
`resolve_node()` for the whole process — whichever queued job happens to be first drained by
that node's dedicated client thread — is what triggers `node_map_load()`
(`app/shared/src/qnet_utils.c`, around line 102) to call `getenv("TRAFFIC_NODE_MAP")` (around
line 108) for the one and only time in the process's life. `pthread_once_t node_map_once`
(around line 83) guarantees `node_map_load()` never re-runs no matter how many later jobs
call `resolve_node()`; every subsequent call is a cheap re-check of the already-populated
`node_map[]` array (around line 82).

`resolve_node()` itself is only ever reached from one call site: `build_open_path()` (around
line 174), which in turn is only ever called from `ipc_client_thread_main()`'s per-job loop
(around line 428). So the practical trigger is: **the first job any client thread ever
dequeues from its `ipc_client_queue_t`**, which in turn is the first time application code
anywhere in that process calls `ipc_client_post()` and the client thread gets around to
processing it. `ipc_client_post()` (around line 365) itself never touches the map — it only
enqueues and returns immediately (non-blocking), so posting many requests before the client
thread wakes up still only parses the env var once, on the first one actually processed.

## Function call chain

All numbered steps are real function calls in `app/shared/src/qnet_utils.c`, in call order,
for one outgoing job on one node's client thread.

1. `ipc_client_thread_main()` (around line 401) dequeues a job (`job.target_id`,
   `job.req`, ...) from the ring-buffer queue under `q->lock` (around lines 411–422) — this
   part has nothing to do with node resolution, it is the same queue-drain every job goes
   through.
2. It calls `build_open_path(job.target_id, path, sizeof(path))` (around line 428), the single
   function responsible for turning a `controller_id_t` into the exact string that will be
   passed to `name_open()`.
3. `build_open_path()` (around line 165) first calls `build_path(id, local_path,
   sizeof(local_path))` (around line 170) — the same helper `ipc_attach()` uses to build its
   own attach name — which in turn calls `ipc_attach_name(id)` (around line 36) to look up
   `id`'s suffix in the file-local `ATTACH_SUFFIX[]` table (around lines 13–24, e.g.
   `CTRL_L3 -> "l3"`) and `snprintf()`s `"%s/%s"` with `TRAFFIC_NAME_PREFIX` ("traffic") to
   produce `local_path = "traffic/l3"` (around line 40). This is the exact same string
   `ipc_attach()` registered on the attach side — `build_open_path()` never invents its own
   naming convention, it only ever wraps `build_path()`'s output (see the comment at line
   160–164).
4. `build_open_path()` then calls `resolve_node(id)` (around line 174).
5. `resolve_node()` (around line 151) runs `pthread_once(&node_map_once, node_map_load)`
   (around line 153, lazily triggering step 6 the first time only), then checks `id < 0 ||
   id >= CTRL_UNKNOWN || !node_map[id].set` (around line 154). If `id` is out of range or was
   never mentioned in `TRAFFIC_NODE_MAP`, it returns `NULL`. Otherwise it returns
   `node_map[id].node`, the cached Qnet node name string.
6. `node_map_load()` (around line 102, runs at most once per process): calls
   `getenv("TRAFFIC_NODE_MAP")` (around line 108). If unset or empty, it returns immediately
   — `node_map[]` stays all-zero (`.set == 0` for every entry), so every future
   `resolve_node()` call returns `NULL` for every `id`. Otherwise it copies the value into a
   fixed 512-byte stack buffer (`strncpy`, around line 121, silently truncating an absurdly
   long value rather than crashing) and hand-rolls the split with `strchr(entry, ',')` (around
   line 126) to isolate one `"<suffix>=<nodename>"` entry at a time, then `strchr(entry, '=')`
   (around line 133) to split that entry into `<suffix>` and `<nodename>`.
7. For each entry, `node_map_load()` calls `suffix_to_id(entry, eq - entry)` (around line
   135) — a **reverse lookup against the same `ATTACH_SUFFIX[]` table** `build_path()` reads
   (around lines 89–100), so the env var's keys and the wire attach-point suffixes can never
   drift apart. If the suffix matches a known `controller_id_t`, it `strncpy()`s the node name
   into `node_map[id].node` (around line 137, capped at 63 chars +
   NUL, `TRAFFIC_NODE_NAME_MAX`) and sets `node_map[id].set = 1` (around line 139). An unknown
   suffix or an empty node name (`eq[1] == '\0'`) is silently skipped — that one entry only,
   parsing continues for the rest of the comma-separated list (around line 141–143).
8. Back in `build_open_path()` (around line 174–201), the two branches:
   - **`node == NULL`** (suffix absent from the map, or `TRAFFIC_NODE_MAP` unset entirely —
     the default): `snprintf(path, path_size, "%s", local_path)` (around line 188). Result:
     exactly `"traffic/l3"` — no `/net/` prefix at all. Byte-for-byte what
     `ipc_client_thread_main()` always passed to `name_open()` before this feature existed.
   - **`node != NULL`** (suffix present in the map): `snprintf(path, path_size,
     "/net/%s/dev/name/global/%s", node, local_path)` (around line 199). Result: e.g.
     `"/net/VM_x86_Target02/dev/name/global/traffic/l3"`.
9. `ipc_client_thread_main()` receives the finished `path` back from `build_open_path()`
   (return value `0` on success — the only failure mode is `build_path()` returning `-1` for
   an invalid `id`, around line 170) and calls `name_open(path, 0)` (around line 429) — the
   actual blocking Qnet name-resolution call, on this dedicated client thread only.
10. If `name_open()` fails (`coid == -1`) **and** the path contains `"/dev/name/global/"`
    (around line 430), `ipc_client_thread_main()` builds one fallback path,
    `alt_path`, by substring-swapping `global` for `local` (around lines 431–434) and retries
    `name_open(alt_path, 0)` (around line 435). This covers the case where the target node's
    global name server (`gns`) isn't running and `ipc_attach()` fell back to registering under
    `/dev/name/local/...` instead (see `ipc_attach()`, around lines 226–233) — the client side
    mirrors that same attach-side fallback rather than just giving up. This retry is
    independent of `TRAFFIC_NODE_MAP` parsing itself; it only ever triggers on the cross-node
    branch, since the same-node branch's `path` never contains `/dev/name/global/`.
11. On success or failure, `job.on_reply()` is invoked with `send_ok` reflecting whether
    `name_open()` (either attempt) and the subsequent `MsgSend()` (around line 438) succeeded
    — a target whose node is unreachable or misconfigured in `TRAFFIC_NODE_MAP` fails exactly
    the same way an offline same-node peer would: `send_ok == 0`, `reply == NULL`, no crash,
    no exception path distinct from ordinary peer-unreachable handling.

## Cross-node view

`TRAFFIC_NODE_MAP` does not change *what* gets sent (`ipc_request_t`'s `verb`/`payload` are
built identically regardless of topology) or *which thread* sends it (still only
`ipc_client_thread_main()`, per node) — it only changes the **one string** passed to
`name_open()`, computed fresh by `build_open_path()` for every job, never cached per-target
(only the underlying `TRAFFIC_NODE_MAP` table itself is cached, via `pthread_once`). This is
why the fix is additive: `ipc_attach()`'s switch from flags `0` to `NAME_FLAG_ATTACH_GLOBAL`
(around line 230) means every attach point is already reachable from other nodes the moment
it starts, whether or not `TRAFFIC_NODE_MAP` is ever set on the *attaching* process — the
`TRAFFIC_NODE_MAP` variable only needs to be set on the process that is **calling out**
(the client side), and only needs to list the suffixes that process actually targets via
`ipc_client_post()`.

Concretely, for the `"one VM per role"` topology in `app/shared/README.md`'s worked example
(`c1` on one node, `l1..l6` on a second, `rl1..rl3` on a third), a process running `lx_main 3`
(identity `L3`) that needs to reach Central would have `TRAFFIC_NODE_MAP` include
`c1=<c1's-node-name>`; when its client thread processes a job with `target_id == CTRL_C1`,
`resolve_node(CTRL_C1)` returns that node name and `build_open_path()` produces
`/net/<c1's-node-name>/dev/name/global/traffic/c1` instead of plain `traffic/c1`. Every other
Lx/RLx process making the identical `ipc_client_post(q, CTRL_C1, ...)` call independently goes
through the exact same trace on its own client thread with its own process's copy of
`TRAFFIC_NODE_MAP` — there is no shared/broadcast state between processes, each one parses
its own environment exactly once.

The single-machine/no-`TRAFFIC_NODE_MAP` case is not a degraded fallback path bolted on for
safety — it is what `node_map_load()` naturally produces when `getenv()` returns `NULL`
(all-zero table, `resolve_node()` always `NULL`, `build_open_path()` always takes the
same-node branch), so the pre-existing single-node test setup requires zero configuration
change and is exercised by the exact same code path as the cross-node case, just with an
empty table.

## System-level summary diagram

```mermaid
---
title: F-02 — Cross-Node Qnet Name Resolution (TRAFFIC_NODE_MAP)
---
flowchart TD
    A["ipc_client_post(q, target_id, req, on_reply, ctx)\n(any thread; enqueues, returns immediately)"] --> B["ipc_client_thread_main() dequeues job\n(dedicated client thread, this node only)"]
    B --> C["build_open_path(target_id, path, sizeof(path))"]
    C --> D["build_path(target_id, local_path, ...)\n-> ipc_attach_name(target_id) -> \"traffic/&lt;suffix&gt;\""]
    D --> E["resolve_node(target_id)\npthread_once(&node_map_once, node_map_load) -- first call only"]
    E -->|"first call only"| F["node_map_load(): getenv(\"TRAFFIC_NODE_MAP\")\nparse \"suffix=node\" pairs via suffix_to_id()\ncache into node_map[] (process-lifetime)"]
    F --> G{"node_map[target_id].set ?"}
    E --> G
    G -->|"no: suffix absent /\nTRAFFIC_NODE_MAP unset"| H["path = local_path\ne.g. name_open(\"traffic/l3\", 0)\n(same-node, pre-existing behavior)"]
    G -->|"yes: suffix present"| I["path = \"/net/&lt;node&gt;/dev/name/global/\" + local_path\ne.g. name_open(\"/net/VM_x86_Target02/dev/name/global/traffic/l3\", 0)"]
    H --> J["name_open(path, 0) -> coid"]
    I --> J
    J -->|"coid == -1 and path has /dev/name/global/"| K["retry: swap global -> local in path\nname_open(alt_path, 0)"]
    J -->|"coid != -1"| L["MsgSend(coid, &req, ..., &reply, ...)\nname_close(coid)"]
    K --> L
    L --> M["job.on_reply(target_id, req, reply, send_ok, ctx)"]
```

The left/right split at `resolve_node()`'s decision point is the entire feature: everything
above and below it (queueing, `build_path()`'s suffix lookup, `name_open()`/`MsgSend()`/
`name_close()`, the global→local retry) is identical whether the target is on the same node
or a different one — `TRAFFIC_NODE_MAP` only decides which literal string `name_open()`
receives.
