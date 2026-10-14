#!/bin/bash
# Start and verify a flat localhost topology with one proxy and one datanode per node.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
CONFIG="${1:-$ROOT/project/config/cluster.ini}"
BUILD="$ROOT/project/cmake/build"
mapfile -t NODES < <(PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/project/config/ip_layout.py" --ini "$CONFIG" --format nodes)
[ "${#NODES[@]}" -gt 0 ] || { echo "No nodes configured" >&2; exit 1; }

wait_port() {
  local host="$1" port="$2" label="$3"
  for _ in $(seq 1 100); do
    if (echo >"/dev/tcp/$host/$port") 2>/dev/null; then return 0; fi
    sleep 0.05
  done
  echo "Timed out waiting for $label at $host:$port" >&2
  return 1
}

pkill -9 run_datanode 2>/dev/null || true
pkill -9 run_proxy 2>/dev/null || true
rm -f /tmp/hlrc-datanode-*.pid /tmp/hlrc-proxy-*.pid

for entry in "${NODES[@]}"; do
  read -r node_id proxy datanode <<<"$entry"
  nohup "$BUILD/run_datanode" "$datanode" </dev/null >"/tmp/hlrc-datanode-$node_id.log" 2>&1 &
  echo $! >"/tmp/hlrc-datanode-$node_id.pid"
done
for entry in "${NODES[@]}"; do
  read -r node_id proxy datanode <<<"$entry"
  wait_port "${datanode%:*}" "${datanode##*:}" "datanode $node_id"
done

for entry in "${NODES[@]}"; do
  read -r node_id proxy datanode <<<"$entry"
  nohup "$BUILD/run_proxy" "$proxy" </dev/null >"/tmp/hlrc-proxy-$node_id.log" 2>&1 &
  echo $! >"/tmp/hlrc-proxy-$node_id.pid"
done
for entry in "${NODES[@]}"; do
  read -r node_id proxy datanode <<<"$entry"
  proxy_port="${proxy##*:}"
  wait_port "${proxy%:*}" "$proxy_port" "proxy grpc $node_id"
  wait_port "${proxy%:*}" "$((proxy_port + 2000))" "proxy peer $node_id"
done

dn_count=$(pgrep -xc run_datanode || true)
proxy_count=$(pgrep -xc run_proxy || true)
expected=${#NODES[@]}
if [ "$dn_count" -ne "$expected" ] || [ "$proxy_count" -ne "$expected" ]; then
  echo "Process count mismatch: expected=$expected datanodes=$dn_count proxies=$proxy_count" >&2
  exit 1
fi
echo "Verified $expected datanodes and $expected proxies; all gRPC and peer ports are listening."
