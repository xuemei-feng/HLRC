#!/usr/bin/env bash
# Deploy and control a real-node HLRC cluster (parallel SSH/rsync).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-}"
CONFIG="${HLRC_CONFIG:-${2:-$ROOT/project/config/cluster.ini}}"
LAYOUT="$ROOT/project/config/ip_layout.py"
PARALLEL="${PARALLEL:-32}"

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

# Run fn once per item with up to PARALLEL concurrent jobs.
# Usage: parallel_items fn item1 item2 ...
parallel_items() {
  local fn="$1"; shift
  local -a items=("$@")
  local status=0
  local i=0
  local -a pids=()
  local -a slots=()

  for ((i = 0; i < ${#items[@]}; i++)); do
    while [ "${#pids[@]}" -ge "$PARALLEL" ]; do
      local pid="${pids[0]}"
      unset "pids[0]"
      pids=("${pids[@]}")
      if ! wait "$pid"; then
        status=1
      fi
    done
    "$fn" "${items[$i]}" &
    pids+=("$!")
  done
  for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
      status=1
    fi
  done
  return "$status"
}

deploy_common_to_host() {
  local host="$1"
  remote "$host" "mkdir -p '$REMOTE_ROOT/project/cmake/build' '$REMOTE_ROOT/project/config' '$REMOTE_ROOT/project/third_party/jerasure/lib' '$REMOTE_ROOT/project/third_party/gf-complete/lib'"
  rsync -a -e "$RSYNC_SSH" \
    "$ROOT/project/config/nodeInformation.xml" \
    "$ROOT/project/config/parameterConfiguration.xml" \
    "$CONFIG" \
    "$SSH_USER@$host:$REMOTE_ROOT/project/config/"
  rsync -a -e "$RSYNC_SSH" \
    "$ROOT/project/third_party/jerasure/lib/" \
    "$SSH_USER@$host:$REMOTE_ROOT/project/third_party/jerasure/lib/"
  rsync -a -e "$RSYNC_SSH" \
    "$ROOT/project/third_party/gf-complete/lib/" \
    "$SSH_USER@$host:$REMOTE_ROOT/project/third_party/gf-complete/lib/"
}

deploy_storage_bins() {
  local host="$1"
  rsync -a -e "$RSYNC_SSH" \
    "$ROOT/project/cmake/build/run_datanode" \
    "$ROOT/project/cmake/build/run_proxy" \
    "$SSH_USER@$host:$REMOTE_ROOT/project/cmake/build/"
}

start_storage_node() {
  local entry="$1"
  local id proxy datanode host
  read -r id proxy datanode <<<"$entry"
  host="${proxy%:*}"
  remote "$host" "cd '$REMOTE_ROOT'; pkill -9 run_datanode 2>/dev/null || true; pkill -9 run_proxy 2>/dev/null || true; nohup env LD_LIBRARY_PATH='$RUNTIME_LIB' ./project/cmake/build/run_datanode '$datanode' </dev/null >'/tmp/hlrc-datanode-$id.log' 2>&1 & echo \$! >'/tmp/hlrc-datanode-$id.pid'; nohup env LD_LIBRARY_PATH='$RUNTIME_LIB' HLRC_REPAIR_WORKERS='$REPAIR_WORKERS' ./project/cmake/build/run_proxy '$proxy' </dev/null >'/tmp/hlrc-proxy-$id.log' 2>&1 & echo \$! >'/tmp/hlrc-proxy-$id.pid'"
}

wait_storage_node() {
  local entry="$1"
  local id proxy datanode
  read -r id proxy datanode <<<"$entry"
  wait_port "${datanode%:*}" "${datanode##*:}"
  wait_port "${proxy%:*}" "${proxy##*:}"
  wait_port "${proxy%:*}" "$(( ${proxy##*:} + 2000 ))"
}

stop_storage_host() {
  local host="$1"
  remote "$host" "pkill -9 run_datanode 2>/dev/null || true; pkill -9 run_proxy 2>/dev/null || true"
}

clean_storage_host() {
  local host="$1"
  remote "$host" "rm -rf '$REMOTE_ROOT/storage' '$REMOTE_ROOT/project/cmake/build/storage'"
}

status_storage_node() {
  local entry="$1"
  local id proxy datanode host line outfile
  read -r id proxy datanode <<<"$entry"
  host="${proxy%:*}"
  outfile="$STATUS_DIR/$(printf '%04d' "$id")"
  line="$(remote "$host" "printf 'datanode='; pgrep -xc run_datanode || true; printf 'proxy='; pgrep -xc run_proxy || true")"
  printf 'node %s %s: %s\n' "$id" "$host" "$line" >"$outfile"
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
    echo "Deploy common files -> ${#hosts[@]} hosts (parallel=$PARALLEL)"
    parallel_items deploy_common_to_host "${hosts[@]}"
    echo "Deploy storage binaries -> ${#STORAGE_IPS[@]} hosts (parallel=$PARALLEL)"
    parallel_items deploy_storage_bins "${STORAGE_IPS[@]}"
    echo "Deploy coordinator/client binaries"
    rsync -a -e "$RSYNC_SSH" "$ROOT/project/cmake/build/run_coordinator" "$SSH_USER@$COORD_IP:$REMOTE_ROOT/project/cmake/build/" &
    pid_coord=$!
    rsync -a -e "$RSYNC_SSH" "$ROOT/project/cmake/build/repair_benchmark" "$ROOT/project/cmake/build/main_client" "$SSH_USER@$CLIENT_IP:$REMOTE_ROOT/project/cmake/build/" &
    pid_client=$!
    wait "$pid_coord"
    wait "$pid_client"
    echo "Deployment completed."
    ;;
  start)
    require_distributed
    REPAIR_WORKERS="${HLRC_REPAIR_WORKERS:-32}"
    if ! [[ "$REPAIR_WORKERS" =~ ^[1-9][0-9]{0,2}$ ]] || [ "$REPAIR_WORKERS" -gt 128 ]; then
      echo "HLRC_REPAIR_WORKERS must be in 1..128" >&2
      exit 2
    fi
    echo "Start storage nodes -> ${#NODE_LINES[@]} hosts (parallel=$PARALLEL)"
    parallel_items start_storage_node "${NODE_LINES[@]}"
    echo "Wait for storage ports (parallel=$PARALLEL)"
    parallel_items wait_storage_node "${NODE_LINES[@]}"
    remote "$COORD_IP" "cd '$REMOTE_ROOT'; pkill -9 run_coordinator 2>/dev/null || true; nohup env LD_LIBRARY_PATH='$RUNTIME_LIB' ./project/cmake/build/run_coordinator </dev/null >/tmp/hlrc-coordinator.log 2>&1 & echo \$! >/tmp/hlrc-coordinator.pid"
    wait_port "$COORD_IP" "$(coordinator_port)"
    echo "Cluster ready: client=$CLIENT_IP coordinator=$COORD_IP storage_nodes=${#STORAGE_IPS[@]}"
    ;;
  stop)
    require_distributed
    echo "Stop storage processes -> ${#STORAGE_IPS[@]} hosts (parallel=$PARALLEL)"
    parallel_items stop_storage_host "${STORAGE_IPS[@]}"
    remote "$COORD_IP" "pkill -9 run_coordinator 2>/dev/null || true"
    hosts=("$CLIENT_IP" "$COORD_IP" "${STORAGE_IPS[@]}")
    mapfile -t hosts < <(printf '%s\n' "${hosts[@]}" | awk '!seen[$0]++')
    echo "Clean storage dirs -> ${#hosts[@]} hosts (parallel=$PARALLEL)"
    parallel_items clean_storage_host "${hosts[@]}"
    echo "Cluster stopped; storage cleaned on ${#hosts[@]} hosts."
    ;;
  status)
    require_distributed
    printf 'coordinator %s: ' "$COORD_IP"; remote "$COORD_IP" "pgrep -xc run_coordinator || true"
    STATUS_DIR="$(mktemp -d)"
    export STATUS_DIR
    parallel_items status_storage_node "${NODE_LINES[@]}" || true
    if compgen -G "$STATUS_DIR/*" >/dev/null; then
      cat "$STATUS_DIR"/*
    fi
    rm -rf "$STATUS_DIR"
    ;;
  benchmark)
    require_distributed
    K="${3:?Usage: $0 benchmark CONFIG K single|double|triple|quadruple|muti}"; FAILURE="${4:?missing failure type}"
    printf -v benchmark_args '%q ' "$K" "$FAILURE" "${@:5}"
    remote "$CLIENT_IP" "cd '$REMOTE_ROOT'; env LD_LIBRARY_PATH='$RUNTIME_LIB' ./project/cmake/build/repair_benchmark $benchmark_args"
    ;;
  *)
    echo "Usage: $0 {generate|deploy|start|stop|status} [cluster.ini]"
    echo "       $0 benchmark [cluster.ini] K {single|double|triple|quadruple|muti} [--repeat N] [--warmup N] [--stripe-id ID]"
    echo "Env:   PARALLEL=32 (max concurrent SSH/rsync jobs)"
    echo "       HLRC_REPAIR_WORKERS=32 (repair pool workers per proxy, start only)"
    exit 2
    ;;
esac
