#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/limit_common.sh"

CONFIG_FILE="${SCRIPT_DIR}/project/config/cluster.ini"
load_proxy_topology "$CONFIG_FILE" "$SCRIPT_DIR"
HOSTS_FILE="${SCRIPT_DIR}/proxy_hosts"
USER=$(get_ini ssh user "$CONFIG_FILE")
PORT=$(get_ini ssh port "$CONFIG_FILE")
REMOTE_ROOT=$(get_ini ssh remote_root "$CONFIG_FILE")
USER="${USER:-root}"
PORT="${PORT:-22}"
REMOTE_ROOT="${REMOTE_ROOT:-$SCRIPT_DIR}"

[ -f "$HOSTS_FILE" ] || {
    echo "Proxy hosts file not found: $HOSTS_FILE" >&2
    exit 1
}

REMOTE_COMMAND="cd '$REMOTE_ROOT' && bash unlimit.sh"
echo ">> clear proxy bandwidth limits ..."
PDSH_OUTPUT=$(sudo pdsh -R ssh -w ^"$HOSTS_FILE" -l "$USER" -f 5 -x "-p $PORT" "$REMOTE_COMMAND" 2>&1)
echo "$PDSH_OUTPUT"
if echo "$PDSH_OUTPUT" | grep -qE 'ssh exited with exit code|fail \|'; then
    echo ">> failed on some proxy hosts" >&2
    exit 1
fi
echo ">> done"
