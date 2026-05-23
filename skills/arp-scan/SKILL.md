---
skill: arp-scan
last_updated: 2026-05-22
updated_after_sprint: 0
confidence: low
status: draft
note: "Update confidence and status after first sprint using this skill."
---

# Skill: arp-scan

## Purpose
Understand the raw ARP discovery engine — how it broadcasts ARP requests,
collects replies, resolves hostnames in parallel, and looks up MAC vendors
from the OUI database.

## When to use
- Debugging why a host isn't being discovered
- Extending the scanner to support a different subnet or interface
- Understanding the `ScanResult` / `Host` data structures
- Adding a new field to each discovered host

## Files involved

| File | Role |
|------|------|
| `src/arp.c / .h` | Raw ARP broadcast + receive (AF_PACKET) |
| `src/scanner.c / .h` | Orchestrates scan: calls arp, resolves in parallel |
| `src/oui.c / .h` | MAC → vendor name lookup (nmap OUI database) |
| `src/probe.c / .h` | Active probing fallback for unknown vendors |
| `src/display.c / .h` | Terminal table renderer |

## Key data structures (`src/scanner.h`)

```c
typedef struct {
    char    ip[16];          // "192.168.20.26"
    uint8_t mac[6];          // raw 6-byte MAC
    char    mac_str[18];     // "FE:67:DE:21:6A:3F"
    char    hostname[128];   // reverse DNS or IP fallback
    char    vendor[64];      // OUI lookup or probe result
    char    comment[128];    // from ~/.ipscanner.comments
    int     online;          // 1 = ARP replied
} Host;

typedef struct {
    Host            hosts[1024];  // up to 1024 hosts
    int             count;
    pthread_mutex_t lock;         // protects count during ARP receive
} ScanResult;
```

## Step-by-step instructions

### How a scan works

```
scan_network(iface, result)
  │
  ├── arp_get_local_ip(iface)   → get our IP + subnet (e.g. 192.168.20.0/24)
  ├── open AF_PACKET raw socket
  ├── for each IP in subnet:
  │       send ARP request (who has X? tell our IP)
  └── receive loop (1 second timeout):
          for each ARP reply:
              add Host to result (IP + MAC)
              result->count++

resolve_hosts(result)
  └── for each host: spawn pthread
          getnameinfo()   → hostname
          oui_lookup()    → vendor
          probe_vendor()  → if vendor still unknown
```

### 1. Run a scan manually

```bash
sudo ./build/ipscanner -i eth0
```

### 2. Check which interface to use

```bash
./build/ipscanner -l
# Lists all non-loopback interfaces
```

### 3. Export scan results as JSON

```bash
sudo ./build/ipscanner -i eth0 -j /tmp/scan.json
cat /tmp/scan.json | python3 -m json.tool
```

### 4. OUI database location

```
/usr/share/nmap/nmap-mac-prefixes
```

Format: `AABBCC VendorName` (first 3 bytes of MAC, no colons, uppercase).

If the file is missing, install nmap:
```bash
sudo apt install nmap
```

### 5. Adding a new field to Host

1. Add the field to `Host` struct in `src/scanner.h`
2. Populate it in `src/scanner.c` (inside `resolve_one()` pthread)
3. Output it in `src/display.c` (terminal) and `src/web.c` (JSON)
4. Increase `MAX_HOSTS` if needed (currently 1024)

### 6. Changing scan timeout or subnet

In `src/arp.c`, look for the receive timeout and subnet derivation:

```c
// Timeout for ARP reply collection
struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

// Subnet is derived from interface IP + netmask automatically
```

## Optional scripts (if any)

```bash
# Verify ARP is working at OS level
arping -I eth0 192.168.20.1

# See raw ARP traffic
sudo tcpdump -i eth0 arp

# Check nmap OUI database
grep "^AABBCC" /usr/share/nmap/nmap-mac-prefixes
```

## Advanced examples / cross-references

- Thread safety: `result->lock` (pthread_mutex) protects `count` during
  concurrent ARP reply processing in `arp.c`
- `resolve_hosts()` in `scanner.c` spawns N threads (one per host), joins all
  before returning — scan time ≈ max(single probe) not sum
- See `skills/probe/SKILL.md` to add new vendor detection probes
- See `skills/comments/SKILL.md` to understand how labels are attached after scanning
