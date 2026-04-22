#!/bin/bash
# mqtt-sync-comments.sh
# Subscribe to iSoft/GoPak/LineCup/Info/+ on the MQTT broker,
# parse LocalIP + machine_name from each message,
# cross-reference with ARP scan to get MAC, then update comments.
#
# Usage: sudo ./scripts/mqtt-sync-comments.sh [interface] [wait_seconds]
#
# Requirements: mosquitto-clients  (sudo apt install mosquitto-clients)

SCANNER="$(cd "$(dirname "$0")" && pwd)/../ipscanner"
[ -x "$SCANNER" ] || SCANNER="$(cd "$(dirname "$0")" && pwd)/../build/ipscanner"

IFACE="${1:-eth0}"
WAIT="${2:-30}"          # seconds to listen for MQTT messages
JSON_TMP="/tmp/ipscan_result.json"

# MQTT map file lives next to the comments file in the real user's home
_REAL_HOME="$(eval echo ~${SUDO_USER:-$USER})"
MQTT_MAP="${_REAL_HOME}/.ipscanner.mqttmap"

# ── MQTT broker credentials ───────────────────────────────────────────────────
MQTT_HOST="vm01.i-soft.com.vn"
MQTT_PORT="11883"
MQTT_USER="mqtt-user-01"
MQTT_PASS="MtTP5WBlYy3CSaRL9c_s4VFQ"
MQTT_TOPIC="iSoft/GoPak/LineCup/Info/+"

# ── Check dependencies ────────────────────────────────────────────────────────
if ! command -v mosquitto_sub &>/dev/null; then
    echo "Error: mosquitto_sub not found."
    echo "Install with:  sudo apt install mosquitto-clients"
    exit 1
fi

# ── 1. ARP scan to build IP → MAC mapping ────────────────────────────────────
echo "[1/3] Scanning $IFACE for IP→MAC mapping ..."
"$SCANNER" -i "$IFACE" -j "$JSON_TMP"
echo ""

# ── 2. Subscribe to MQTT and collect messages ─────────────────────────────────
echo "[2/3] Listening on MQTT $MQTT_TOPIC for ${WAIT}s ..."
echo "      (broker: $MQTT_HOST:$MQTT_PORT)"
echo ""

MQTT_LOG="/tmp/mqtt_info_dump.txt"

# mosquitto_sub -F "%t\t%p" prints: topic<TAB>payload per line
# -W sets idle timeout (seconds without a message before exit)
# -C with a large number keeps collecting; use timeout command to stop
timeout "$WAIT" mosquitto_sub \
    -h "$MQTT_HOST" \
    -p "$MQTT_PORT" \
    -u "$MQTT_USER" \
    -P "$MQTT_PASS" \
    -t "$MQTT_TOPIC" \
    -F "%t\t%p" \
    2>/dev/null > "$MQTT_LOG"

LINE_COUNT=$(wc -l < "$MQTT_LOG")
echo "  Received $LINE_COUNT message(s)."
echo ""

if [ "$LINE_COUNT" -eq 0 ]; then
    echo "  No messages received. Check MQTT broker connectivity."
    echo "  Test with:"
    echo "    mosquitto_sub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS -t '$MQTT_TOPIC' -C 1"
    exit 1
fi

# ── 3. Parse messages and update comments ─────────────────────────────────────
echo "[3/3] Updating comments ..."
echo ""

python3 << PYEOF
import json, re, subprocess, sys

SCANNER  = "$SCANNER"
ARP_JSON = "$JSON_TMP"
MQTT_LOG = "$MQTT_LOG"
MQTT_MAP = "$MQTT_MAP"

# Load ARP scan: build ip → mac dict
with open(ARP_JSON) as f:
    hosts = json.load(f)

ip_to_mac = {h['ip']: h['mac'] for h in hosts}

# Parse MQTT log: each line is "topic<TAB>payload"
updates = {}   # machine_name → {ip, mac, comment}

with open(MQTT_LOG) as f:
    for line in f:
        line = line.strip()
        if '\t' not in line:
            continue
        topic, payload = line.split('\t', 1)

        machine_name = topic.split('/')[-1].strip()
        if not machine_name:
            continue

        # Parse LocalIP from payload
        # Format: {"type":"Info","data":"LocalIP: 192.168.20.12, ..."}
        try:
            data = json.loads(payload)
            info_str = data.get('data', '')
        except json.JSONDecodeError:
            info_str = payload

        m = re.search(r'LocalIP:\s*([\d.]+)', info_str)
        if not m:
            continue
        local_ip = m.group(1).strip()

        # Format comment: "5[GM10]" → "Line 5 [GM10]"
        formatted = re.sub(r'\s*(\[GM)', r' \1', machine_name).strip()
        comment   = f"Line {formatted}"

        mac = ip_to_mac.get(local_ip)

        # Keep the most recent entry per machine name
        updates[machine_name] = {
            'ip':      local_ip,
            'mac':     mac,
            'comment': comment,
            'topic':   topic,
        }

if not updates:
    print("  No LocalIP data found in MQTT messages.")
    sys.exit(0)

updated_mac = 0
skipped     = 0

for mn, info in sorted(updates.items()):
    ip      = info['ip']
    mac     = info['mac']
    comment = info['comment']
    topic   = info['topic']

    print(f"  {ip}  →  {comment}")
    print(f"    MQTT topic : {topic}")

    if mac:
        print(f"    MAC        : {mac}")
        print(f"    Writing    : {mac}={comment}  →  {MQTT_MAP}")
        # Write to mqttmap file (not the manual comments file)
        subprocess.run([SCANNER, '-f', MQTT_MAP, '-C', f'{mac}={comment}'],
                       capture_output=True)
        updated_mac += 1
    else:
        # Device not seen in ARP scan — skip rather than creating an IP fallback.
        # Register MAC placeholder so the next sync can fill it in.
        print(f"    MAC        : not in ARP scan — device offline, skipping")
        skipped += 1

    print()

print(f"Done: {updated_mac} saved to {MQTT_MAP}, {skipped} skipped (offline).")
print(f"Manual labels in ~/.ipscanner.comments always override MQTT labels.")
print(f"Run 'sudo ./gen-nginx.sh' or click Rescan to refresh the dashboard.")
PYEOF
