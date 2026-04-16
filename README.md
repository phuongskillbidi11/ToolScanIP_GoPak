# ToolScanIP

ARP-based network scanner with a terminal UI, web dashboard, and nginx reverse-proxy generator.
Built in C for Linux / Raspberry Pi. Designed for factory/IoT environments running Luckfox devices.

---

## What it does

| Mode | Command | Description |
|------|---------|-------------|
| Terminal scan | `sudo ./ipscanner -i eth0` | ARP-scan the subnet, display table, SSH into any host |
| Web UI | `sudo ./ipscanner -i eth0 -w 8080` | Live browser dashboard with rescan button |
| Save comment | `./ipscanner -C "192.168.20.26=Line 11 [GM11]"` | Attach a label to an IP or MAC |
| JSON export | `sudo ./ipscanner -i eth0 -j /tmp/out.json` | Dump results as JSON |
| nginx setup | `sudo ./gen-nginx.sh` | Scan + generate nginx proxies for all Luckfox devices |
| List interfaces | `./ipscanner -l` | Show available network interfaces |

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                        main.c                           │
│  CLI flags → route to: scan / web / comment / list      │
└────┬──────────────┬──────────────┬───────────────┬──────┘
     │              │              │               │
  arp.c          scanner.c     comments.c       web.c
  Raw ARP        Parallel       ~/.ipscanner     HTTP server
  broadcast      resolve        .comments        :PORT
  + receive      (pthreads)     file             │
     │              │                            │
  oui.c          probe.c                    scanner.html
  MAC→vendor     SSH/HTTP/mDNS              (embedded via
  lookup         banner grab                 xxd at build)
     │
  display.c
  ANSI table
  terminal UI
```

### Data flow

1. `arp.c` — sends ARP requests on the raw socket, collects replies into `ScanResult`
2. `scanner.c` — spawns one pthread per host, each calls:
   - `getnameinfo()` for hostname
   - `oui_lookup()` for MAC vendor
   - `probe_vendor()` if vendor is unknown/randomized
3. `comments.c` — loads `~/.ipscanner.comments`, applies labels to matching hosts
4. `display.c` or `web.c` — renders the result to terminal or HTTP

---

## File structure

```
ToolScanIP/
├── src/
│   ├── main.c          CLI entry point, argument parsing, SSH prompt
│   ├── arp.c / .h      ARP scanner (AF_PACKET raw socket)
│   ├── scanner.c / .h  Parallel host resolution (pthreads)
│   ├── oui.c / .h      MAC vendor lookup (nmap OUI database)
│   ├── probe.c / .h    Active probing: SSH banner, HTTP, mDNS, Telnet
│   ├── comments.c / .h Persistent host labels (read/write ~/.ipscanner.comments)
│   ├── display.c / .h  Terminal table rendering (ANSI + UTF-8 box-drawing)
│   ├── web.c / .h      Minimal HTTP server (serve HTML + JSON API)
│   └── scanner_html.h  ← AUTO-GENERATED from web/scanner.html (do not edit)
├── web/
│   ├── scanner.html    Web dashboard source (edit this, not scanner_html.h)
│   └── index.html      Reference copy of Luckfox device UI
├── scripts/
│   └── gen-nginx.sh    Scan + generate nginx proxy config for Luckfox devices
├── Makefile
├── .gitignore
└── README.md
```

> **Rule:** edit `web/scanner.html`. Never edit `src/scanner_html.h` — it is regenerated
> by `make` using `xxd -i web/scanner.html`.

---

## Key data structures (`src/scanner.h`)

```c
typedef struct {
    char    ip[16];          // "192.168.20.26"
    uint8_t mac[6];          // raw bytes
    char    mac_str[18];     // "FE:67:DE:21:6A:3F"
    char    hostname[128];   // reverse DNS or IP fallback
    char    vendor[64];      // OUI lookup or probe result
    char    comment[128];    // from ~/.ipscanner.comments
    int     online;          // 1 = ARP replied
} Host;

typedef struct {
    Host            hosts[1024];
    int             count;
    pthread_mutex_t lock;
} ScanResult;
```

---

## Web server routes (`src/web.c`)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/` | Serve embedded `scanner.html` |
| GET | `/api/scan` | Return current scan as JSON |
| GET | `/rescan` | Trigger new scan, return `200 ok` |

JSON response shape (`/api/scan`):
```json
{
  "iface": "eth0",
  "last_scan": "2026-04-14 10:30:00",
  "count": 22,
  "hosts": [
    {
      "ip": "192.168.20.26",
      "mac": "FE:67:DE:21:6A:3F",
      "hostname": "192.168.20.26",
      "vendor": "Randomized MAC (Privacy)",
      "comment": "Line 11 [GM11]",
      "online": true
    }
  ]
}
```

---

## Comments file (`~/.ipscanner.comments`)

Plain text, one entry per line:
```
# key = IP address or MAC address
192.168.20.26=Line 11 [GM11]
FE:67:DE:21:6A:3F=Line 11 [GM11]
```

- Keyed by **IP** (changes if device gets a new DHCP lease) or **MAC** (stable)
- MAC key takes priority when both exist
- Save via CLI: `./ipscanner -C "192.168.20.26=Line 11 [GM11]"`
- Does **not** require root

### Luckfox auto-detect rule

If a host's comment contains **both** `"Line"` and `"[GM"`, the terminal SSH prompt
automatically uses `sshpass -p luckfox` for one-click login.

---

## Build

### Requirements

```bash
sudo apt install gcc make xxd nmap  # native
sudo apt install gcc-arm-linux-gnueabihf  # Pi cross-compile
```

### Commands

```bash
make              # build build/ipscanner  (x86_64 / WSL)
make pi           # cross-compile → build/ipscanner-pi  (ARM)
make deploy       # make pi + scp to Pi
make clean        # remove build/
sudo make install # install to /usr/local/bin/ipscanner (setuid root)
```

### How HTML gets embedded

```
web/scanner.html
  → xxd -i → src/scanner_html.h   (byte array: scanner_html[], scanner_html_len)
  → #include'd by src/web.c
  → compiled into binary
```

When you edit `web/scanner.html`, just run `make` — the header is regenerated automatically.

---

## Deploy to Raspberry Pi

```bash
# 1. Cross-compile + upload
make deploy        # prompts for SSH password

# 2. On the Pi — generate nginx config + start scanner daemon
cd ~/ToolScanIP
sudo ./gen-nginx.sh
```

### What `gen-nginx.sh` does

1. Runs `ipscanner -j /tmp/ipscan_result.json` to scan
2. Writes `/etc/nginx/conf.d/ipscan-proxy.conf` — one server block per Luckfox device
   - Port formula: `9000 + last IP octet` (e.g., `.26` → `:9026`)
   - Proxies to `device_ip:8000` with WebSocket upgrade headers
3. Writes `/var/www/ipscan/index.html` — static landing page with clickable device table
4. Starts `ipscanner -w 8181` as background daemon (internal port)
5. Writes `/etc/nginx/conf.d/ipscan-scanner.conf` — proxies public `:8080` → `localhost:8181`
6. Reloads nginx

### Resulting URLs (Pi Tailscale IP: `100.66.44.107`)

| URL | What |
|-----|------|
| `http://100.66.44.107/` | Static device dashboard |
| `http://100.66.44.107:8080/` | Live scanner web UI |
| `http://100.66.44.107:9026/` | Luckfox device at 192.168.20.26 |
| `http://100.66.44.107:9029/` | Luckfox device at 192.168.20.29 |

> Re-run `sudo ./gen-nginx.sh` whenever devices are added/removed to refresh everything.

---

## Makefile Pi config

Edit the top of `Makefile` to match your Pi:

```makefile
PI_HOST = admin@100.66.44.107   # SSH target
PI_DIR  = ~/ToolScanIP          # destination directory on Pi
PI_CC   = arm-linux-gnueabihf-gcc  # ARM cross-compiler
```

---

## Extending the project

### Add a new web route

In `src/web.c`, add a handler function and register it in the dispatch block:

```c
static void route_mypage(int fd) {
    wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    wstr(fd, "hello\n");
}

// inside web_serve() dispatch:
else if (strcmp(path, "/mypage") == 0)
    route_mypage(fd);
```

### Add a new CLI flag

In `src/main.c`, add a `case` to the `getopt` loop and handle it before or after the root check
(no root needed for read-only or file operations).

### Add a new probe method

In `src/probe.c`, add a new probe inside `probe_vendor()`. Keep `PROBE_TIMEOUT_MS` (300ms)
to avoid slowing down parallel resolution.

### Change the web UI

Edit `web/scanner.html` freely — it is plain HTML/CSS/JS. Run `make` to rebuild.
The file is served from memory; no web server config needed.

---

## Dependencies (runtime)

| Dependency | Used for | Required |
|------------|----------|----------|
| `/usr/share/nmap/nmap-mac-prefixes` | OUI vendor lookup | Optional (falls back to probe) |
| `sshpass` | Auto SSH login to Luckfox | Optional |
| `nginx` | Reverse proxy (Pi only) | Pi deploy only |

---

## License

I-Soft Tran Hoang Phuong Embedded IIOT
