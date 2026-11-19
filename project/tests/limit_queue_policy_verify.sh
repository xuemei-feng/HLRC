#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
source "$ROOT/limit_common.sh"

if [ "${1:-}" != --kernel ]; then
    tc_calls=0
    tc() { tc_calls=$((tc_calls + 1)); }
    PROXY_QUEUE_MODE=invalid
    if apply_proxy_limits dummy0 2097152kbit 100gbit 10.200.0.1 2>/dev/null; then
        echo "FAIL: invalid mode accepted" >&2; exit 1
    fi
    PROXY_QUEUE_MODE=fq_codel
    for setting in PROXY_QUEUE_LIMIT PROXY_QUEUE_MEMORY_BYTES PROXY_QUEUE_TARGET_MS PROXY_QUEUE_INTERVAL_MS; do
        old_value="${!setting}"
        for invalid in 0 -1 abc 999999999999999999999; do
            printf -v "$setting" '%s' "$invalid"
            if apply_proxy_limits dummy0 2097152kbit 100gbit 10.200.0.1 2>/dev/null; then
                echo "FAIL: invalid $setting accepted" >&2; exit 1
            fi
        done
        printf -v "$setting" '%s' "$old_value"
    done
    [ "$tc_calls" -eq 0 ]
    PROXY_QUEUE_TARGET_MS=100
    if validate_queue_settings 2>/dev/null; then
        echo "FAIL: target >= interval accepted" >&2; exit 1
    fi
    PROXY_QUEUE_TARGET_MS=5

    # Exercise the actual remote command builder without SSH, rsync or pdsh.
    sync_limit_runtime_to_proxies() { :; }
    get_ini() {
        case "$2" in user) echo root ;; port) echo 22 ;; remote_root) echo /opt/hlrc ;; esac
    }
    sudo() {
        [ "$1" = pdsh ]
        printf '%s\n' "${@: -1}"
    }
    PROXY_QUEUE_MODE=legacy
    PROXY_QUEUE_LIMIT=2048
    result="$(run_limit_all_remote 2)"
    [[ "$result" == *"PROXY_QUEUE_MODE='legacy'"* ]]
    [[ "$result" == *"PROXY_QUEUE_LIMIT='2048'"* ]]
    [[ "$result" == *"PROXY_QUEUE_MEMORY_BYTES='16777216'"* ]]
    [[ "$result" == *"PROXY_QUEUE_TARGET_MS='5'"* ]]
    [[ "$result" == *"PROXY_QUEUE_INTERVAL_MS='100'"* ]]
    [[ "$result" == *"bash limit_2Gb.sh"* ]]
    echo "PASS: invalid settings leave tc untouched; cluster command forwards queue settings"
    exit 0
fi

# Run ONLY inside an isolated network namespace:
# unshare -n -- bash project/tests/limit_queue_policy_verify.sh --kernel
if [ "$(readlink /proc/self/ns/net)" = "$(readlink /proc/1/ns/net)" ]; then
    echo "FAIL: kernel test requires an isolated network namespace" >&2; exit 1
fi
PROXY_PORT=50405
IFB_DEV=ifb-queue-test
ip link add queue-test type dummy
ip addr add 10.200.0.1/24 dev queue-test
ip link set queue-test up
check_policy() {
    python3 - "$PROXY_QUEUE_MODE" "$PROXY_QUEUE_LIMIT" "$PROXY_QUEUE_MEMORY_BYTES" <<'PY'
import json, math, re, subprocess, sys
mode, packet_limit, memory_limit = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
def tc(*args):
    return json.loads(subprocess.check_output(['tc', '-j'] + list(args)))
for dev in ('queue-test', 'ifb-queue-test'):
    # Some tc versions still print HTB classes as text even with -j.
    classes = subprocess.check_output(['tc', 'class', 'show', 'dev', dev], text=True)
    limited = next(line for line in classes.splitlines() if line.startswith('class htb 1:10 '))
    for setting in ('rate', 'ceil'):
        value, unit = re.search(r'\b' + setting + r' ([\d.]+)([KMG]?bit)\b', limited).groups()
        bits = float(value) * {'bit': 1, 'Kbit': 1000, 'Mbit': 1000000, 'Gbit': 1000000000}[unit]
        assert math.isclose(bits, 2097152000, rel_tol=0.001), limited
    leaves = [q for q in tc('qdisc', 'show', 'dev', dev) if q.get('handle') == '110:']
    if mode == 'fq_codel':
        assert len(leaves) == 1 and leaves[0]['kind'] == 'fq_codel', leaves
        assert leaves[0]['parent'] == '1:10', leaves
        options = leaves[0]['options']
        assert options['limit'] == packet_limit, options
        assert options['memory_limit'] == memory_limit, options
        assert 4500 <= options['target'] <= 5500, options
        assert 95000 <= options['interval'] <= 105000, options
    else:
        assert not leaves, leaves
    assert tc('filter', 'show', 'dev', dev, 'parent', '1:'), 'proxy filters missing'
print('PASS: ' + mode + ' on egress + IFB; rate/ceil and proxy filters retained')
PY
}
apply_proxy_limits queue-test 2097152kbit 100gbit 10.200.0.1 10.200.0.1 10.200.0.2
check_policy
before="$(tc -j qdisc show dev queue-test)"
PROXY_QUEUE_MODE=invalid
if apply_proxy_limits queue-test 2097152kbit 100gbit 10.200.0.1 10.200.0.2 2>/dev/null; then
    echo "FAIL: invalid mode accepted" >&2; exit 1
fi
[ "$before" = "$(tc -j qdisc show dev queue-test)" ]
PROXY_QUEUE_MODE=legacy
apply_proxy_limits queue-test 2097152kbit 100gbit 10.200.0.1 10.200.0.1 10.200.0.2
check_policy
PROXY_QUEUE_MODE=fq_codel
PROXY_QUEUE_LIMIT=2048
PROXY_QUEUE_MEMORY_BYTES=8388608
apply_proxy_limits queue-test 2097152kbit 100gbit 10.200.0.1 10.200.0.1 10.200.0.2
check_policy
clear_bandwidth_limits queue-test 1
echo "PASS: policy switching, custom limits, invalid-setting protection and cleanup"
