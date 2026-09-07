# Live dashboard (demo tool, read-only)

A small, dependency-free visualization for presenting the traffic-control
system live instead of tiling 10 raw terminal windows. **It is not part of
the graded QNX codebase** — it never touches, builds, or sends anything
into `app/`. It only reads the text that `c_main` already prints once a
second via `c_hmi_render()` (`app/central/src/c_hmi.c`), and renders it as
a network map in a browser.

Read-only by design: all simulated input (vehicle demand, pedestrian
requests, train approach, operator commands) still happens exactly as
before, on each node's own keyboard. The dashboard only observes.

## Requirements

- Python 3 (stdlib only — no `pip install` needed).
- A modern browser. No frontend build step (plain HTML/CSS/JS).

## Running it

1. Start `c_main` with its output redirected to a file:
   ```bash
   ./build/bin/c_main > status.log 2>&1 &
   ```
2. Start the dashboard server, pointing it at that file:
   ```bash
   python3 tools/dashboard/server.py --log status.log --port 8080
   ```
3. Open `http://localhost:8080` in a browser.
4. Run the other 9 nodes as usual, on their own terminals/VMs.

The overview map updates automatically every ~500ms (plain HTTP polling of
`/state.json`, no manual refresh needed). Click any `L1`-`L6`/`RL1`-`RL3`
box to open its detail panel (signal/gate diagram, mode/phase/supervisory,
active faults, sensor bits).

## If Central runs on a different VM than the one you present from

The dashboard only needs read access to `status.log` — it doesn't need to
run on the same machine as `c_main`. Two options:

- Run `server.py` directly on Central's VM and reach it from your host
  browser through a forwarded port (see `docs/QNX_DEPLOYMENT_RUN_GUIDE.md`
  for the VirtualBox port-forwarding steps already used for other
  purposes).
- Or keep `status.log` synced to wherever you run `server.py` (e.g. a
  VirtualBox shared folder, or `rsync -a vm:status.log status.log` on a
  loop) and point `--log` at the local copy.

## How the parsing works

`server.py`'s `ROW_RE` regex matches exactly one data row of
`c_hmi_render()`'s table (ID/ROLE/MODE/PHASE/CROSSING_STATE/SUPERVISORY/
FAULTS/SENSOR/OVERRIDE/AVAILABILITY). If `c_hmi.c`'s print format ever
changes, update `ROW_RE` and the corresponding field list in
`static/app.js` (`PHASE_NAMES`/`SUPERVISORY_NAMES`/`CROSSING_NAMES`/
`FAULT_BITS`/`SENSOR_BITS`, mirrored from `app/shared/includes/sys_types.h`)
to match.

`C1` itself never appears as a data row (it doesn't report to itself —
see `c_mode_eng_t.controllers[9]`, which only holds the 9 remote
controllers); the dashboard just shows C1 as "online" whenever
`status.log` is receiving fresh lines at all.
