#!/usr/bin/env python3
"""Self-test-only stand-in for lx_main/rlx_main: reads one char at a time
from stdin (like the real nodes' scanf(" %c", &input) loops) and prints a
fixed reaction. Not part of the real system - used only to validate
runner.py's mechanics on a machine with no QNX binaries available."""
import sys

print("mock_node ready", flush=True)
while True:
    ch = sys.stdin.read(1)
    if not ch:
        break
    if ch == "a":
        print("SIGNAL -> ARTERIAL GREEN", flush=True)
    elif ch == "w":
        print("SIGNAL -> ARTERIAL YELLOW", flush=True)
    elif ch == "q":
        break
