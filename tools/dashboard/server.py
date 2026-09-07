#!/usr/bin/env python3
"""
Read-only live dashboard backend for the Traffic Control System.

Does NOT touch, build, or run any QNX code. It only reads the stdout of an
already-running `c_main` process, which already prints a full status table
once per second via c_hmi_render() (app/central/src/c_hmi.c) - this script
just tails that output, parses the table, and serves it as JSON.

The frontend (static/) polls /state.json every ~500ms - plain HTTP, no
WebSocket library, no pip install required (stdlib only), so it runs
anywhere Python 3 is available, independent of the QNX toolchain.

Usage:
    ./build/bin/c_main > status.log 2>&1 &
    python3 tools/dashboard/server.py --log status.log --port 8080
    # open http://localhost:8080 in a browser

If c_main runs on a separate VM from the one you view the dashboard on,
either run this script on that same VM and reach it via a forwarded port,
or continuously copy/sync status.log to wherever this script runs
(e.g. `rsync` on a loop, or a shared folder) and point --log at the copy.
"""
import argparse
import http.server
import json
import re
import socketserver
import threading
import time
from pathlib import Path

STATIC_DIR = Path(__file__).parent / "static"

# One data row of c_hmi_render()'s table (app/central/src/c_hmi.c), e.g.:
#   "L3    INTERSECTION  1         2      -                3            0x0      0x5      1         AVAILABLE"
# Columns: ID ROLE MODE PHASE CROSSING_STATE SUPERVISORY FAULTS SENSOR OVERRIDE AVAILABILITY
ROW_RE = re.compile(
    r"^(?P<id>C1|L[1-6]|RL[1-3])\s+"
    r"(?P<role>CENTRAL|INTERSECTION|RAILWAY)\s+"
    r"(?P<mode>\d+)\s+"
    r"(?P<phase>\d+|-)\s+"
    r"(?P<crossing>\d+|-)\s+"
    r"(?P<supervisory>\d+)\s+"
    r"(?P<faults>0x[0-9a-fA-F]+)\s+"
    r"(?P<sensor>0x[0-9a-fA-F]+|-)\s+"
    r"(?P<override>\d+)\s+"
    r"(?P<availability>AVAILABLE|UNAVAILABLE)\s*$"
)

state_lock = threading.Lock()
latest_state = {"nodes": {}, "last_update_ts": 0, "log_path": None, "started_ts": time.time()}


def parse_line(line):
    m = ROW_RE.match(line.strip())
    if not m:
        return None
    d = m.groupdict()
    return {
        "id": d["id"],
        "role": d["role"],
        "mode": int(d["mode"]),
        "phase": None if d["phase"] == "-" else int(d["phase"]),
        "crossing_state": None if d["crossing"] == "-" else int(d["crossing"]),
        "supervisory": int(d["supervisory"]),
        "faults": int(d["faults"], 16),
        "sensor_status": None if d["sensor"] == "-" else int(d["sensor"], 16),
        "override_active": bool(int(d["override"])),
        "availability": d["availability"],
    }


def tail_log(path):
    """Follow `path` like `tail -f`, feeding complete lines to parse_line().

    Starts from the BEGINNING of the file (not the end) so a dashboard
    started after c_main has already been running for a while still picks
    up the most recent state of every node on its first read, instead of
    showing nothing until the next table print.
    """
    while not Path(path).exists():
        time.sleep(0.5)
    with open(path, "r", errors="replace") as f:
        buf = ""
        while True:
            chunk = f.read()
            if not chunk:
                time.sleep(0.2)
                continue
            buf += chunk
            *complete, buf = buf.split("\n")
            changed = False
            for line in complete:
                row = parse_line(line)
                if row:
                    with state_lock:
                        latest_state["nodes"][row["id"]] = row
                        changed = True
            if changed:
                with state_lock:
                    latest_state["last_update_ts"] = time.time()


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(STATIC_DIR), **kwargs)

    def do_GET(self):
        if self.path == "/state.json":
            with state_lock:
                payload = json.dumps(latest_state).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(payload)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(payload)
            return
        super().do_GET()

    def log_message(self, fmt, *args):
        pass  # keep the console quiet during a demo


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log", required=True, help="Path to c_main's redirected stdout log")
    ap.add_argument("--port", type=int, default=8080)
    args = ap.parse_args()

    latest_state["log_path"] = args.log
    t = threading.Thread(target=tail_log, args=(args.log,), daemon=True)
    t.start()

    with socketserver.ThreadingTCPServer(("0.0.0.0", args.port), Handler) as httpd:
        print(f"Dashboard running: http://localhost:{args.port}  (tailing {args.log})")
        print("Ctrl+C to stop. This does not affect c_main or any QNX process.")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
