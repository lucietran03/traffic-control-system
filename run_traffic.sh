#!/bin/sh

# ==============================================================================
# Traffic Control System - VM Quick Launcher for QNX Neutrino RTOS
#
# Usage:
#   ./run_traffic.sh          # Auto-detect role from hostname
#   ./run_traffic.sh c1       # Run Central Controller
#   ./run_traffic.sh l1       # Run Intersection Controller 1
#   ./run_traffic.sh l1..l6   # Run Intersection Controller 1..6
#   ./run_traffic.sh rl1..rl3 # Run Railway Controller 1..3
#   ./run_traffic.sh stop     # Stop existing controllers
# ==============================================================================

# ------------------------------------------------------------------------------
# 0. Move to directory containing this script/binaries
# ------------------------------------------------------------------------------

SCRIPT_DIR=$(dirname "$0")

cd "$SCRIPT_DIR" 2>/dev/null || {
    echo "Warning: Could not enter script directory. Trying /tmp..."
    cd /tmp || {
        echo "Error: Could not enter /tmp"
        exit 1
    }
}

# Make controller binaries executable if they exist.
chmod +x central c_main intersection lx_main railway rlx_main 2>/dev/null


# ------------------------------------------------------------------------------
# 1. Handle STOP command
# ------------------------------------------------------------------------------

if [ "$1" = "stop" ] || [ "$1" = "kill" ]; then
    echo "Stopping any running traffic controllers..."

    slay -f central 2>/dev/null
    slay -f c_main 2>/dev/null
    slay -f intersection 2>/dev/null
    slay -f lx_main 2>/dev/null
    slay -f railway 2>/dev/null
    slay -f rlx_main 2>/dev/null

    echo "All controllers stopped."
    exit 0
fi


# ------------------------------------------------------------------------------
# 2. Qnet Node Map configuration
# ------------------------------------------------------------------------------

if [ -z "$TRAFFIC_NODE_MAP" ]; then
    TRAFFIC_NODE_MAP="c1=c1_vm,l1=l1_vm,l2=l2_vm,l3=l3_vm,l4=l4_vm,l5=l5_vm,l6=l6_vm,rl1=rl1_vm,rl2=rl2_vm,rl3=rl3_vm"
    export TRAFFIC_NODE_MAP
fi


# ------------------------------------------------------------------------------
# 3. Determine Role and ID
# ------------------------------------------------------------------------------

ROLE=""
ID=""

# Convert first argument to lowercase.
ARG1=$(echo "$1" | tr '[:upper:]' '[:lower:]')

case "$ARG1" in
    c1|c|central|c_main)
        ROLE="central"
        ;;

    l1|l2|l3|l4|l5|l6)
        ROLE="intersection"
        ID="${ARG1#l}"
        ;;

    rl1|rl2|rl3)
        ROLE="railway"
        ID="${ARG1#rl}"
        ;;

    l|lx|intersection)
        ROLE="intersection"
        ID="$2"
        ;;

    rl|rlx|railway)
        ROLE="railway"
        ID="$2"
        ;;
esac


# ------------------------------------------------------------------------------
# 4. Auto-detect role from hostname if no argument was supplied
# ------------------------------------------------------------------------------

if [ -z "$ROLE" ]; then

    H=$(hostname 2>/dev/null | tr '[:upper:]' '[:lower:]')

    case "$H" in
        *rl1*)
            ROLE="railway"
            ID="1"
            ;;

        *rl2*)
            ROLE="railway"
            ID="2"
            ;;

        *rl3*)
            ROLE="railway"
            ID="3"
            ;;

        *l1*)
            ROLE="intersection"
            ID="1"
            ;;

        *l2*)
            ROLE="intersection"
            ID="2"
            ;;

        *l3*)
            ROLE="intersection"
            ID="3"
            ;;

        *l4*)
            ROLE="intersection"
            ID="4"
            ;;

        *l5*)
            ROLE="intersection"
            ID="5"
            ;;

        *l6*)
            ROLE="intersection"
            ID="6"
            ;;

        *c1*|*central*)
            ROLE="central"
            ;;

        *)
            echo "============================================================"
            echo "Error: Could not auto-detect node role."
            echo "Hostname: '$H'"
            echo "============================================================"
            echo ""
            echo "Run manually:"
            echo "  $0 c1         Central Controller"
            echo "  $0 l1         Intersection Controller 1"
            echo "  $0 l2         Intersection Controller 2"
            echo "  $0 l3         Intersection Controller 3"
            echo "  $0 l4         Intersection Controller 4"
            echo "  $0 l5         Intersection Controller 5"
            echo "  $0 l6         Intersection Controller 6"
            echo "  $0 rl1        Railway Controller 1"
            echo "  $0 rl2        Railway Controller 2"
            echo "  $0 rl3        Railway Controller 3"
            echo "  $0 stop       Stop all controllers"
            exit 1
            ;;
    esac
fi


# ------------------------------------------------------------------------------
# 5. Locate Binary and Execute
# ------------------------------------------------------------------------------

BIN=""

case "$ROLE" in

    # --------------------------------------------------------------------------
    # Central Controller
    # --------------------------------------------------------------------------

    central)

        if [ -f "./central" ] && [ ! -d "./central" ]; then
            BIN="./central"
        elif [ -f "./c_main" ] && [ ! -d "./c_main" ]; then
            BIN="./c_main"
        fi

        if [ -z "$BIN" ]; then
            echo "Error: Neither './central' nor './c_main' found in:"
            pwd
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


    # --------------------------------------------------------------------------
    # Intersection Controller
    # --------------------------------------------------------------------------

    intersection)

        case "$ID" in
            1|2|3|4|5|6)
                ;;
            *)
                echo "Error: Intersection Controller requires ID 1..6"
                echo "Usage: $0 l1"
                echo "       $0 l2"
                echo "       ..."
                echo "       $0 l6"
                exit 1
                ;;
        esac

        if [ -f "./intersection" ] && [ ! -d "./intersection" ]; then
            BIN="./intersection"
        elif [ -f "./lx_main" ] && [ ! -d "./lx_main" ]; then
            BIN="./lx_main"
        fi

        if [ -z "$BIN" ]; then
            echo "Error: Neither './intersection' nor './lx_main' found in:"
            pwd
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


    # --------------------------------------------------------------------------
    # Railway Controller
    # --------------------------------------------------------------------------

    railway)

        case "$ID" in
            1|2|3)
                ;;
            *)
                echo "Error: Railway Controller requires ID 1..3"
                echo "Usage: $0 rl1"
                echo "       $0 rl2"
                echo "       $0 rl3"
                exit 1
                ;;
        esac

        if [ -f "./railway" ] && [ ! -d "./railway" ]; then
            BIN="./railway"
        elif [ -f "./rlx_main" ] && [ ! -d "./rlx_main" ]; then
            BIN="./rlx_main"
        fi

        if [ -z "$BIN" ]; then
            echo "Error: Neither './railway' nor './rlx_main' found in:"
            pwd
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


    # --------------------------------------------------------------------------
    # Unexpected role
    # --------------------------------------------------------------------------

    *)
        echo "Error: Unknown controller role '$ROLE'"
        exit 1
        ;;
esac
