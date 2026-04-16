# Skill: comments

## Purpose
Attach persistent human-readable labels to network hosts. Labels are stored in
`~/.ipscanner.comments` keyed by IP or MAC address. MAC keys are preferred because
they survive DHCP IP changes after a router reset.

## When to use
- Labelling a new Luckfox device on the network
- Correcting a label after a device gets a new IP
- Converting old IP-keyed entries to MAC-keyed entries
- Troubleshooting missing labels after a router reset

## Files involved

| File | Role |
|------|------|
| `~/.ipscanner.comments` | Persistent label store (plain text) |
| `src/comments.c / .h` | Load, save, apply, free comment entries |
| `src/main.c` | `-C` flag handling (no root needed) |

## Step-by-step instructions

### 1. Save a comment (no sudo required)

```bash
# By IP (simple, but breaks if IP changes)
./ipscanner -C "192.168.20.26=Line 11 [GM11]"

# By MAC (recommended — survives router resets)
./ipscanner -C "FE:67:DE:21:6A:3F=Line 11 [GM11]"
```

### 2. View all saved comments

```bash
cat ~/.ipscanner.comments
```

### 3. Find the MAC address of a device

```bash
sudo ./ipscanner -i eth0
# Copy the MAC from the table for the device you want to label
```

### 4. Convert all IP entries to MAC entries (bulk)

```bash
# Delete all IP-based lines (lines starting with digits)
sed -i '/^[0-9]*\./d; /^IP=/d' ~/.ipscanner.comments

# Re-add each device by MAC
./ipscanner -C "FE:67:DE:21:6A:3F=Line 11 [GM11]"
./ipscanner -C "82:D8:59:07:70:DF=Line 3 [GM7]"
# ... repeat for each device
```

### 5. Use a custom comments file

```bash
sudo ./ipscanner -i eth0 -f /path/to/custom.comments
```

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
- Leading/trailing spaces around `=` are included in the label — avoid them

## Luckfox auto-login rule

If a comment contains **both** the word `Line` **and** `[GM`, the terminal SSH
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

## Optional scripts (if any)

```bash
# Backup before bulk edits
cp ~/.ipscanner.comments ~/.ipscanner.comments.bak

# View only MAC-keyed entries
grep "^[0-9A-Fa-f][0-9A-Fa-f]:" ~/.ipscanner.comments

# Count entries
wc -l ~/.ipscanner.comments
```

## Advanced examples / cross-references

- Comments are loaded at scan time and applied after ARP resolution
- `comments_apply()` in `src/comments.c` matches by MAC first, then IP
- The web `/regen` route reloads comments on each scan — no restart needed
- See `skills/arp-scan/SKILL.md` for how comments are applied to `ScanResult`
