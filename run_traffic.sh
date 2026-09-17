#!/bin/sh
# ==============================================================================
# Traffic Control System - VM Quick Launcher for QNX Neutrino RTOS
# Usage:
#   ./run_traffic.sh          # Auto-detect role from hostname (c1_vm, l1_vm, etc.)
#   ./run_traffic.sh c1       # Run Central Controller
#   ./run_traffic.sh l1..l6   # Run Intersection Controller (1 to 6)
#   ./run_traffic.sh rl1..rl3 # Run Railway Crossing Controller (1 to 3)
#   ./run_traffic.sh stop     # Slay/kill existing running controllers
# ==============================================================================

# Ensure we run in /tmp or directory containing the binaries
cd "$(dirname "$0")" 2>/dev/null || cd /tmp

# Make all controller binaries executable in case permissions were reset during transfer
chmod +x central c_main intersection lx_main railway rlx_main 2>/dev/null

# ------------------------------------------------------------------------------
# 1. Handle STOP command
# ------------------------------------------------------------------------------
if [ "$1" = "stop" ] || [ "$1" = "kill" ]; then
    echo "Stopping any running traffic controllers..."
    slay -f central c_main intersection lx_main railway rlx_main 2>/dev/null
    echo "All controllers stopped."
    exit 0
fi

# ------------------------------------------------------------------------------
# 2. Qnet Node Map configuration (10-node mapping across 10 VMs)
# ------------------------------------------------------------------------------
if [ -z "$TRAFFIC_NODE_MAP" ]; then
    export TRAFFIC_NODE_MAP="c1=c1_vm,l1=l1_vm,l2=l2_vm,l3=l3_vm,l4=l4_vm,l5=l5_vm,l6=l6_vm,rl1=rl1_vm,rl2=rl2_vm,rl3=rl3_vm"
fi

# ------------------------------------------------------------------------------
# 3. Determine Role and ID
# ------------------------------------------------------------------------------
ROLE=""
ID=""

# Check manual argument first
ARG1=$(echo "$1" | tr '[:upper:]' '[:lower:]')
case "$ARG1" in
    c1 | c | central | c_main)
        ROLE="central"
        ;;
    l1 | l2 | l3 | l4 | l5 | l6)
        ROLE="intersection"
        ID="${ARG1#l}"
        ;;
    rl1 | rl2 | rl3)
        ROLE="railway"
        ID="${ARG1#rl}"
        ;;
    l | lx | intersection)
        ROLE="intersection"
        ID="$2"
        ;;
    rl | rlx | railway)
        ROLE="railway"
        ID="$2"
        ;;
esac

# If no role determined from arguments, auto-detect from hostname
if [ -z "$ROLE" ]; then
    H=$(hostname 2>/dev/null | tr '[:upper:]' '[:lower:]')
    case "$H" in
        c1* | *c1* | *central*)
            ROLE="central"
            ;;
        rl1* | *rl1*)
            ROLE="railway"; ID="1"
            ;;
        rl2* | *rl2*)
            ROLE="railway"; ID="2"
            ;;
        rl3* | *rl3*)
            ROLE="railway"; ID="3"
            ;;
        l1* | *l1*)
            ROLE="intersection"; ID="1"
            ;;
        l2* | *l2*)
            ROLE="intersection"; ID="2"
            ;;
        l3* | *l3*)
            ROLE="intersection"; ID="3"
            ;;
        l4* | *l4*)
            ROLE="intersection"; ID="4"
            ;;
        l5* | *l5*)
            ROLE="intersection"; ID="5"
            ;;
        l6* | *l6*)
            ROLE="intersection"; ID="6"
            ;;
        *)
            echo "============================================================"
            echo "Error: Could not auto-detect node role from hostname: '$H'"
            echo "============================================================"
            echo "Please run manually with a node name argument:"
            echo "  $0 c1        -> Central Controller"
            echo "  $0 l1 .. l6  -> Intersection Controller 1..6"
            echo "  $0 rl1 .. rl3-> Railway Controller 1..3"
            echo "  $0 stop      -> Stop all running controllers"
            exit 1
            ;;
    esac
fi

# ------------------------------------------------------------------------------
# 4. Locate Binary and Execute
# ------------------------------------------------------------------------------
BIN=""
case "$ROLE" in
    central)
        if [ -f "./central" ] && [ ! -d "./central" ]; then
            BIN="./central"
        elif [ -f "./c_main" ] && [ ! -d "./c_main" ]; then
            BIN="./c_main"
        fi

        if [ -z "$BIN" ]; then
            echo "Error: Neither './central' nor './c_main' file found in $(pwd)"
            exit 1
        fi

        chmod +x "$BIN" 2>/dev/null
        echo "============================================================"
        echo " Launching Central Controller (C1)"
        echo " Hostname : $(hostname)"
        echo " Binary   : $BIN"
        echo " Node Map : $TRAFFIC_NODE_MAP"
        echo "============================================================"
        exec "$BIN"
        ;;

    intersection)
        if [ -z "$ID" ]; then
            echo "Error: Intersection Controller requires an ID between 1 and 6"
            echo "Usage: $0 l<1-6>"
            exit 1
        fi

        if [ -f "./intersection" ] && [ ! -d "./intersection" ]; then
            BIN="./intersection"
        elif [ -f "./lx_main" ] && [ ! -d "./lx_main" ]; then
            BIN="./lx_main"
        fi

        if [ -z "$BIN" ]; then
            echo "Error: Neither './intersection' nor './lx_main' file found in $(pwd)"
            exit 1
        fi

        chmod +x "$BIN" 2>/dev/null
        echo "============================================================"
        echo " Launching Intersection Controller (L$ID)"
        echo " Hostname : $(hostname)"
        echo " Binary   : $BIN $ID"
        echo " Node Map : $TRAFFIC_NODE_MAP"
        echo "============================================================"
        exec "$BIN" "$ID"
        ;;

    railway)
        if [ -z "$ID" ]; then
            echo "Error: Railway Controller requires an ID between 1 and 3"
            echo "Usage: $0 rl<1-3>"
            exit 1
        fi

        if [ -f "./railway" ] && [ ! -d "./railway" ]; then
            BIN="./railway"
        elif [ -f "./rlx_main" ] && [ ! -d "./rlx_main" ]; then
            BIN="./rlx_main"
        fi

        if [ -z "$BIN" ]; then
            echo "Error: Neither './railway' nor './rlx_main' file found in $(pwd)"
            exit 1
        fi

        chmod +x "$BIN" 2>/dev/null
        echo "============================================================"
        echo " Launching Railway Controller (RL$ID)"
        echo " Hostname : $(hostname)"
        echo " Binary   : $BIN $ID"
        echo " Node Map : $TRAFFIC_NODE_MAP"
        echo "============================================================"
        exec "$BIN" "$ID"
        ;;
esac
