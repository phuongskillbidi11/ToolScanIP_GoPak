#!/bin/bash
# fetch-machine-names.sh
# SSH into each online Luckfox device, read /var/lib/luckfox/mqtt_mapping.txt,
# extract machine_name, and update ~/.ipscanner.comments by MAC address.
#
# Usage: sudo ./scripts/fetch-machine-names.sh [interface]
#
# Requirements: sshpass

# Binary is at ../ipscanner (Pi) or ../build/ipscanner (local dev)
_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -x "$_DIR/../ipscanner" ]; then
    SCANNER="$_DIR/../ipscanner"
elif [ -x "$_DIR/../build/ipscanner" ]; then
    SCANNER="$_DIR/../build/ipscanner"
else
    echo "Error: ipscanner binary not found"
    exit 1
fi
IFACE="${1:-eth0}"
JSON_TMP="/tmp/ipscan_result.json"
SSH_PASS="luckfox"
SSH_USER="root"
MQTT_FILE="/var/lib/luckfox/mqtt_mapping.txt"

# ── 1. Scan network ───────────────────────────────────────────────────────────
echo "[1/2] Scanning $IFACE ..."
"$SCANNER" -i "$IFACE" -j "$JSON_TMP"

# ── 2. SSH into each Luckfox device and fetch machine_name ───────────────────
echo "[2/2] Fetching machine names from devices ..."
echo ""

python3 << PYEOF
import json, subprocess, re, sys

SCANNER  = "$SCANNER"
SSH_USER = "$SSH_USER"
SSH_PASS = "$SSH_PASS"
MQTT_FILE = "$MQTT_FILE"

with open("$JSON_TMP") as f:
    hosts = json.load(f)

# Only process devices that look like Luckfox (Line + [GM in comment)
devices = [h for h in hosts
           if 'Line' in h.get('comment','') and '[GM' in h.get('comment','')]

if not devices:
    print("  No Luckfox devices found. Make sure they have 'Line' and '[GM' comments.")
    sys.exit(0)

updated = 0
failed  = 0

for h in devices:
    ip      = h['ip']
    mac     = h['mac']
    old_cmt = h.get('comment','').strip()

    print(f"  {ip}  ({old_cmt})")

    try:
        result = subprocess.run(
            ['sshpass', '-p', SSH_PASS,
             'ssh',
             '-o', 'StrictHostKeyChecking=no',
             '-o', 'ConnectTimeout=5',
             f'{SSH_USER}@{ip}',
             f'cat {MQTT_FILE}'],
            capture_output=True, text=True, timeout=10
        )

        if result.returncode != 0:
            print(f"    SSH failed: {result.stderr.strip() or 'no output'}")
            failed += 1
            continue

        data = json.loads(result.stdout)
        machine_name = data.get('machine_name', '').strip()

        if not machine_name:
            print(f"    No machine_name in {MQTT_FILE}")
            failed += 1
            continue

        # Format: "3[GM7]" → "Line 3 [GM7]", "3A[GM6]" → "Line 3A [GM6]"
        # Ensure exactly one space before [GM
        formatted = re.sub(r'\s*(\[GM)', r' \1', machine_name).strip()
        comment = f"Line {formatted}"

        # MQTT topic this device publishes to
        topic = f"iSoft/GoPak/LineCup/Info/{machine_name}"

        print(f"    machine_name : {machine_name}")
        print(f"    MQTT topic   : {topic}")
        print(f"    Comment      : {comment}")

        # Update comment by MAC (stable key)
        subprocess.run(
            [SCANNER, '-C', f'{mac}={comment}'],
            capture_output=True
        )
        print(f"    Saved to comments file")
        updated += 1

    except json.JSONDecodeError:
        print(f"    Could not parse {MQTT_FILE} — invalid JSON")
        failed += 1
    except subprocess.TimeoutExpired:
        print(f"    Timeout connecting to {ip}")
        failed += 1
    except Exception as e:
        print(f"    Error: {e}")
        failed += 1

    print()

print(f"Done: {updated} updated, {failed} failed.")
print(f"Run 'sudo ./gen-nginx.sh' or click Rescan on the dashboard to refresh.")
PYEOF
