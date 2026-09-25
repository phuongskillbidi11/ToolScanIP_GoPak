#!/bin/bash

SCAN_URL="http://127.0.0.1:8181/api/scan"
REMOTE_LOG="/var/log/isoft-node-oee.log"
BASE_DIR="/var/lib/node-log-collector"
STATE_DIR="$BASE_DIR/state"
LOG_DIR="$BASE_DIR/logs"

mkdir -p "$STATE_DIR" "$LOG_DIR" || {
    echo "Error: cannot create collector directories under $BASE_DIR" >&2
    exit 1
}

scan_tmp=$(mktemp) || exit 1
current_ips_tmp=$(mktemp) || {
    rm -f "$scan_tmp"
    exit 1
}
transfer_tmp=$(mktemp) || {
    rm -f "$scan_tmp" "$current_ips_tmp"
    exit 1
}
payload_tmp=$(mktemp) || {
    rm -f "$scan_tmp" "$current_ips_tmp" "$transfer_tmp"
    exit 1
}
trap 'rm -f "$scan_tmp" "$current_ips_tmp" "$transfer_tmp" "$payload_tmp"' EXIT

if ! curl -fsS "$SCAN_URL" -o "$scan_tmp"; then
    echo "Error: could not fetch the current scan from $SCAN_URL" >&2
    exit 1
fi

# Match gen-nginx.sh: only comments containing both "Line" and "[GM".
if ! python3 - "$scan_tmp" > "$current_ips_tmp" <<'PY'
import ipaddress
import json
import sys

with open(sys.argv[1], encoding="utf-8") as scan_file:
    data = json.load(scan_file)

hosts = data["hosts"]
ips = set()
for host in hosts:
    comment = host.get("comment", "")
    if "Line" not in comment or "[GM" not in comment:
        continue

    ip = host.get("ip", "")
    try:
        parsed = ipaddress.ip_address(ip)
    except ValueError:
        continue
    if parsed.version == 4:
        ips.add(str(parsed))

for ip in sorted(ips, key=lambda value: tuple(map(int, value.split(".")))):
    print(ip)
PY
then
    echo "Error: could not parse the current scan response" >&2
    exit 1
fi

while IFS= read -r ip; do
    [ -n "$ip" ] || continue

    offset_file="$STATE_DIR/$ip.offset"
    local_log="$LOG_DIR/$ip.log"
    offset=0

    if [ -f "$offset_file" ]; then
        IFS= read -r offset < "$offset_file" || offset=0
        case "$offset" in
            ''|*[!0-9]*)
                echo "Warning: invalid stored offset for $ip; restarting at byte 0" >&2
                offset=0
                ;;
        esac
    fi

    ssh_err_tmp="$STATE_DIR/.ssh_err.$$"
    : > "$transfer_tmp"
    ssh_ok=1
    for attempt in 1 2; do
        if ssh -o ConnectTimeout=10 "root@$ip" sh -s -- "$offset" "$REMOTE_LOG" \
            > "$transfer_tmp" 2> "$ssh_err_tmp" <<'REMOTE'
offset=$1
remote_log=$2

remote_size=$(wc -c < "$remote_log") || exit 1
case "$remote_size" in
    ''|*[!0-9]*) exit 1 ;;
esac

if [ "$remote_size" -lt "$offset" ]; then
    start=0
else
    start=$offset
fi

printf 'SIZE %s %s\n' "$remote_size" "$start"
count=$((remote_size - start))
if [ "$count" -gt 0 ]; then
    tail -c "+$((start + 1))" "$remote_log" | head -c "$count"
fi
REMOTE
        then
            ssh_ok=0
            break
        fi
        # A board with a changed SSH host key (this fleet's MAC/host-key
        # instability, documented in this plan's spec.md Background) gets
        # its password auth refused outright by a stale known_hosts entry.
        # Matches the same pattern already handled for Multi SCP
        # (ssh_run_with_hostkey_retry() in src/web.c) — clear the stale
        # entry and retry once.
        if [ "$attempt" -eq 1 ] && grep -q "REMOTE HOST IDENTIFICATION HAS CHANGED" "$ssh_err_tmp" 2>/dev/null; then
            ssh-keygen -R "$ip" >/dev/null 2>&1
            continue
        fi
        break
    done
    cat "$ssh_err_tmp" >&2 2>/dev/null
    rm -f "$ssh_err_tmp"
    if [ "$ssh_ok" -ne 0 ]; then
        echo "Warning: could not collect logs from $ip; skipping" >&2
        continue
    fi

    IFS=' ' read -r marker remote_size start < "$transfer_tmp" || marker=
    if [ "$marker" != "SIZE" ] ||
        [[ ! "$remote_size" =~ ^[0-9]+$ ]] ||
        [[ ! "$start" =~ ^[0-9]+$ ]] ||
        [ "$start" -gt "$remote_size" ]; then
        echo "Warning: invalid response while collecting logs from $ip; skipping" >&2
        continue
    fi

    tail -n +2 "$transfer_tmp" > "$payload_tmp"
    expected_size=$((remote_size - start))
    payload_size=$(wc -c < "$payload_tmp")
    if [ "$payload_size" -ne "$expected_size" ]; then
        echo "Warning: incomplete log transfer from $ip; skipping" >&2
        continue
    fi

    if [ "$payload_size" -gt 0 ] && ! cat "$payload_tmp" >> "$local_log"; then
        echo "Warning: could not append collected logs for $ip; offset unchanged" >&2
        continue
    fi
    offset_tmp="$offset_file.tmp.$$"
    if ! printf '%s\n' "$remote_size" > "$offset_tmp" ||
        ! mv "$offset_tmp" "$offset_file"; then
        echo "Warning: could not update stored offset for $ip" >&2
        rm -f "$offset_tmp"
    fi
done < "$current_ips_tmp"

# Prune orphan state/log pairs for IPs that are not in the current scan.
{
    for path in "$STATE_DIR"/*.offset; do
        [ -e "$path" ] || continue
        name=${path##*/}
        printf '%s\n' "${name%.offset}"
    done
    for path in "$LOG_DIR"/*.log; do
        [ -e "$path" ] || continue
        name=${path##*/}
        printf '%s\n' "${name%.log}"
    done
} | sort -u | while IFS= read -r local_ip; do
    [ -n "$local_ip" ] || continue
    if ! grep -Fqx -- "$local_ip" "$current_ips_tmp"; then
        echo "Pruning orphan files for $local_ip (not in current scan)" >&2
        rm -f -- "$STATE_DIR/$local_ip.offset" "$LOG_DIR/$local_ip.log"
    fi
done
