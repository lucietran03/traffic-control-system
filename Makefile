# =============================================================================
# Traffic Control System - top-level Makefile
# =============================================================================
#
# This is the single, versioned build entry point for the three distributed
# QNX executables described in docs/QNX_PROJECT_FILE_OVERVIEW.md:
#
#   c_main   <- app/central/src/*.c      + app/shared/src/*.c
#   lx_main  <- app/intersection/src/*.c + app/shared/src/*.c
#   rlx_main <- app/railway/src/*.c      + app/shared/src/*.c
#
# Before this file existed, the only documented way to build the project was
# to hand-configure a QNX Momentics IDE project per
# docs/QNX_MOMENTICS_INTEGRATION.md. That workflow still works and is not
# being removed - some teammates may still prefer the IDE - but it is not
# reproducible from a clean checkout or a CI runner. This Makefile is that
# reproducible path; the IDE docs now cross-reference it as an alternative.
#
# -----------------------------------------------------------------------------
# TWO BUILD MODES
# -----------------------------------------------------------------------------
#
#   1. REAL QNX BUILD          -> `make`, `make all`, `make central`,
#                                  `make intersection`, `make railway`
#      Requires: a QNX Software Development Platform (SDP) install with
#      `qcc` (QNX's compiler driver) on PATH - i.e. you have sourced QNX's
#      environment script (e.g. `qnxsdp-env.sh`) or launched a QNX SDP
#      shell/IDE terminal. Produces build/bin/{c_main,lx_main,rlx_main},
#      the real target binaries, linked against QNX's own libc/pthread.
#      If `qcc` is not found, these targets print an error explaining that
#      and exit non-zero rather than silently doing nothing useful.
#
#   2. HOST SYNTAX CHECK       -> `make check-syntax`
#      Requires: only a plain host `gcc`/`clang` (whatever came with your
#      OS / Xcode CLT / build-essential - nothing QNX-specific). Compiles
#      every .c file below with `-fsyntax-only` against small stand-in
#      headers in tools/host_syntax_stubs/ (see that directory's comments)
#      so the team can catch gross C syntax errors *today*, without a QNX
#      machine. This is explicitly NOT a real build: nothing is linked, no
#      QNX kernel/libc is involved, and it proves nothing about QNX API
#      usage being correct. app/shared/src/qnet_utils.c is intentionally
#      EXCLUDED from this target - it is the one file that genuinely calls
#      QNX-only kernel primitives (MsgSend/MsgReceive/name_attach/...) and
#      faithfully stubbing all of that would be large, fragile, and prone
#      to giving false confidence. It can only be meaningfully checked by
#      a real `qcc`.
#
# -----------------------------------------------------------------------------
# WHY ONE TOP-LEVEL MAKEFILE (not one per subsystem)
# -----------------------------------------------------------------------------
# This is a 3-binary, ~25-file student project. A single Makefile with
# per-subsystem variables/rules is easier to read, diff, and maintain than
# four separate Makefiles glued together with recursive `make -C` (which
# has its own well-known pitfalls for a project this small). If the project
# grows substantially, splitting into per-subsystem Makefiles included from
# this one is a reasonable next step - not needed yet.
#
# -----------------------------------------------------------------------------
# QNX TARGET SPEC - PLEASE CONFIRM BEFORE RELYING ON THIS
# -----------------------------------------------------------------------------
# qcc's `-V` flag selects the target CPU/libc variant to build for; the
# right value depends on your actual deployment target. Per
# docs/QNX_MOMENTICS_INTEGRATION.md section 2.2 ("select the x86_64 CPU
# variant, as required by QNX7.1") and docs/QNX_DEPLOYMENT_RUN_GUIDE.md
# (all documented topologies use VirtualBox VMs with CPU Architecture
# x86_64), this project currently targets x86_64 QNX 7.1 VMs, so
# `-Vgcc_ntox86_64` is used below as the best documented guess. This has
# NOT been verified against a real QNX SDP install (none is available on
# this development machine) - run `qcc -V` on your SDP install to list the
# exact variant names it actually has, and override QNX_TARGET_SPEC below
# (or on the command line: `make QNX_TARGET_SPEC=-Vgcc_ntoarmv7le all`) if
# you ever target ARM hardware instead of the x86_64 VMs.
# =============================================================================

# --- QNX toolchain -----------------------------------------------------------
QCC ?= qcc
QNX_TARGET_SPEC ?= -Vgcc_ntox86_64
# See the big comment block above: confirm/override QNX_TARGET_SPEC for your
# actual QNX SDP install and target architecture.

QCC_WARN_FLAGS := -Wall -Wextra

# QNX Neutrino's libc includes pthread support directly, so no separate
# -lpthread is needed when linking with qcc (unlike a generic POSIX/glibc
# host build). If your SDP variant needs it, add `-lpthread` to the link
# recipes below.

# --- Host syntax-check toolchain ---------------------------------------------
HOSTCC ?= $(shell command -v gcc 2>/dev/null || command -v clang 2>/dev/null || echo cc)
HOST_STD := -std=gnu11
STUB_DIR := tools/host_syntax_stubs

# --- Layout --------------------------------------------------------------
BUILD_DIR := build
BIN_DIR   := $(BUILD_DIR)/bin

SHARED_SRC_DIR       := app/shared/src
SHARED_INC_DIR       := app/shared/includes
CENTRAL_SRC_DIR      := app/central/src
CENTRAL_INC_DIR      := app/central/includes
INTERSECTION_SRC_DIR := app/intersection/src
INTERSECTION_INC_DIR := app/intersection/includes
RAILWAY_SRC_DIR      := app/railway/src
RAILWAY_INC_DIR      := app/railway/includes

SHARED_SRCS       := $(wildcard $(SHARED_SRC_DIR)/*.c)
CENTRAL_SRCS      := $(wildcard $(CENTRAL_SRC_DIR)/*.c)
INTERSECTION_SRCS := $(wildcard $(INTERSECTION_SRC_DIR)/*.c)
RAILWAY_SRCS      := $(wildcard $(RAILWAY_SRC_DIR)/*.c)

SHARED_OBJS       := $(patsubst $(SHARED_SRC_DIR)/%.c,$(BUILD_DIR)/shared/%.o,$(SHARED_SRCS))
CENTRAL_OBJS      := $(patsubst $(CENTRAL_SRC_DIR)/%.c,$(BUILD_DIR)/central/%.o,$(CENTRAL_SRCS))
INTERSECTION_OBJS := $(patsubst $(INTERSECTION_SRC_DIR)/%.c,$(BUILD_DIR)/intersection/%.o,$(INTERSECTION_SRCS))
RAILWAY_OBJS      := $(patsubst $(RAILWAY_SRC_DIR)/%.c,$(BUILD_DIR)/railway/%.o,$(RAILWAY_SRCS))

.DEFAULT_GOAL := all
.PHONY: all central intersection railway clean check-syntax help

# Fails loudly (rather than a confusing compiler-not-found error) if qcc is
# missing when a real-build target is invoked. Shared across the three
# link recipes below.
define REQUIRE_QCC
	@command -v $(QCC) >/dev/null 2>&1 || { \
		echo "ERROR: '$(QCC)' not found on PATH."; \
		echo "A real QNX build requires the QNX SDP toolchain (qcc/q++)."; \
		echo "Source your QNX SDP environment script (e.g. qnxsdp-env.sh)"; \
		echo "or run this from a QNX SDP shell/IDE terminal."; \
		echo "For a host-only syntax check without QNX, run: make check-syntax"; \
		exit 1; \
	}
endef

# --- Top-level targets --------------------------------------------------------

all: central intersection railway ## Build all three QNX binaries (requires qcc)

central: $(BIN_DIR)/c_main ## Build only the Central Controller binary
intersection: $(BIN_DIR)/lx_main ## Build only the Intersection Controller binary
railway: $(BIN_DIR)/rlx_main ## Build only the Railway Controller binary

help:
	@echo "Targets:"
	@echo "  make [all]          - build c_main, lx_main, rlx_main (needs qcc)"
	@echo "  make central        - build build/bin/c_main only"
	@echo "  make intersection   - build build/bin/lx_main only"
	@echo "  make railway        - build build/bin/rlx_main only"
	@echo "  make check-syntax   - host gcc/clang -fsyntax-only check, NOT a QNX build"
	@echo "  make clean          - remove build/"
	@echo ""
	@echo "QNX_TARGET_SPEC is currently: $(QNX_TARGET_SPEC) (see top-of-file comment)"

# --- Object directories --------------------------------------------------------

$(BUILD_DIR)/shared $(BUILD_DIR)/central $(BUILD_DIR)/intersection $(BUILD_DIR)/railway $(BIN_DIR):
	mkdir -p $@

# --- Compile rules (real QNX build, via qcc) ------------------------------------

$(BUILD_DIR)/shared/%.o: $(SHARED_SRC_DIR)/%.c | $(BUILD_DIR)/shared
	$(REQUIRE_QCC)
	$(QCC) $(QNX_TARGET_SPEC) $(QCC_WARN_FLAGS) -I$(SHARED_INC_DIR) -c $< -o $@

$(BUILD_DIR)/central/%.o: $(CENTRAL_SRC_DIR)/%.c | $(BUILD_DIR)/central
	$(REQUIRE_QCC)
	$(QCC) $(QNX_TARGET_SPEC) $(QCC_WARN_FLAGS) -I$(SHARED_INC_DIR) -I$(CENTRAL_INC_DIR) -c $< -o $@

$(BUILD_DIR)/intersection/%.o: $(INTERSECTION_SRC_DIR)/%.c | $(BUILD_DIR)/intersection
	$(REQUIRE_QCC)
	$(QCC) $(QNX_TARGET_SPEC) $(QCC_WARN_FLAGS) -I$(SHARED_INC_DIR) -I$(INTERSECTION_INC_DIR) -c $< -o $@

$(BUILD_DIR)/railway/%.o: $(RAILWAY_SRC_DIR)/%.c | $(BUILD_DIR)/railway
	$(REQUIRE_QCC)
	$(QCC) $(QNX_TARGET_SPEC) $(QCC_WARN_FLAGS) -I$(SHARED_INC_DIR) -I$(RAILWAY_INC_DIR) -c $< -o $@

# --- Link rules (real QNX build, via qcc) ---------------------------------------

$(BIN_DIR)/c_main: $(CENTRAL_OBJS) $(SHARED_OBJS) | $(BIN_DIR)
	$(REQUIRE_QCC)
	$(QCC) $(QNX_TARGET_SPEC) -o $@ $^

$(BIN_DIR)/lx_main: $(INTERSECTION_OBJS) $(SHARED_OBJS) | $(BIN_DIR)
	$(REQUIRE_QCC)
	$(QCC) $(QNX_TARGET_SPEC) -o $@ $^

$(BIN_DIR)/rlx_main: $(RAILWAY_OBJS) $(SHARED_OBJS) | $(BIN_DIR)
	$(REQUIRE_QCC)
	$(QCC) $(QNX_TARGET_SPEC) -o $@ $^

# --- Host syntax check (NOT a QNX build - see top-of-file comment) -------------
#
# Compiles every .c file with the host's own gcc/clang using -fsyntax-only
# (parse + type-check, no codegen, no linking) against the stub headers in
# tools/host_syntax_stubs/. This exists purely so the team has *something*
# runnable today without QNX SDP access; it cannot catch QNX-API-specific
# mistakes and does not replace a real `make all` build on the QNX target.

CHECK_SHARED_SRCS := $(filter-out $(SHARED_SRC_DIR)/qnet_utils.c,$(SHARED_SRCS))

check-syntax:
	@echo "=== make check-syntax: host $(HOSTCC) -fsyntax-only pass ==="
	@echo "This is NOT a real QNX build - no qcc, no QNX headers/libs involved."
	@echo "$(SHARED_SRC_DIR)/qnet_utils.c is excluded (see $(STUB_DIR)/sys/neutrino.h)."
	@status=0; \
	for f in $(CHECK_SHARED_SRCS); do \
		echo "[shared]       $$f"; \
		$(HOSTCC) -fsyntax-only $(HOST_STD) -Wall -Wextra -I$(STUB_DIR) -I$(SHARED_INC_DIR) $$f || status=1; \
	done; \
	for f in $(CENTRAL_SRCS); do \
		echo "[central]      $$f"; \
		$(HOSTCC) -fsyntax-only $(HOST_STD) -Wall -Wextra -I$(STUB_DIR) -I$(SHARED_INC_DIR) -I$(CENTRAL_INC_DIR) $$f || status=1; \
	done; \
	for f in $(INTERSECTION_SRCS); do \
		echo "[intersection] $$f"; \
		$(HOSTCC) -fsyntax-only $(HOST_STD) -Wall -Wextra -I$(STUB_DIR) -I$(SHARED_INC_DIR) -I$(INTERSECTION_INC_DIR) $$f || status=1; \
	done; \
	for f in $(RAILWAY_SRCS); do \
		echo "[railway]      $$f"; \
		$(HOSTCC) -fsyntax-only $(HOST_STD) -Wall -Wextra -I$(STUB_DIR) -I$(SHARED_INC_DIR) -I$(RAILWAY_INC_DIR) $$f || status=1; \
	done; \
	if [ $$status -eq 0 ]; then \
		echo "check-syntax: PASS (host syntax check only - not a QNX build)"; \
	else \
		echo "check-syntax: FAIL"; \
	fi; \
	exit $$status

# --- Housekeeping --------------------------------------------------------------

clean: ## Remove all build output
	rm -rf $(BUILD_DIR)
