#!/bin/bash
# fetch-machine-names.sh
# SSH into every online host, try to read /var/lib/luckfox/mqtt_mapping.txt,
# and update ~/.ipscanner.comments by MAC address.
#
# Behaviour:
#   - No placeholder comment needed — tries ALL online hosts
#   - No existing comment  → saves device label automatically
#   - Comment matches device → skips (prints OK)
#   - Comment differs from device → asks user to confirm update
#
# Usage: sudo ./scripts/fetch-machine-names.sh [interface]
#
# Requirements: sshpass  (sudo apt install sshpass)

_DIR="$(cd "$(dirname "$0")" && pwd)"
if   [ -x "$_DIR/../ipscanner" ];       then SCANNER="$_DIR/../ipscanner"
elif [ -x "$_DIR/../build/ipscanner" ]; then SCANNER="$_DIR/../build/ipscanner"
else echo "Error: ipscanner binary not found"; exit 1; fi

IFACE="${1:-eth0}"
JSON_TMP="/tmp/ipscan_result.json"
SSH_PASS="luckfox"
SSH_USER="root"
MQTT_FILE="/var/lib/luckfox/mqtt_mapping.txt"

# ── 1. Scan network ───────────────────────────────────────────────────────────
echo "[1/2] Scanning $IFACE ..."
"$SCANNER" -i "$IFACE" -j "$JSON_TMP"

# ── 2. SSH into each host and fetch machine_name ──────────────────────────────
echo "[2/2] Fetching machine names from devices ..."
echo ""

python3 << PYEOF
import json, subprocess, re, sys

SCANNER   = "$SCANNER"
SSH_USER  = "$SSH_USER"
SSH_PASS  = "$SSH_PASS"
MQTT_FILE = "$MQTT_FILE"

with open("$JSON_TMP") as f:
    hosts = json.load(f)

added   = 0
updated = 0
kept    = 0

for h in hosts:
    ip      = h['ip']
    mac     = h['mac']
    old_cmt = h.get('comment', '').strip()

    # Try SSH into every host — non-Luckfox devices will fail silently
    try:
        r = subprocess.run(
            ['sshpass', '-p', SSH_PASS,
             'ssh',
             '-o', 'StrictHostKeyChecking=no',
             '-o', 'ConnectTimeout=3',
             f'{SSH_USER}@{ip}',
             f'cat {MQTT_FILE}'],
            capture_output=True, text=True, timeout=8
        )
        if r.returncode != 0:
            continue   # not a Luckfox or unreachable — skip silently

        data         = json.loads(r.stdout)
        machine_name = data.get('machine_name', '').strip()
        if not machine_name:
            continue

    except (subprocess.TimeoutExpired, json.JSONDecodeError, Exception):
        continue

    # Format machine_name → "Line X [GMY]"
    formatted = re.sub(r'\s*(\[GM)', r' \1', machine_name).strip()
    new_cmt   = f"Line {formatted}"

    print(f"  {ip}  (MAC: {mac})")
    print(f"    Device label : {new_cmt}  (from {MQTT_FILE})")

    if not old_cmt:
        # No comment yet → add automatically
        subprocess.run([SCANNER, '-C', f'{mac}={new_cmt}'], capture_output=True)
        print(f"    Added        : {new_cmt}")
        added += 1

    elif old_cmt.lower() == new_cmt.lower():
        # Already correct → skip
        print(f"    OK (matches) : {old_cmt}")
        kept += 1

    else:
        # Mismatch → ask user
        print(f"    Current label: {old_cmt}")
        print(f"    Conflict: device says '{new_cmt}', file says '{old_cmt}'")
        try:
            with open('/dev/tty') as tty:
                sys.stdout.write(f"    Update to '{new_cmt}'? [y/N] ")
                sys.stdout.flush()
                ans = tty.readline().strip().lower()
        except Exception:
            ans = 'n'

        if ans == 'y':
            subprocess.run([SCANNER, '-C', f'{mac}={new_cmt}'], capture_output=True)
            print(f"    Updated to   : {new_cmt}")
            updated += 1
        else:
            print(f"    Kept         : {old_cmt}")
            kept += 1

    print()

print(f"Done: {added} added, {updated} updated, {kept} unchanged.")
print(f"Run 'sudo ./gen-nginx.sh' or click Rescan on the dashboard to refresh.")
PYEOF
