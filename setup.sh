 # ==========================================================
# Custom Cluster Configuration (Traffic Node Map & Static IP)
# ==========================================================
if [ -z "$TRAFFIC_NODE_MAP" ]; then
    export TRAFFIC_NODE_MAP="c1=c1_vm,l1=l1_vm,l2=l2_vm,l3=l3_vm,l4=l4_vm,l5=l5_vm,l6=l6_vm,rl1=rl1_vm,rl2=rl2_vm,rl3=rl3_vm"
fi

HOST=$(hostname)
IP=""

case "$HOST" in
  l1_vm)  IP="169.254.216.81" ;;
  l2_vm)  IP="169.254.216.82" ;;
  l3_vm)  IP="169.254.216.83" ;;
  l4_vm)  IP="169.254.216.84" ;;
  l5_vm)  IP="169.254.216.85" ;;
  l6_vm)  IP="169.254.216.86" ;;
  rl1_vm) IP="169.254.216.91" ;;
  rl2_vm) IP="169.254.216.92" ;;
  rl3_vm) IP="169.254.216.93" ;;
  c1_vm)  IP="169.254.216.100" ;;
  *)      echo "Unknown hostname $HOST; skipping static IP assignment" ;;
esac

if [ -n "$IP" ]; then
  ifconfig wm0 $IP netmask 255.255.0.0 up
fi


# ==========================================================
# Automated Application Launcher (/tmp)
# ==========================================================
BIN_DIR="/tmp"

case "$HOST" in
  c1_vm)
    echo "---> Launching Central Control Program"
    $BIN_DIR/central &
    ;;
  l[1-6]_vm)
    NUM=$(echo "$HOST" | sed -e 's/^l//' -e 's/_vm$//')
    echo "---> Launching Intersection Program for node $NUM"
    $BIN_DIR/intersection "$NUM" &
    ;;
  rl[1-3]_vm)
    NUM=$(echo "$HOST" | sed -e 's/^rl//' -e 's/_vm$//')
    echo "---> Launching Railway Program for node $NUM"
    $BIN_DIR/railway "$NUM" &
    ;;
  *)
    echo "No designated application for host: $HOST"
    ;;
esac
