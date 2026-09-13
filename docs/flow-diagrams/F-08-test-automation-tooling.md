# F-08 — Scripted Test-Automation Tooling: Function-Level Flow

**Trigger:** `runner.py`'s `main()` → `load_cases()` → one JSON case's `nodes`/`steps`/`checks`
**Scope:** spawns real binaries (`c_main`/`lx_main`/`rlx_main`) as ordinary OS subprocesses on **one machine** — not real cross-VM Qnet
**Spec:** `tools/test-automation/README.md` · module doc comment `runner.py` lines 13-14 (environments A-D) · `docs/test-plan/04-timing-assumptions.md`, `06-concurrency-race.md`

```mermaid
sequenceDiagram
    participant Main as main()<br/>load_cases()
    participant Runner as run_case()<br/>runner.py
    participant N1 as NodeProcess L1<br/>subprocess.Popen
    participant N2 as NodeProcess L2/RL1<br/>subprocess.Popen

    Main->>Runner: one case (nodes/steps/checks), filtered by --filter
    Runner->>Runner: case.skip? -> return ("SKIP", skip_reason)

    Runner->>N1: spawn (stdin/stdout=PIPE)
    Runner->>N2: spawn (stdin/stdout=PIPE)
    par background capture
        N1-->>N1: _read_loop() thread<br/>(ts, line) appended to self.lines
    and
        N2-->>N2: _read_loop() thread
    end

    Runner->>Runner: sleep setup_wait_ms<br/>t0 = time.monotonic()

    loop steps[] sorted by after_ms
        Runner->>Runner: sleep delta since last step
        Runner->>N1: send_key(step.send)
        Runner->>N2: send_key(step.send)
    end

    loop checks[]
        Runner->>N1: wait_for_pattern(pattern, within_ms, since t0, after_ms)
        N1-->>Runner: (True, line) or (False, None) at deadline
    end

    Runner->>Runner: any unmatched? FAIL : PASS
    Runner->>N1: dump_log() then terminate()<br/>(SIGTERM, SIGKILL after 2s)
    Runner->>N2: dump_log() then terminate()
```

`mock_node.py` is not a real node: a ~19-line `stdin`-echo stand-in (prints `"mock_node ready"`, reacts to `'a'`/`'w'`/`'q'`) used only to validate `runner.py`'s own mechanics (spawn/`send_key`/`wait_for_pattern`/log-dump) without a QNX build. Never referenced by a real case's `"binary"` field.

## Code map

| Step | File : Function |
|---|---|
| Load & filter cases | `runner.py : load_cases()`, `main()` |
| Skip check (env C/D cases) | `runner.py : run_case()` — `case.get("skip")`, never a silent pass |
| Spawn one process per node | `runner.py : NodeProcess.__init__()` → `subprocess.Popen()` |
| Capture timestamped output | `runner.py : NodeProcess._read_loop()` (daemon thread, `self.lines` under `self.lock`) |
| Anchor shared clock | `runner.py : run_case()` — `sleep(setup_wait_ms)`, `t0 = time.monotonic()` |
| Scripted keystrokes | `runner.py : run_case()` steps loop → `NodeProcess.send_key()` |
| Pattern/window check | `runner.py : NodeProcess.wait_for_pattern()` (`after_ms`/`within_ms` anchored to `t0`) |
| Verdict + persistence | `runner.py : run_case()` → `NodeProcess.dump_log()` |
| Cleanup (always) | `runner.py : NodeProcess.terminate()` in `finally` — SIGTERM, then SIGKILL after 2s |
| Repeat for race cases | `runner.py : run_case_repeated()` — own `run-<i>/` dir per attempt, stops at first non-PASS |
| Runner self-test fixture | `mock_node.py` — not part of the traffic-control system under test |

## Cross-node view

`runner.py` launches every node a case needs as subprocesses of **one Python process on one machine** — this is environment **(B) multi-node-same-machine**: several binaries running locally satisfy Qnet's same-node name resolution (no `TRAFFIC_NODE_MAP` needed), so `L1`/`L2`/`RL1` talk to each other over real Qnet IPC as one OS's local transport.

It does **not** exercise environment **(C) cross-VM** or **(D) test_client-only** — real multi-machine Qnet messaging. Those cases are marked `"skip": true` with a `skip_reason` and surface as `SKIP`, never a silent pass.

In short: this tool proves the node binaries' local IPC/state machines behave correctly under scripted timing — it does not prove multi-machine Qnet transport correctness.
