#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/limit_common.sh"

PROXY_GB="${1:-}"
if [ -z "$PROXY_GB" ]; then
    echo "Usage: $0 <proxy_to_proxy_gb>" >&2
    echo "  allowed rates: 0.5, 1, 2, 5, 10 Gb/s" >&2
    exit 1
fi

gb_to_kbps "$PROXY_GB" >/dev/null
run_limit_all_remote "$PROXY_GB"
