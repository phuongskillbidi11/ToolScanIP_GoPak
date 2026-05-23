---
skill: probe
last_updated: 2026-05-22
updated_after_sprint: 0
confidence: low
status: draft
note: "Update confidence and status after first sprint using this skill."
---

# Skill: probe

## Purpose
Active probing identifies device vendors when the OUI MAC database returns
"Unknown" or "Randomized MAC (Privacy)". Probes open TCP/UDP connections to
common ports and inspect the response banners.

## When to use
- Devices show "Unknown" vendor and you want better identification
- Adding a new probe type (e.g., MQTT, Modbus, custom port)
- Tuning probe timeout to trade speed vs accuracy
- Debugging why a device isn't being identified correctly

## Files involved

| File | Role |
|------|------|
| `src/probe.c` | All probe implementations |
| `src/probe.h` | Public interface: `probe_vendor(Host *h)` |
| `src/scanner.c` | Calls `probe_vendor()` per host in parallel pthreads |

## Step-by-step instructions

### 1. Understand when probing runs

In `src/scanner.c`, each host pthread runs:

```c
oui_lookup(h->mac, h->vendor, VENDOR_LEN);
if (strcmp(h->vendor, "Unknown") == 0 ||
    strncmp(h->vendor, "Randomized", 10) == 0)
    probe_vendor(h);   // ← active probe only if OUI lookup failed
```

### 2. Current probes (in order)

| Probe | Port | Protocol | What it looks for |
|-------|------|----------|-------------------|
| SSH banner | 22 | TCP | `SSH-` prefix → vendor keyword table |
| HTTP | 80 | TCP | `Server:` header or body keywords |
| Telnet | 23 | TCP | Banner keywords |
| mDNS | 5353 | UDP | `_luckfox` service record |

### 3. Probe timeout

```c
#define PROBE_TIMEOUT_MS 300   // src/probe.c — 300ms per probe
```

Increase for slower devices, decrease for faster scans. Too low = missed
detections; too high = slow scans (multiplied by number of unknown hosts).

### 4. Adding a new probe

In `src/probe.c`, add a function and call it from `probe_vendor()`:

```c
static int probe_modbus(Host *h)
{
    // Modbus TCP port 502
    int fd = tcp_connect(h->ip, 502, PROBE_TIMEOUT_MS);
    if (fd < 0) return 0;

    // Send Modbus device identification request
    uint8_t req[] = {0x00,0x01,0x00,0x00,0x00,0x06,
                     0x01,0x2B,0x0E,0x01,0x00};
    send(fd, req, sizeof(req), 0);

    char buf[256] = {0};
    recv(fd, buf, sizeof(buf)-1, 0);
    close(fd);

    if (buf[7] == 0x2B) {   // valid Modbus response
        snprintf(h->vendor, VENDOR_LEN, "Modbus Device");
        return 1;
    }
    return 0;
}

// Inside probe_vendor():
void probe_vendor(Host *h)
{
    if (probe_ssh(h))    return;
    if (probe_http(h))   return;
    if (probe_telnet(h)) return;
    if (probe_mdns(h))   return;
    if (probe_modbus(h)) return;   // ← add here
}
```

### 5. Keyword table (SSH / HTTP / Telnet)

In `src/probe.c` there is a keyword→vendor mapping table:

```c
static const struct { const char *kw; const char *vendor; } kw_table[] = {
    { "luckfox",    "Luckfox"            },
    { "OpenWrt",    "OpenWrt Router"     },
    { "Raspberry",  "Raspberry Pi"       },
    { "nginx",      "nginx Web Server"   },
    // add new entries here
    { NULL, NULL }
};
```

## Optional scripts (if any)

```bash
# Manually test SSH banner of a host
nc -w1 192.168.20.26 22

# Manually test HTTP response
curl -s --max-time 1 http://192.168.20.26/ | head -5

# Check mDNS services on the network
avahi-browse -a -t 2>/dev/null | head -20
```

## Advanced examples / cross-references

- Probes run in parallel (one pthread per host) — no locking needed as each
  probe writes only to its own `Host *h`
- `tcp_connect(ip, port, timeout_ms)` is a helper in `probe.c` that uses
  `select()` for non-blocking connect with timeout
- See `skills/arp-scan/SKILL.md` for the `Host` struct and parallel resolution
- See `skills/build/SKILL.md` to rebuild after changes
