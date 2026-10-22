#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/limit_common.sh"

CONFIG_FILE="${SCRIPT_DIR}/project/config/cluster.ini"
load_proxy_topology "$CONFIG_FILE" "$SCRIPT_DIR"
LOCAL_IP=$(get_local_proxy_ip) || {
    echo "fail | this host is not a configured proxy" >&2
    exit 1
}
IFACE=$(detect_iface "${PROXY_IPS[0]}") || {
    echo "fail | no active physical interface" >&2
    exit 1
}
clear_bandwidth_limits "$IFACE"
