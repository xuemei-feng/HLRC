#!/usr/bin/env bash
# Deploy and control a real-node HLRC cluster.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-}"
CONFIG="${HLRC_CONFIG:-${2:-$ROOT/project/config/cluster.ini}}"
LAYOUT="$ROOT/project/config/ip_layout.py"

ini() {
  python3 - "$CONFIG" "$1" "$2" <<'PY'
import configparser, sys
c=configparser.ConfigParser(); c.read(sys.argv[1]); print(c.get(sys.argv[2],sys.argv[3],fallback=""))
PY
}
SSH_USER="$(ini ssh user)"; SSH_USER="${SSH_USER:-root}"
SSH_PORT="$(ini ssh port)"; SSH_PORT="${SSH_PORT:-22}"
REMOTE_ROOT="$(ini ssh remote_root)"; REMOTE_ROOT="${REMOTE_ROOT:-/opt/hlrc}"
RUNTIME_LIB="$REMOTE_ROOT/project/third_party/jerasure/lib:$REMOTE_ROOT/project/third_party/gf-complete/lib"
SSH=(ssh -p "$SSH_PORT" -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new)
RSYNC_SSH="ssh -p $SSH_PORT -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new"
mapfile -t NODE_LINES < <(python3 "$LAYOUT" --ini "$CONFIG" --format nodes)
CLIENT_IP="$(python3 "$LAYOUT" --ini "$CONFIG" --format client-ip)"
COORD_IP="$(python3 "$LAYOUT" --ini "$CONFIG" --format coordinator-ip)"
mapfile -t STORAGE_IPS < <(printf '%s\n' "${NODE_LINES[@]}" | awk '{split($2,a,":"); print a[1]}')

remote() { local host="$1"; shift; "${SSH[@]}" "$SSH_USER@$host" "$@"; }
wait_port() { local host="$1" port="$2"; for _ in $(seq 1 100); do (echo >/dev/tcp/"$host"/"$port") 2>/dev/null && return 0; sleep .1; done; echo "Timeout: $host:$port" >&2; return 1; }
require_distributed() { [ "$(ini topology mode)" = distributed ] || { echo "cluster_remote.sh requires topology.mode=distributed" >&2; exit 2; }; }
coordinator_port() {
  python3 - "$ROOT/project/config/parameterConfiguration.xml" <<'PY'
import sys, xml.etree.ElementTree as ET
print(ET.parse(sys.argv[1]).getroot().findtext("CoordinatorPort"))
PY
}

case "$ACTION" in
  generate)
    require_distributed
    python3 "$ROOT/project/config/generate_xml_from_ini.py" "$CONFIG"
    ;;
  deploy)
    require_distributed
    "$0" generate "$CONFIG"
    hosts=("$CLIENT_IP" "$COORD_IP" "${STORAGE_IPS[@]}")
    mapfile -t hosts < <(printf '%s\n' "${hosts[@]}" | awk '!seen[$0]++')
    for host in "${hosts[@]}"; do
      remote "$host" "mkdir -p '$REMOTE_ROOT/project/cmake/build' '$REMOTE_ROOT/project/config' '$REMOTE_ROOT/project/third_party/jerasure/lib' '$REMOTE_ROOT/project/third_party/gf-complete/lib'"
      rsync -a -e "$RSYNC_SSH" "$ROOT/project/config/nodeInformation.xml" "$ROOT/project/config/parameterConfiguration.xml" "$CONFIG" "$SSH_USER@$host:$REMOTE_ROOT/project/config/"
      rsync -a -e "$RSYNC_SSH" "$ROOT/project/third_party/jerasure/lib/" "$SSH_USER@$host:$REMOTE_ROOT/project/third_party/jerasure/lib/"
      rsync -a -e "$RSYNC_SSH" "$ROOT/project/third_party/gf-complete/lib/" "$SSH_USER@$host:$REMOTE_ROOT/project/third_party/gf-complete/lib/"
    done
    for host in "${STORAGE_IPS[@]}"; do
      rsync -a -e "$RSYNC_SSH" "$ROOT/project/cmake/build/run_datanode" "$ROOT/project/cmake/build/run_proxy" "$SSH_USER@$host:$REMOTE_ROOT/project/cmake/build/"
    done
    rsync -a -e "$RSYNC_SSH" "$ROOT/project/cmake/build/run_coordinator" "$SSH_USER@$COORD_IP:$REMOTE_ROOT/project/cmake/build/"
    rsync -a -e "$RSYNC_SSH" "$ROOT/project/cmake/build/repair_benchmark" "$ROOT/project/cmake/build/main_client" "$SSH_USER@$CLIENT_IP:$REMOTE_ROOT/project/cmake/build/"
    echo "Deployment completed."
    ;;
  start)
    require_distributed
    for entry in "${NODE_LINES[@]}"; do
      read -r id proxy datanode <<<"$entry"; host="${proxy%:*}"
      remote "$host" "cd '$REMOTE_ROOT'; pkill -9 run_datanode 2>/dev/null || true; pkill -9 run_proxy 2>/dev/null || true; nohup env LD_LIBRARY_PATH='$RUNTIME_LIB' ./project/cmake/build/run_datanode '$datanode' </dev/null >'/tmp/hlrc-datanode-$id.log' 2>&1 & echo \$! >'/tmp/hlrc-datanode-$id.pid'; nohup env LD_LIBRARY_PATH='$RUNTIME_LIB' ./project/cmake/build/run_proxy '$proxy' </dev/null >'/tmp/hlrc-proxy-$id.log' 2>&1 & echo \$! >'/tmp/hlrc-proxy-$id.pid'"
    done
    for entry in "${NODE_LINES[@]}"; do
      read -r id proxy datanode <<<"$entry"
      wait_port "${datanode%:*}" "${datanode##*:}"
      wait_port "${proxy%:*}" "${proxy##*:}"
      wait_port "${proxy%:*}" "$(( ${proxy##*:} + 2000 ))"
    done
    remote "$COORD_IP" "cd '$REMOTE_ROOT'; pkill -9 run_coordinator 2>/dev/null || true; nohup env LD_LIBRARY_PATH='$RUNTIME_LIB' ./project/cmake/build/run_coordinator </dev/null >/tmp/hlrc-coordinator.log 2>&1 & echo \$! >/tmp/hlrc-coordinator.pid"
    wait_port "$COORD_IP" "$(coordinator_port)"
    echo "Cluster ready: client=$CLIENT_IP coordinator=$COORD_IP storage_nodes=${#STORAGE_IPS[@]}"
    ;;
  stop)
    require_distributed
    for host in "${STORAGE_IPS[@]}"; do remote "$host" "pkill -9 run_datanode 2>/dev/null || true; pkill -9 run_proxy 2>/dev/null || true"; done
    remote "$COORD_IP" "pkill -9 run_coordinator 2>/dev/null || true"
    ;;
  status)
    require_distributed
    printf 'coordinator %s: ' "$COORD_IP"; remote "$COORD_IP" "pgrep -xc run_coordinator || true"
    for entry in "${NODE_LINES[@]}"; do read -r id proxy datanode <<<"$entry"; host="${proxy%:*}"; printf 'node %s %s: ' "$id" "$host"; remote "$host" "printf 'datanode='; pgrep -xc run_datanode || true; printf 'proxy='; pgrep -xc run_proxy || true"; done
    ;;
  benchmark)
    require_distributed
    K="${3:?Usage: $0 benchmark CONFIG K single|double|multi}"; FAILURE="${4:?missing failure type}"
    remote "$CLIENT_IP" "cd '$REMOTE_ROOT'; env LD_LIBRARY_PATH='$RUNTIME_LIB' ./project/cmake/build/repair_benchmark '$K' '$FAILURE'"
    ;;
  *) echo "Usage: $0 {generate|deploy|start|stop|status} [cluster.ini]"; echo "       $0 benchmark [cluster.ini] K {single|double|multi}"; exit 2;;
esac
