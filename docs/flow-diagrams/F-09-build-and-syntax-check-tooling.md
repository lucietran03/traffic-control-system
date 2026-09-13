# F-09 — Build & Host-Syntax-Check Tooling: Function-Level Flow

## What this feature does

This is infrastructure, not one of the 10 formally-specified use cases in `usecase.md` — it
exists because the development machine this project has been built on has never had a QNX
Software Development Platform (SDP) install, so `qcc` (QNX's compiler driver) has never been
on `PATH` here (root `Makefile`, around lines 25-31, 68-69). Without `qcc`, nobody working on
this machine can build or even parse-check `app/central`, `app/intersection`, or
`app/railway` against a real QNX toolchain before pushing. `make check-syntax` is the
stand-in: it runs the plain host `gcc`/`clang` with `-fsyntax-only` (parse + type-check, no
codegen, no linking) against a small set of hand-written stand-in headers in
`tools/host_syntax_stubs/`, so the team has *something* automatable today that catches gross
C syntax errors, without pretending it is a real QNX build or proving anything about QNX API
usage being correct (Makefile, around lines 33-46, 189-196).

## Entry point

Two independent `make` targets, invoked from the repo root, that never call each other:

- **`make` / `make all` / `make central` / `make intersection` / `make railway`** — the real
  build. Defined starting at the top-level targets block, around line 135.
- **`make check-syntax`** — the host syntax check this document is about. Defined around
  line 199.

There is no automatic trigger for either — a developer or a CI job runs one of these two
`make` targets explicitly. Nothing in the source tree invokes the other target as a
dependency; they are two deliberately separate code paths sharing only the same `SHARED_SRCS`
/ `CENTRAL_SRCS` / `INTERSECTION_SRCS` / `RAILWAY_SRCS` file lists (Makefile, around lines
106-109).

## "Call chain" (build-system steps, not C function calls)

These are `make`/shell steps, traced in the order they actually execute, not C function
calls — there is no runtime process to trace here.

### Path A — `make` (real QNX build)

1. `make all` depends on `central`, `intersection`, `railway` (around line 135), each of which
   depends on one binary target (`$(BIN_DIR)/c_main` etc., around lines 137-139).
2. Every compile rule (e.g. `$(BUILD_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.c`, around line 159)
   and every link rule (e.g. `$(BIN_DIR)/c_main: $(CENTRAL_OBJS) $(SHARED_OBJS)`, around line
   177) first expands `$(REQUIRE_QCC)` (the `define REQUIRE_QCC` block, around lines 122-131)
   before doing anything else.
3. `REQUIRE_QCC` runs `command -v $(QCC)` (`QCC ?= qcc`, around line 76). If `qcc` is not on
   `PATH`, it prints an explicit error ("A real QNX build requires the QNX SDP toolchain
   (qcc/q++)... For a host-only syntax check without QNX, run: make check-syntax") and
   `exit 1`s — around lines 123-130 — rather than silently failing or falling back to the host
   compiler. This is the guard that keeps Path A and Path B from ever being silently conflated:
   a real build never quietly downgrades into a syntax-only pass.
4. If `qcc` *is* present, the compile rule runs
   `$(QCC) $(QNX_TARGET_SPEC) $(QCC_WARN_FLAGS) -I... -c $< -o $@` (around lines 161, 165, 169,
   173) — `-Vgcc_ntox86_64` by default (around line 77) — compiling against QNX's own real
   `<sys/neutrino.h>`, `<sys/dispatch.h>`, `<sys/netmgr.h>`, libc, and pthread headers, then the
   link rule runs `$(QCC) $(QNX_TARGET_SPEC) -o $@ $^` (around lines 179, 183, 187) to produce
   `build/bin/c_main` / `lx_main` / `rlx_main`. On this development machine, `qcc` has never
   actually been present, so step 3's guard is the only branch of Path A that has ever executed
   here — the rest of Path A is unverified against a real SDP (Makefile's own top-of-file
   comment, around lines 59-72, flags `QNX_TARGET_SPEC` as an unconfirmed best guess for
   exactly this reason).

### Path B — `make check-syntax` (host stand-in)

1. `HOSTCC` resolves to whatever the dev machine actually has: `$(shell command -v gcc ...||
   command -v clang ... || echo cc)` (around line 89) — no QNX toolchain involved anywhere in
   this path.
2. `CHECK_SHARED_SRCS := $(filter-out $(SHARED_SRC_DIR)/qnet_utils.c,$(SHARED_SRCS))` (around
   line 197) computes the shared-source list for this target as *every* file in
   `app/shared/src/*.c` **except** `qnet_utils.c`. The three per-subsystem lists
   (`CENTRAL_SRCS`, `INTERSECTION_SRCS`, `RAILWAY_SRCS`) are used unfiltered.
3. The `check-syntax:` recipe (around lines 199-225) loops over `$(CHECK_SHARED_SRCS)`, then
   `$(CENTRAL_SRCS)`, then `$(INTERSECTION_SRCS)`, then `$(RAILWAY_SRCS)`, and for each file
   runs `$(HOSTCC) -fsyntax-only $(HOST_STD) -Wall -Wextra -I$(STUB_DIR) -I<subsystem includes>
   $$f` (around lines 206, 210, 214, 218) — `HOST_STD := -std=gnu11` (around line 90),
   `STUB_DIR := tools/host_syntax_stubs` (around line 91). `-I$(STUB_DIR)` is what makes
   `#include <sys/neutrino.h>` resolve to the fake header instead of failing outright (there is
   no real one on this machine to find).
4. Any non-zero exit sets `status=1` but the loop keeps going so a single bad file doesn't hide
   failures in the rest; the final `exit $$status` (around line 225) is what makes CI/manual
   invocation actually fail on a real error.

**Why `qnet_utils.c` is the one file excluded (step 2 above):** every other `.c` file that
transitively includes `<sys/neutrino.h>` (via `app/shared/includes/qnet_utils.h`) — `c_main.c`,
`lx_main.c`, `lx_comm.c`, `rlx_main.c`, `rlx_comm.c` — only needs two harmless symbols from
that header to parse (see stub file below). `app/shared/src/qnet_utils.c` itself is different:
it is the one file in the whole tree that actually *calls* real QNX-only kernel primitives —
`name_attach()`, `MsgReceive()`/`MsgReply()`/`MsgSend()`, `ConnectAttach()`/`ConnectDetach()`,
`timer_create()` against QNX's pulse/sigevent extension fields — and it does so using QNX-only
manifest constants like `ND_LOCAL_NODE` (used in the `ConnectAttach(ND_LOCAL_NODE, 0, chid,
_NTO_SIDE_CHANNEL, 0)` call inside `ipc_timer_arm()`, `app/shared/src/qnet_utils.c` around line
247) that come from real QNX headers (`<sys/netmgr.h>` for `ND_LOCAL_NODE`) with no portable
host equivalent. Faithfully stubbing all of that — a real message-passing kernel ABI — would
require a large, fragile fake surface that risks either false failures (an imperfect stub
rejecting valid QNX code) or false confidence (a stub that silently accepts a real bug because
it never modeled the constraint the real kernel enforces). The Makefile comment block (around
lines 41-46) and the stub header's own comment (`tools/host_syntax_stubs/sys/neutrino.h`,
around lines 24-32) both say the same thing: that file "can only be meaningfully checked by a
real `qcc`."

**A real, known blind spot this exclusion creates:** this project's own git history is
evidence, not hypothetical. `app/shared/src/qnet_utils.c` (around line 10) has
`#include <sys/netmgr.h>` sitting right above `#include <sys/neutrino.h>`'s neighbors —
added in commit `6fd82b7` specifically because `ND_LOCAL_NODE` (used in the `ConnectAttach()`
call at line 247) is declared by `<sys/netmgr.h>`, not `<sys/neutrino.h>`, on real QNX. Because
`qnet_utils.c` is the one file `make check-syntax` never compiles, a missing
`#include <sys/netmgr.h>` in that file is structurally invisible to Path B — `check-syntax`
would report `PASS` regardless, since it never touches this file at all. The bug was only
something a real `qcc` pass (or manual source review, which is what actually happened here)
could have caught. This is the concrete demonstration of the Makefile's own warning: "it
cannot catch QNX-API-specific mistakes" (around line 195) is not a theoretical caveat, it is
this exact incident.

## Cross-node view

Not applicable — F-09 is a build-time-only, single-machine mechanism. Neither `make` nor
`make check-syntax` starts a process, opens a channel, or sends a single byte over Qnet or
any other IPC. There are no nodes, no `MsgSend`/`MsgReceive` round trips, and no distributed
state to reason about here; forcing this section to describe a "cross-node" story would
misrepresent the feature. The entire effect of both targets is local: files in, either an
object/binary in `build/` or a pass/fail exit code on stdout/exit status.

## System-level summary diagram

```text
                              app/{shared,central,intersection,railway}/src/*.c
                                                 |
                    +----------------------------+----------------------------+
                    |                                                         |
              PATH A: `make` / `make all`                          PATH B: `make check-syntax`
              REQUIRE_QCC guard (Makefile ~L122)                   HOSTCC := gcc|clang|cc (~L89)
                    |                                                         |
        qcc on PATH?  -- no --> ERROR, exit 1                     CHECK_SHARED_SRCS = SHARED_SRCS
              |  (~L123-130,                                       minus qnet_utils.c (~L197)
             yes    "run make check-syntax")                                  |
              |                                                    -fsyntax-only, -I tools/host_syntax_stubs
     $(QCC) $(QNX_TARGET_SPEC) ...                                 (fakes <sys/neutrino.h>: only
     against REAL QNX headers/libc                                 _PULSE_CODE_MINAVAIL + timer_t,
     (-Vgcc_ntox86_64 default, ~L77)                                neutrino.h ~L38-56)
              |                                                                |
     compile (~L159-173) + link (~L177-187)                        parse + type-check every included
              |                                                     file EXCEPT qnet_utils.c
              v                                                                v
     build/bin/{c_main,lx_main,rlx_main}                           "check-syntax: PASS/FAIL" (~L220-225)
     (real QNX binaries - never produced                           (a verdict, not a binary -
      on this dev machine: no qcc found)                            nothing is linked, nothing runs)

     app/shared/src/qnet_utils.c is compiled ONLY on the left path.
     It is the one file that genuinely calls QNX kernel primitives
     (name_attach, MsgSend/MsgReceive/MsgReply, ConnectAttach with
     ND_LOCAL_NODE from <sys/netmgr.h>, ...) that the stub headers on
     the right deliberately do not attempt to fake. A missing/wrong
     include in that file (as really happened once - see above) is
     invisible to the right-hand path by construction.
```
