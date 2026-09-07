# Test automation (scripted keystrokes)

Automates `docs/test-plan/`'s manual test cases: instead of a person
pressing keys and eyeballing the console, `runner.py` launches the real
QNX-built binaries as subprocesses, sends the exact same keystrokes at
exact millisecond offsets, and checks the resulting output against
expected patterns automatically. No QNX C code changes - it drives the
already-built binaries exactly the way a person at a keyboard would.

**Scope**: only environment **(A)** single-node and **(B)**
multi-node-same-machine cases (see `docs/test-plan/README.md`'s
environment legend) can be automated this way — launching several
binaries as local subprocesses on one machine already satisfies "same
node" Qnet resolution, no `TRAFFIC_NODE_MAP` needed. Environment **(C)**
(genuinely separate VMs/machines) and **(D)** (needs the not-built
`test_client` tool) cases, plus anything requiring a debugger or human
visual/stopwatch judgment a regex can't replace, are marked
`"skip": true` with a reason in their case file — the runner reports
these as `SKIP`, never fakes a pass.

## Running it

Requires the real binaries already built (see repo-root `Makefile` or
QNX Momentics — `make check-syntax` alone is NOT enough, you need a real
QNX build since this runs the actual programs):

```bash
python3 tools/test-automation/runner.py \
    --binary-dir build/bin \
    --cases tools/test-automation/cases/
```

Run one category, or filter to specific IDs:

```bash
python3 runner.py --binary-dir ../../build/bin --cases cases/06-concurrency-race.json
python3 runner.py --binary-dir ../../build/bin --cases cases/ --filter TC-UC08
```

Per-node captured output for every case that actually ran (pass or fail)
is saved to `results/<test-id>/<node-id>.log` for debugging a failure.

## Case file schema

One JSON file per `docs/test-plan/NN-*.md` category, same base filename,
containing a `test_cases` array. Each entry:

```jsonc
{
  "id": "TC-UC01-1",            // must match the ID in the .md file
  "title": "short title",
  "type": "positive",           // positive | negative | edge
  "environment": "A",           // A or B only - see scope note above
  "skip": false,                // true if this case can't be automated
  "skip_reason": null,          // required string if skip is true
  "nodes": [                    // every binary this case needs running
    {"node_id": "L1", "binary": "lx_main", "args": ["1"]}
  ],
  "setup_wait_ms": 500,         // let nodes attach before sending input
  "steps": [                    // scripted input, executed in after_ms order
    {"after_ms": 0,    "node_id": "L1", "send": "a"},
    {"after_ms": 9000, "node_id": "L1", "send": "w"}
  ],
  "checks": [                   // every pattern must appear or the case FAILs
    {"node_id": "L1", "pattern": "ARTERIAL.*YELLOW", "within_ms": 9000,
     "description": "human-readable reason this check exists"}
  ],
  "cleanup_wait_ms": 300
}
```

**`checks[].after_ms` (optional, defaults to 0)**: a lower bound, measured
from the same case-start reference point as `within_ms`'s upper bound
(both are anchored to when the case's `steps` began, not to whenever the
check itself happens to start polling). Lines printed before `after_ms`
are ignored even if they match the pattern. This matters whenever a
node's log can legitimately print the exact same line more than once in
one run (e.g. `lx_signal.c`'s "PED SIGNAL side 0 -> WALK" once per
pedestrian-recall cycle, or `rlx_gate.c`'s "commanding gates DOWN" both
on the original close and again on a reclose) - without `after_ms`, a
check would always match whichever occurrence comes first
chronologically, which is often the WRONG one for a race case that's
specifically trying to prove something about the *second* occurrence.
Omit it (or leave it 0) for the common case where the pattern is only
ever expected once, or where the first occurrence is exactly the one
being tested for.

**`repeat` (optional, not in the original schema above)**: an integer N on
a case object, e.g. `"repeat": 10`. Meaning: run this exact case N times
back-to-back (fresh node subprocesses each time, same as any other run of
the case); report the case as a single PASS only if every one of the N
runs independently passed - any single FAIL/ERROR makes the whole case
FAIL, any SKIP short-circuits to a single SKIP without repeating. This is
for `docs/test-plan/06-concurrency-race.md`'s race-condition cases, which
explicitly recommend repeating a case N>=5-10 times to raise the odds of
actually hitting a narrow timing window in at least one run - exactly the
kind of thing a human re-running a manual test by hand won't do
consistently, but a script can. A case with no `repeat` field runs exactly
once, unchanged from before.

**`send` values**:
- `lx_sensor.c`/`rlx_sensor.c` keys are read one character at a time
  (`scanf(" %c", &input)`) — send exactly the single character, e.g.
  `"a"`, no trailing newline needed.
- `c_operator.c` numeric prompts are read with `scanf("%ld", ...)` —
  **always include a trailing `\n`**, e.g. `"3\n"`, otherwise `scanf`
  blocks waiting for more digits since a bare `"3"` alone never tells it
  the number is complete.

**`pattern` values** must be regexes matched against the *actual* printf
output of `lx_signal.c`/`rlx_signal.c`/`rlx_gate.c`/`c_comm.c`/`c_hmi.c`/
`c_logger.c` — grep the real source for the literal string before writing
a pattern, don't paraphrase from the `.md` test-plan prose.

## Known gap

This tool has no equivalent of `docs/test-plan/04-timing-assumptions.md`'s
human-stopwatch instructions built in beyond what `checks[].within_ms`
already gives you (which itself is a real automated timing assertion,
generally more precise than a human with a phone stopwatch) — for cases
that need to measure the gap *between two* log lines precisely (e.g. "L3
must start exactly ~21s after L1"), the case file should express this as
two chained checks against two different nodes with appropriate
`within_ms` windows, not as a single check.
