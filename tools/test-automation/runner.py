#!/usr/bin/env python3
"""
Scripted-keystroke test automation runner for the Traffic Control System.

Replaces manual keyboard input (and manual stopwatch timing for
docs/test-plan/04-timing-assumptions.md) with precisely-timed scripted
input + automated log-pattern checks. Does NOT need any change to the
QNX C code - it only launches the already-built binaries as ordinary
subprocesses and writes to their stdin exactly what a person would have
typed, at exact millisecond offsets (far more precise than a human for
docs/test-plan/06-concurrency-race.md's sub-second race windows).

Scope: covers environment (A) single-node and (B) multi-node-same-machine
test cases (same-node Qnet name resolution needs no TRAFFIC_NODE_MAP, so
running several binaries as local subprocesses on one machine is exactly
"environment B"). Environment (C) cross-VM and (D) test_client-only cases
are out of scope for this tool - see each case file's "skip"/"skip_reason"
field - and are reported as SKIPPED, never silently passed.

Usage:
    python3 runner.py --binary-dir ../../build/bin --cases cases/01-usecase-functional.json
    python3 runner.py --binary-dir ../../build/bin --cases cases/           # run every *.json in a directory
    python3 runner.py --binary-dir ../../build/bin --cases cases/ --filter TC-UC01

Requires only Python 3 stdlib. Must be run on a machine with the actual
built QNX binaries (c_main/lx_main/rlx_main) reachable - it cannot run
against nothing, and it does not build them (see the repo-root Makefile).
"""
import argparse
import json
import os
import re
import subprocess
import sys
import threading
import time
from pathlib import Path

RESULTS_DIR = Path(__file__).parent / "results"


class NodeProcess:
    """Wraps one running node subprocess: feeds it scripted stdin, keeps a
    thread-safe rolling transcript of everything it has printed."""

    def __init__(self, node_id, binary_path, args):
        self.node_id = node_id
        self.lines = []
        self.lock = threading.Lock()
        self.proc = subprocess.Popen(
            [str(binary_path)] + list(args),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self):
        try:
            for line in self.proc.stdout:
                with self.lock:
                    self.lines.append((time.monotonic(), line.rstrip("\n")))
        except Exception:
            pass

    def send_key(self, ch):
        """Writes exactly one character to this node's stdin and flushes -
        equivalent to a single keypress. No newline needed: every node's
        sensor/operator reader loop uses scanf(" %c", ...), which reads one
        character at a time regardless of line buffering; since stdin here
        is a pipe (not a real tty), there is no canonical-mode line
        buffering to fight - bytes are visible to the child's read() the
        moment we write+flush them."""
        try:
            self.proc.stdin.write(ch)
            self.proc.stdin.flush()
        except (BrokenPipeError, OSError):
            pass

    def transcript_text(self, since_monotonic=0.0):
        with self.lock:
            return "\n".join(line for ts, line in self.lines if ts >= since_monotonic)

    def wait_for_pattern(self, pattern, within_ms, since_monotonic=0.0, after_ms=0):
        """`after_ms` (default 0) sets a LOWER bound, relative to
        since_monotonic (the case's t0) - lines before it are ignored even
        if they match. This is what lets a case distinguish "the first
        ARTERIAL GREEN" from "the second one, one cycle later" (both print
        an identical line) instead of always matching whichever occurrence
        comes first chronologically. `within_ms` remains the UPPER bound,
        also measured from since_monotonic (not from whenever this
        function happens to start polling), so multiple checks in the same
        case don't drift relative to each other or to t0."""
        earliest_ts = since_monotonic + after_ms / 1000.0
        deadline_monotonic = since_monotonic + (after_ms + within_ms) / 1000.0
        regex = re.compile(pattern)
        while True:
            with self.lock:
                for ts, line in self.lines:
                    if ts >= earliest_ts and regex.search(line):
                        return True, line
            if time.monotonic() >= deadline_monotonic:
                return False, None
            time.sleep(0.02)

    def terminate(self):
        try:
            self.proc.stdin.close()
        except Exception:
            pass
        self.proc.terminate()
        try:
            self.proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(timeout=2)

    def dump_log(self, path):
        with self.lock:
            text = "\n".join(line for _, line in self.lines)
        path.write_text(text)


def run_case(case, binary_dir, result_dir_name=None):
    """Runs one test-case dict once. Returns (status, detail) where status
    is 'PASS' / 'FAIL' / 'SKIP' / 'ERROR'.

    result_dir_name optionally overrides the per-node log directory name
    (results/<result_dir_name>/<node>.log) - used by run_case_repeated()
    below so each repeat of a "repeat": N case gets its own subdirectory
    instead of every run overwriting the previous one's logs. Defaults to
    case["id"], identical to the pre-existing behavior for callers that
    don't pass it (i.e. every case with no "repeat" field)."""
    if case.get("skip"):
        return "SKIP", case.get("skip_reason", "marked skip in case file")

    nodes = {}
    case_result_dir = RESULTS_DIR / (result_dir_name or case["id"])
    case_result_dir.mkdir(parents=True, exist_ok=True)

    try:
        for n in case["nodes"]:
            binary_path = Path(binary_dir) / n["binary"]
            if not binary_path.exists():
                return "ERROR", f"binary not found: {binary_path} (build it first - see repo-root Makefile)"
            nodes[n["node_id"]] = NodeProcess(n["node_id"], binary_path, n.get("args", []))

        time.sleep(case.get("setup_wait_ms", 500) / 1000.0)
        t0 = time.monotonic()

        steps = sorted(case.get("steps", []), key=lambda s: s["after_ms"])
        elapsed_ms = 0
        for step in steps:
            wait_ms = step["after_ms"] - elapsed_ms
            if wait_ms > 0:
                time.sleep(wait_ms / 1000.0)
            elapsed_ms = step["after_ms"]
            target = nodes.get(step["node_id"])
            if target is None:
                return "ERROR", f"step references unknown node_id '{step['node_id']}'"
            target.send_key(step["send"])

        failures = []
        for check in case.get("checks", []):
            target = nodes.get(check["node_id"])
            if target is None:
                failures.append(f"check references unknown node_id '{check['node_id']}'")
                continue
            ok, matched_line = target.wait_for_pattern(
                check["pattern"], check["within_ms"], since_monotonic=t0, after_ms=check.get("after_ms", 0)
            )
            if not ok:
                failures.append(
                    f"[{check['node_id']}] pattern not seen within {check['within_ms']}ms: "
                    f"/{check['pattern']}/ ({check.get('description', '')})"
                )

        time.sleep(case.get("cleanup_wait_ms", 300) / 1000.0)

        # Defensive re-create: some environments (e.g. a sandboxed/ephemeral
        # filesystem overlay) can reclaim an empty directory created minutes
        # earlier by the time a long-running case (setup_wait_ms/steps can
        # add up to tens of seconds, see docs/test-plan/06-concurrency-race.md's
        # cases) finally reaches this point - re-asserting it right before
        # writing is a cheap no-op on a normal filesystem (exist_ok=True)
        # and avoids a spurious ERROR on one that isn't.
        case_result_dir.mkdir(parents=True, exist_ok=True)
        for node_id, n in nodes.items():
            n.dump_log(case_result_dir / f"{node_id}.log")

        if failures:
            return "FAIL", "; ".join(failures)
        return "PASS", ""
    except Exception as e:
        return "ERROR", f"{type(e).__name__}: {e}"
    finally:
        for n in nodes.values():
            n.terminate()


def run_case_repeated(case, binary_dir):
    """Honors an optional "repeat": N field (see README.md's schema
    section) by running run_case() N times, fresh node subprocesses each
    time. Returns the same (status, detail) shape as run_case() - PASS
    only if every run PASSed; the first non-PASS run's (status, detail)
    is returned immediately (SKIP is never repeated - if the case is
    marked skip, run_case() would report SKIP on every single repeat
    anyway, so stopping after one is just avoiding useless repeated work,
    not a behavior change). A case with no "repeat" field (or repeat<=1)
    runs exactly once, via the exact same call run_case() always made -
    zero behavior change for every pre-existing case file."""
    repeat = case.get("repeat", 1)
    if not isinstance(repeat, int) or repeat <= 1:
        return run_case(case, binary_dir)

    for i in range(1, repeat + 1):
        result_dir_name = f"{case['id']}/run-{i}"
        status, detail = run_case(case, binary_dir, result_dir_name=result_dir_name)
        if status == "SKIP":
            return status, detail
        if status != "PASS":
            return status, f"run {i}/{repeat} failed: {detail}"
    return "PASS", f"{repeat}/{repeat} repeats passed"


def load_cases(cases_path):
    path = Path(cases_path)
    files = sorted(path.glob("*.json")) if path.is_dir() else [path]
    all_cases = []
    for f in files:
        data = json.loads(f.read_text())
        for c in data.get("test_cases", []):
            c["_source_file"] = f.name
            all_cases.append(c)
    return all_cases


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--binary-dir", required=True, help="Directory containing c_main/lx_main/rlx_main")
    ap.add_argument("--cases", required=True, help="A case JSON file, or a directory of them")
    ap.add_argument("--filter", default=None, help="Only run test case IDs matching this regex")
    args = ap.parse_args()

    RESULTS_DIR.mkdir(exist_ok=True)
    cases = load_cases(args.cases)
    if args.filter:
        rx = re.compile(args.filter)
        cases = [c for c in cases if rx.search(c["id"])]

    if not cases:
        print("No test cases matched.")
        return 1

    counts = {"PASS": 0, "FAIL": 0, "SKIP": 0, "ERROR": 0}
    for case in cases:
        status, detail = run_case_repeated(case, args.binary_dir)
        counts[status] += 1
        marker = {"PASS": "PASS", "FAIL": "FAIL", "SKIP": "SKIP", "ERROR": "ERR "}[status]
        print(f"[{marker}] {case['id']} - {case.get('title', '')}" + (f"  ({detail})" if detail else ""))

    total = sum(counts.values())
    print("\n" + "-" * 60)
    print(f"Total: {total}  Pass: {counts['PASS']}  Fail: {counts['FAIL']}  "
          f"Skip: {counts['SKIP']}  Error: {counts['ERROR']}")
    print(f"Per-node logs for every run case: {RESULTS_DIR}/<test-id>/<node>.log")
    return 0 if counts["FAIL"] == 0 and counts["ERROR"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
