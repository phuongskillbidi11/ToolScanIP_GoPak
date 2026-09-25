#!/bin/bash
# Install Pi 1's fleet SSH key on Luckfox nodes. Run as root on Pi 1.
# Usage: distribute-fleet-key.sh [ip ...]   (no args = every node in the current scan)
# Idempotent: nodes where the key already works are skipped.

KEY=/root/.ssh/id_ed25519_fleet
PUB="$KEY.pub"
PASSFILE=/root/.fleet-passwords
SCAN_URL="http://127.0.0.1:8181/api/scan"
TAG="ipscanner-log-collector@pi1"
SSH_OPTS=(-o StrictHostKeyChecking=accept-new -o ConnectTimeout=10 -o LogLevel=ERROR)

if [ ! -f "$KEY" ] || [ ! -f "$PUB" ]; then
    echo "Error: $KEY / $PUB missing — generate the fleet key first" >&2
    exit 1
fi
if [ ! -f "$PASSFILE" ]; then
    echo "Error: $PASSFILE missing — create it (root-only, one password per line)" >&2
    exit 1
fi

pubkey=$(head -n1 "$PUB")
case "$pubkey" in
    *"'"*|"")
        echo "Error: unexpected content in $PUB" >&2
        exit 1
        ;;
esac
case "$pubkey" in
    *" $TAG") ;;
    *)
        echo "Error: $PUB does not end with the expected comment $TAG" >&2
        exit 1
        ;;
esac

nodes=()
if [ "$#" -gt 0 ]; then
    nodes=("$@")
else
    if ! scan=$(curl -fsS "$SCAN_URL"); then
        echo "Error: could not fetch the current scan from $SCAN_URL" >&2
        exit 1
    fi
    # Same filter as collect-node-logs.sh / gen-nginx.sh: "Line" and "[GM" in the comment.
    if ! mapfile -t nodes < <(printf '%s' "$scan" | python3 -c '
import ipaddress, json, sys
ips = set()
for host in json.load(sys.stdin)["hosts"]:
    comment = host.get("comment", "")
    if "Line" not in comment or "[GM" not in comment:
        continue
    try:
        parsed = ipaddress.ip_address(host.get("ip", ""))
    except ValueError:
        continue
    if parsed.version == 4:
        ips.add(str(parsed))
for ip in sorted(ips, key=lambda v: tuple(map(int, v.split(".")))):
    print(ip)
'); then
        echo "Error: could not parse the current scan response" >&2
        exit 1
    fi
    if [ "${#nodes[@]}" -eq 0 ]; then
        echo "Error: the current scan contains 0 fleet nodes — nothing to do" >&2
        exit 1
    fi
fi

# Remote install, fed on stdin. The key line is embedded here so it never
# passes through remote-shell argument quoting.
install_script="umask 077
mkdir -p /root/.ssh && chmod 700 /root/.ssh || exit 1
f=/root/.ssh/authorized_keys
touch \"\$f\" && chmod 600 \"\$f\" || exit 1
[ -s \"\$f\" ] && [ -n \"\$(tail -c1 \"\$f\")\" ] && echo >> \"\$f\"
grep -qxF '$pubkey' \"\$f\" || echo '$pubkey' >> \"\$f\""

err_tmp=$(mktemp) || exit 1
trap 'rm -f "$err_tmp"' EXIT

key_works() {
    ssh -n -i "$KEY" -o IdentitiesOnly=yes -o BatchMode=yes "${SSH_OPTS[@]}" "root@$1" true 2>/dev/null
}

ok=0
skip=0
fail=0

for ip in "${nodes[@]}"; do
    if key_works "$ip"; then
        echo "SKIP $ip (key already works)"
        skip=$((skip + 1))
        continue
    fi

    result="no password worked"
    good_pw=""
    while IFS= read -r -u 3 pw; do
        [ -n "$pw" ] || continue
        SSHPASS="$pw" sshpass -e ssh -o PubkeyAuthentication=no "${SSH_OPTS[@]}" "root@$ip" sh -s <<< "$install_script" > /dev/null 2> "$err_tmp"
        rc=$?
        if grep -q "REMOTE HOST IDENTIFICATION HAS CHANGED" "$err_tmp"; then
            result="host key changed — verify, then: ssh-keygen -R $ip"
            break
        fi
        if [ "$rc" -eq 0 ]; then
            result="installed"
            good_pw="$pw"
            break
        fi
        # 5 = sshpass wrong password, 6 = unknown host key, 255 = ssh auth/connection error
        case "$rc" in
            5|6|255) continue ;;
        esac
        result="install commands failed on node (exit $rc)"
        break
    done 3< "$PASSFILE"

    if [ "$result" != "installed" ]; then
        echo "FAIL $ip ($result)"
        fail=$((fail + 1))
        continue
    fi

    if key_works "$ip"; then
        echo "OK $ip"
        ok=$((ok + 1))
    else
        echo "FAIL $ip (key check failed after install)"
        SSHPASS="$good_pw" sshpass -e ssh -n -o PubkeyAuthentication=no "${SSH_OPTS[@]}" "root@$ip" 'ls -ld /root /root/.ssh /root/.ssh/authorized_keys' 2>&1 | sed 's/^/    /'
        fail=$((fail + 1))
    fi
    unset pw good_pw
done

echo "Summary: $ok ok, $skip skip, $fail fail"
