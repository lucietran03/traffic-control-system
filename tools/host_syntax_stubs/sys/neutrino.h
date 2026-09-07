/*
 * tools/host_syntax_stubs/sys/neutrino.h
 *
 * FAKE, HOST-ONLY STUB. This is NOT QNX's real <sys/neutrino.h>.
 *
 * Purpose: `make check-syntax` compiles this project's sources with the
 * HOST's gcc/clang (no QNX SDP available) purely to catch gross C syntax
 * errors before anyone has access to a real QNX toolchain. app/shared/
 * includes/qnet_utils.h unconditionally does `#include <sys/neutrino.h>`,
 * and that header (not just qnet_utils.c) is transitively pulled in by
 * app/central/src/c_main.c, app/intersection/src/{lx_main,lx_comm}.c and
 * app/railway/src/{rlx_main,rlx_comm}.c. None of those files actually call
 * a QNX kernel primitive themselves (verified by grep - they only mention
 * MsgSend/MsgReceive/etc. in comments), so the only two things they need
 * from this header to parse are:
 *
 *   1. The `_PULSE_CODE_MINAVAIL` constant, used once in qnet_utils.h to
 *      seed an enum of pulse codes.
 *   2. The `timer_t` type, used once in a function prototype
 *      (ipc_timer_arm's out-param). macOS/Darwin's libc does not define
 *      POSIX timer_t/timer_create at all, so without this the syntax
 *      check fails on Darwin even though it might pass on Linux.
 *
 * app/shared/src/qnet_utils.c itself is EXCLUDED from `make check-syntax`
 * (see the Makefile) because it genuinely uses QNX-only kernel APIs
 * (name_attach, MsgReceive/MsgReply/MsgSend, ConnectAttach, the QNX
 * extension fields on struct sigevent, etc.) that would require a much
 * larger and more fragile stub surface to fake convincingly - faking
 * those risks the check-syntax pass being *wrong* (either false failures
 * from an imperfect stub, or false confidence from a stub that silently
 * papers over a real bug). That file can only be meaningfully checked by
 * a real `qcc` once the team has a QNX SDP install.
 *
 * If you add code elsewhere that calls a real QNX primitive, extend this
 * stub deliberately (and re-read the warning above) rather than assuming
 * `make check-syntax` will catch QNX API misuse - it never will.
 */
#ifndef TCS_HOST_SYNTAX_STUB_SYS_NEUTRINO_H
#define TCS_HOST_SYNTAX_STUB_SYS_NEUTRINO_H

/* Real QNX value is 3 (_PULSE_CODE_MINAVAIL sits above the kernel-reserved
 * pulse codes). The exact number is irrelevant for a syntax-only check -
 * it just needs to be a valid integer constant expression. */
#ifndef _PULSE_CODE_MINAVAIL
#define _PULSE_CODE_MINAVAIL 3
#endif

/* Darwin's <time.h> does not define timer_t (no POSIX per-process timers).
 * Glibc's <time.h> already defines it (guarded by __timer_t_defined), so
 * this is a no-op there. If your host libc uses a different guard macro,
 * add it here rather than removing the guard entirely. */
#ifndef __timer_t_defined
#define __timer_t_defined
typedef void *timer_t;
#endif

#endif /* TCS_HOST_SYNTAX_STUB_SYS_NEUTRINO_H */
