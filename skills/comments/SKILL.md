# Skill: comments

## Purpose
Attach persistent human-readable labels to network hosts.
Labels are stored in two separate files with distinct priorities:

| File | Source | Priority |
|------|--------|----------|
| `~/.ipscanner.comments` | Manual (terminal / web UI) | **High** — always wins |
| `~/.ipscanner.mqttmap`  | MQTT auto-sync script      | Low — shown only if no manual entry |

MAC keys are preferred because they survive DHCP IP changes after a router reset.

## When to use
- Labelling a new Luckfox device on the network
- Correcting a label after a device gets a new IP
- Running MQTT sync to auto-populate labels from device broadcasts
- Pinning or overriding an MQTT label with a custom manual one
- Troubleshooting missing or conflicting labels

## Files involved

| File | Role |
|------|------|
| `~/.ipscanner.comments` | Manual label store (plain text) |
| `~/.ipscanner.mqttmap`  | MQTT auto-sync label store (plain text) |
| `src/comments.c / .h`   | Load, save, apply, delete, free comment entries |
| `src/main.c`            | `-C` flag handling (no root needed) |
| `src/web.c`             | `/api/comment` save + delete routes |
| `scripts/mqtt-sync-comments.sh` | MQTT listener that fills `.mqttmap` |

---

## Adding labels manually

### Terminal
```bash
# By MAC (recommended — survives router resets)
./ipscanner -C "FE:67:DE:21:6A:3F=Line 11 [GM11]"

# By IP (simple, but breaks if IP changes)
./ipscanner -C "192.168.20.26=Line 11 [GM11]"
```
Writes to `~/.ipscanner.comments`.

### Web UI
Click the **pen icon** on any row → type label → **Save**.  
Always writes to `~/.ipscanner.comments`.

---

## Adding labels via MQTT (automatic)

```bash
sudo ./scripts/mqtt-sync-comments.sh eth0 30
```
- ARP scans the network → listens to MQTT topic `iSoft/GoPak/LineCup/Info/+` for 30 s
- Parses `LocalIP` + machine name from each message
- Writes `MAC=Line N [GMxx]` entries to `~/.ipscanner.mqttmap` **only**
- Never touches `~/.ipscanner.comments`

### Registering a new MAC for MQTT to fill later
```bash
./ipscanner -C "3A:BC:E0:01:C9:EF"   # no = sign
```
Appends `MAC=` (empty value) to `~/.ipscanner.mqttmap`.  
The next MQTT sync run fills in the label automatically.

---

## Conflict rules — how sources are merged

On every scan both files are loaded in order:
1. `~/.ipscanner.mqttmap` — source tag `mqtt`
2. `~/.ipscanner.comments` — source tag `manual`

Manual entries are loaded last, so they always override MQTT entries for the same MAC.

| Situation | Web badge | Edit/Delete options |
|-----------|-----------|---------------------|
| MAC only in `.mqttmap` | `[MQTT]` green | Read-only. **Pin** button promotes to manual |
| MAC only in `.comments` | `[Manual]` blue | Edit + Delete (from `.comments`) |
| MAC in both files | `[Manual↑]` orange | **Unpin** reverts to MQTT; **Delete** removes manual entry |

**Delete behavior:**
- `[Manual]` → deletes from `.ipscanner.comments`
- `[MQTT]` → no delete (read-only); use **Pin** to create a manual override
- `[Manual↑]` → **Unpin** removes from `.ipscanner.comments` only (MQTT label is preserved)

---

## View saved labels

```bash
cat ~/.ipscanner.comments    # manual entries
cat ~/.ipscanner.mqttmap     # MQTT auto entries
```

## Find the MAC address of a device

```bash
sudo ./ipscanner -i eth0
# Copy the MAC from the table for the device you want to label
```

## Use a custom comments file

```bash
sudo ./ipscanner -i eth0 -f /path/to/custom.comments
# mqttmap is derived automatically: /path/to/custom.mqttmap
```

---

## File format

```
# Lines starting with # are ignored
# Format: key=label
# key = IP address OR MAC address (MAC takes priority if both exist)

FE:67:DE:21:6A:3F=Line 11 [GM11]
82:D8:59:07:70:DF=Line 3 [GM7]
192.168.20.1=Router
```

**Rules:**
- One entry per line
- MAC format: `AA:BB:CC:DD:EE:FF` (uppercase, colon-separated)
- If both IP and MAC entries exist for the same host, MAC wins
- Avoid leading/trailing spaces around `=`

---

## Luckfox auto-login rule

If a comment contains **both** `Line` **and** `[GM`, the terminal SSH
prompt automatically uses `sshpass -p luckfox`:

```
FE:67:DE:21:6A:3F=Line 11 [GM11]   ← auto SSH with password "luckfox"
192.168.20.21=Dell workstation       ← normal SSH prompt
```

Detection logic in `src/main.c`:
```c
if (strstr(host->comment, "Line") && strstr(host->comment, "[GM"))
    pass = "luckfox";
```

---

## Useful maintenance commands

```bash
# Backup before bulk edits
cp ~/.ipscanner.comments ~/.ipscanner.comments.bak
cp ~/.ipscanner.mqttmap  ~/.ipscanner.mqttmap.bak

# View only MAC-keyed entries
grep "^[0-9A-Fa-f][0-9A-Fa-f]:" ~/.ipscanner.comments

# Count entries
wc -l ~/.ipscanner.comments ~/.ipscanner.mqttmap

# Remove duplicate keys (keep last occurrence)
python3 -c "
lines=open('$HOME/.ipscanner.comments').readlines()
seen={}
for l in lines:
    k=l.split('=')[0].strip().upper()
    if k: seen[k]=l
open('$HOME/.ipscanner.comments','w').writelines(seen.values())
"
```

## Cross-references

- `comments_apply()` in `src/comments.c` matches by MAC first, then IP
- The web `/api/scan` JSON includes a `csrc` field (`"mqtt"`, `"manual"`, `"override"`, `"none"`) per host
- The web UI uses `csrc` to render source badges and conditional action buttons
- See `skills/arp-scan/SKILL.md` for how comments are applied to `ScanResult`
