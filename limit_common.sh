#!/bin/bash
# Shared helpers for proxy-to-proxy bandwidth limiting with Linux tc and IFB.
# Outbound traffic is classified on the physical interface by peer IP + sport/dport.
# Inbound traffic is redirected to IFB and classified by peer IP + sport/dport.
# Matching both sport and dport is required so request and response bulk payloads
# (peer Get/Put return path uses an ephemeral client port) are rate-limited.

IFB_DEV="${IFB_DEV:-ifb0}"
PROXY_DATA_PORT_SHIFT=1000
PROXY_PEER_PORT_SHIFT=2000
FALLBACK_LINK_RATE="100gbit"

get_ini() {
    local section="$1" key="$2" file="$3"
    awk -F'=' -v SECTION="$section" -v KEY="$key" '
        function trim(s) { gsub(/^[ \t]+|[ \t]+$/, "", s); return s }
        /^[ \t]*#/ { next }
        /^[ \t]*\[/ {
            line = trim($0)
            in_section = (line == "[" SECTION "]")
            next
        }
        in_section && NF >= 2 {
            k = trim($1)
            if (k == KEY) {
                v = $0
                sub(/^[^=]*=/, "", v)
                print trim(v)
                exit
            }
        }
    ' "$file"
}

gb_to_kbps() {
    case "$1" in
        0.5) echo 512000 ;;
        1)   echo 1048576 ;;
        2)   echo 2097152 ;;
        5)   echo 5242880 ;;
        10)  echo 10485760 ;;
        *)
            echo "Unsupported bandwidth: $1 Gb/s (allowed: 0.5, 1, 2, 5, 10)" >&2
            return 1
            ;;
    esac
}

kbps_to_rate() {
    echo "${1}kbit"
}

kbps_to_label() {
    case "$1" in
        512000)   echo "0.5Gb/s" ;;
        1048576)  echo "1Gb/s" ;;
        2097152)  echo "2Gb/s" ;;
        5242880)  echo "5Gb/s" ;;
        10485760) echo "10Gb/s" ;;
        *)        echo "${1}kbit/s" ;;
    esac
}

detect_iface() {
    local target_ip="$1" candidate

    if [ -n "$target_ip" ]; then
        candidate=$(ip route get "$target_ip" 2>/dev/null | awk '
            / dev / {
                for (i = 1; i <= NF; i++) {
                    if ($i == "dev") { print $(i + 1); exit }
                }
            }')
        if [ -n "$candidate" ] && [ "$candidate" != "lo" ]; then
            echo "$candidate"
            return 0
        fi
    fi

    candidate=$(ip route show default 2>/dev/null | awk '/default/ { print $5; exit }')
    if [ -n "$candidate" ] && [ "$candidate" != "lo" ]; then
        echo "$candidate"
        return 0
    fi
    return 1
}

detect_link_rate() {
    local iface="$1" speed
    if [ -r "/sys/class/net/$iface/speed" ]; then
        read -r speed < "/sys/class/net/$iface/speed" || true
        if [[ "$speed" =~ ^[0-9]+$ ]] && [ "$speed" -gt 0 ]; then
            echo "${speed}mbit"
            return 0
        fi
    fi
    echo "$FALLBACK_LINK_RATE"
}

load_proxy_topology() {
    local config_file="$1" script_dir="$2"
    local layout_script="${script_dir}/project/config/ip_layout.py"

    TOPOLOGY_MODE=$(get_ini topology mode "$config_file")
    PROXY_PORT=$(get_ini topology proxy_port_start "$config_file")
    if [ "$TOPOLOGY_MODE" != "distributed" ]; then
        echo "Bandwidth limiting requires [topology] mode=distributed (actual: ${TOPOLOGY_MODE:-missing})" >&2
        return 1
    fi
    if ! [[ "$PROXY_PORT" =~ ^[0-9]+$ ]] || [ "$PROXY_PORT" -lt 1 ] || [ "$PROXY_PORT" -gt 62535 ]; then
        echo "Invalid topology.proxy_port_start: ${PROXY_PORT:-missing}" >&2
        return 1
    fi
    if [ ! -f "$layout_script" ]; then
        echo "ip_layout.py not found: $layout_script" >&2
        return 1
    fi

    mapfile -t PROXY_IPS < <(
        PYTHONDONTWRITEBYTECODE=1 python3 "$layout_script" --ini "$config_file" --format proxy-ips
    ) || return 1
    if [ "${#PROXY_IPS[@]}" -lt 2 ]; then
        echo "At least two distributed proxy hosts are required" >&2
        return 1
    fi
}

get_local_proxy_ip() {
    local candidate local_ip
    for candidate in "${PROXY_IPS[@]}"; do
        while IFS= read -r local_ip; do
            if [ "$candidate" = "$local_ip" ]; then
                echo "$candidate"
                return 0
            fi
        done < <(ip -o -4 addr show scope global 2>/dev/null | awk '{print $4}' | cut -d/ -f1)
    done
    return 1
}

ensure_ifb() {
    modprobe ifb numifbs=1 2>/dev/null || true
    if ! ip link show "$IFB_DEV" >/dev/null 2>&1; then
        ip link add "$IFB_DEV" type ifb || return 1
    fi
    ip link set dev "$IFB_DEV" up
}

clear_bandwidth_limits() {
    local iface="$1" quiet="${2:-0}"
    if [ -z "$iface" ]; then
        iface=$(detect_iface "") || return 1
    fi
    tc qdisc del dev "$iface" root 2>/dev/null || true
    tc qdisc del dev "$iface" ingress 2>/dev/null || true
    tc qdisc del dev "$IFB_DEV" root 2>/dev/null || true
    if [ "$quiet" -eq 0 ]; then
        echo "ok | cleared | $iface"
    fi
}

setup_ingress_redirect() {
    local iface="$1"
    ensure_ifb || return 1
    tc qdisc add dev "$iface" handle ffff: ingress || return 1
    tc filter add dev "$iface" parent ffff: protocol ip u32 \
        match u32 0 0 action mirred egress redirect dev "$IFB_DEV" || return 1
}

add_proxy_filters() {
    local dev="$1" parent="$2" direction="$3" local_ip="$4"
    shift 4
    local remote_ip port address_field
    local -a ports=(
        "$PROXY_PORT"
        "$((PROXY_PORT + PROXY_DATA_PORT_SHIFT))"
        "$((PROXY_PORT + PROXY_PEER_PORT_SHIFT))"
    )

    if [ "$direction" = "egress" ]; then
        address_field="dst"
    else
        address_field="src"
    fi

    for remote_ip in "$@"; do
        [ "$remote_ip" = "$local_ip" ] && continue
        for port in "${ports[@]}"; do
            # Request path: local -> remote:proxy_port (dport match).
            tc filter add dev "$dev" protocol ip parent "$parent" prio 10 u32 \
                match ip protocol 6 0xff \
                match ip "$address_field" "$remote_ip/32" \
                match ip dport "$port" 0xffff \
                flowid 1:10 || return 1
            # Response / bulk return path: remote:proxy_port -> local:ephemeral (sport match).
            tc filter add dev "$dev" protocol ip parent "$parent" prio 10 u32 \
                match ip protocol 6 0xff \
                match ip "$address_field" "$remote_ip/32" \
                match ip sport "$port" 0xffff \
                flowid 1:10 || return 1
        done
    done
}

setup_htb() {
    local dev="$1" limited_rate="$2" link_rate="$3"
    tc qdisc add dev "$dev" root handle 1: htb default 30 || return 1
    tc class add dev "$dev" parent 1: classid 1:1 htb rate "$link_rate" ceil "$link_rate" || return 1
    tc class add dev "$dev" parent 1:1 classid 1:10 htb rate "$limited_rate" ceil "$limited_rate" prio 1 || return 1
    tc class add dev "$dev" parent 1:1 classid 1:30 htb rate "$link_rate" ceil "$link_rate" prio 3 || return 1
}

apply_proxy_limits() {
    local iface="$1" rate="$2" link_rate="$3" local_ip="$4"
    shift 4

    clear_bandwidth_limits "$iface" 1
    setup_htb "$iface" "$rate" "$link_rate" || return 1
    add_proxy_filters "$iface" "1:0" egress "$local_ip" "$@" || return 1

    setup_ingress_redirect "$iface" || return 1
    setup_htb "$IFB_DEV" "$rate" "$link_rate" || return 1
    add_proxy_filters "$IFB_DEV" "1:0" ingress "$local_ip" "$@" || return 1
}

apply_bandwidth_limits() {
    local proxy_kbps="$1" script_dir="$3"
    local config_file="${script_dir}/project/config/cluster.ini"
    local local_ip iface rate label link_rate

    load_proxy_topology "$config_file" "$script_dir" || return 1
    local_ip=$(get_local_proxy_ip) || {
        echo "fail | this host is not a configured proxy" >&2
        return 1
    }
    iface=$(detect_iface "${PROXY_IPS[0]}") || {
        echo "fail | no active physical interface" >&2
        return 1
    }
    rate=$(kbps_to_rate "$proxy_kbps")
    label=$(kbps_to_label "$proxy_kbps")
    link_rate=$(detect_link_rate "$iface")

    if ! apply_proxy_limits "$iface" "$rate" "$link_rate" "$local_ip" "${PROXY_IPS[@]}"; then
        clear_bandwidth_limits "$iface" 1
        echo "fail | proxy=$local_ip iface=$iface | tc setup failed" >&2
        return 1
    fi

    echo "ok | proxy=$local_ip iface=$iface | proxy-to-proxy=${label} aggregate | ports=${PROXY_PORT},$((PROXY_PORT + PROXY_DATA_PORT_SHIFT)),$((PROXY_PORT + PROXY_PEER_PORT_SHIFT)) sport+dport | egress+ingress"
}

# Sync the limit/unlimit runtime onto each proxy's ssh.remote_root.
# cluster_remote.sh deploy only ships project binaries/config, so limit scripts
# must be pushed explicitly before pdsh runs them under /opt/hlrc (or similar).
sync_limit_runtime_to_proxies() {
    local script_dir="$1" config_file hosts_file
    local user port remote_root rsync_ssh host
    local -a sync_files

    config_file="${script_dir}/project/config/cluster.ini"
    load_proxy_topology "$config_file" "$script_dir" || return 1

    hosts_file="${script_dir}/proxy_hosts"
    {
        echo "# proxy hosts"
        printf '%s\n' "${PROXY_IPS[@]}"
    } >"$hosts_file"

    user=$(get_ini ssh user "$config_file")
    port=$(get_ini ssh port "$config_file")
    remote_root=$(get_ini ssh remote_root "$config_file")
    user="${user:-root}"
    port="${port:-22}"
    remote_root="${remote_root:-$script_dir}"
    rsync_ssh="ssh -p $port -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new"

    sync_files=(
        "$script_dir/limit.sh"
        "$script_dir/limit_common.sh"
        "$script_dir/unlimit.sh"
        "$script_dir/limit_0.5Gb.sh"
        "$script_dir/limit_1Gb.sh"
        "$script_dir/limit_2Gb.sh"
        "$script_dir/limit_5Gb.sh"
        "$script_dir/limit_10Gb.sh"
    )

    echo ">> sync limit runtime to ${#PROXY_IPS[@]} proxy hosts under $remote_root ..."
    local pids=() host_status=0
    for host in "${PROXY_IPS[@]}"; do
        (
            ssh -p "$port" -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new \
                "$user@$host" "mkdir -p '$remote_root/project/config'" || exit 1
            rsync -a -e "$rsync_ssh" "${sync_files[@]}" "$user@$host:$remote_root/" || exit 1
            rsync -a -e "$rsync_ssh" \
                "$script_dir/project/config/cluster.ini" \
                "$script_dir/project/config/ip_layout.py" \
                "$script_dir/project/config/storage_nodes" \
                "$user@$host:$remote_root/project/config/" || exit 1
        ) &
        pids+=("$!")
    done
    for pid in "${pids[@]}"; do
        wait "$pid" || host_status=1
    done
    [ "$host_status" -eq 0 ] || return 1
}

run_limit_all_remote() {
    local proxy_gb="$1" script_dir config_file hosts_file
    local user port remote_root remote_cmd pdsh_output

    script_dir="$(cd "$(dirname "$0")" && pwd)"
    config_file="${script_dir}/project/config/cluster.ini"
    sync_limit_runtime_to_proxies "$script_dir" || return 1

    hosts_file="${script_dir}/proxy_hosts"
    user=$(get_ini ssh user "$config_file")
    port=$(get_ini ssh port "$config_file")
    remote_root=$(get_ini ssh remote_root "$config_file")
    user="${user:-root}"
    port="${port:-22}"
    remote_root="${remote_root:-$script_dir}"
    remote_cmd="cd '$remote_root' && bash limit_${proxy_gb}Gb.sh"

    echo ">> limit proxy-to-proxy=${proxy_gb}Gb/s on distributed proxy hosts ..."
    pdsh_output=$(sudo pdsh -R ssh -w ^"$hosts_file" -l "$user" -f 5 -x "-p $port" "$remote_cmd" 2>&1)
    echo "$pdsh_output"
    if echo "$pdsh_output" | grep -qE 'ssh exited with exit code|fail \||Bandwidth limiting requires|Invalid topology'; then
        echo ">> failed on some proxy hosts" >&2
        return 1
    fi
    echo ">> done"
}

run_unlimit_all_remote() {
    local script_dir config_file hosts_file
    local user port remote_root remote_cmd pdsh_output

    script_dir="$(cd "$(dirname "$0")" && pwd)"
    config_file="${script_dir}/project/config/cluster.ini"
    sync_limit_runtime_to_proxies "$script_dir" || return 1

    hosts_file="${script_dir}/proxy_hosts"
    user=$(get_ini ssh user "$config_file")
    port=$(get_ini ssh port "$config_file")
    remote_root=$(get_ini ssh remote_root "$config_file")
    user="${user:-root}"
    port="${port:-22}"
    remote_root="${remote_root:-$script_dir}"
    remote_cmd="cd '$remote_root' && bash unlimit.sh"

    echo ">> clear proxy bandwidth limits ..."
    pdsh_output=$(sudo pdsh -R ssh -w ^"$hosts_file" -l "$user" -f 5 -x "-p $port" "$remote_cmd" 2>&1)
    echo "$pdsh_output"
    if echo "$pdsh_output" | grep -qE 'ssh exited with exit code|fail \|'; then
        echo ">> failed on some proxy hosts" >&2
        return 1
    fi
    echo ">> done"
}
