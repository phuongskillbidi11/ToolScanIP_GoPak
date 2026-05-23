---
skill: web-ui
last_updated: 2026-05-22
updated_after_sprint: 0
confidence: low
status: draft
note: "Update confidence and status after first sprint using this skill."
---

# Skill: web-ui

## Purpose
Edit the browser dashboard (`web/scanner.html`), understand how it gets embedded
into the binary, and add or modify HTTP routes in `src/web.c`.

## When to use
- Changing the look or behaviour of the web dashboard
- Adding a new API endpoint or HTTP route
- Debugging what the browser receives from the scanner

## Files involved

| File | Role |
|------|------|
| `web/scanner.html` | Dashboard source — **edit this** |
| `src/scanner_html.h` | Auto-generated C byte array — never edit |
| `src/web.c` | HTTP server: routes, JSON builder, nginx helpers |
| `src/web.h` | Public interface (`web_serve`) |

## Step-by-step instructions

### 1. Edit the dashboard

Open `web/scanner.html` in any editor. It is plain HTML + CSS + JS — no framework.

Key sections:
- **Header bar** — title, interface badge, Rescan button
- **Stats cards** — Total Hosts, Online, Luckfox Nodes, Last Scan
- **Host table** — rendered from `/api/scan` JSON
- **SSH panel** — auto-generates sshpass command for Luckfox devices
- **Multi SCP panel** — dual-panel file transfer (Source Device + Target Devices)

### 2. Rebuild to embed changes

```bash
make pi
# scanner_html.h is regenerated, web.c cross-compiled for Pi
```

### 3. Deploy to Pi

```bash
make deploy
# Then on Pi: sudo ./scripts/gen-nginx.sh eth0
```

### 4. Test locally (x86)

```bash
sudo ./build/ipscanner -i eth0 -w 8080
# Open http://localhost:8080/
```

## HTTP routes (`src/web.c`)

| Method | Path | Handler | Description |
|--------|------|---------|-------------|
| GET | `/` | `route_root` | Serve embedded scanner.html |
| GET | `/api/scan` | `route_api_scan` | Current scan as JSON |
| GET | `/api/browse` | `route_api_browse` | List files on a device (`ip=` + `path=`) |
| GET | `/rescan` | `route_rescan` | Rescan network, return `ok` |
| GET | `/regen` | `route_regen` | Rescan + rebuild nginx config + landing page |
| POST | `/api/comment` | `route_api_comment` | Save/delete a MAC or IP comment |
| POST | `/api/multi-scp` | `route_api_multi_scp` | SCP files from source to N targets |
| POST | `/api/mkdir` | `route_api_mkdir` | Create directory on a device |
| POST | `/api/rm` | `route_api_rm` | Remove file/directory on a device |
| POST | `/api/rename` | `route_api_rename` | Rename file on a device |

### `/api/browse` special values

- `ip=<dotted-quad>` — SSH into the device and run `ls -la <path>`
- `ip=LOCAL` — run `ls -la <path>` directly on the Pi (no SSH); used by the
  "Local Server (Raspberry Pi)" option in the Multi SCP source dropdown

### JSON shape (`/api/scan`)

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

### JSON shape (`/api/browse`)

```json
{
  "path": "/root",
  "entries": [
    {
      "name": "firmware.bin",
      "type": "f",
      "size": 4096,
      "perms": "-rw-r--r--",
      "owner": "root",
      "date": "Apr 20 11:32"
    },
    { "name": "logs", "type": "d", ... }
  ]
}
```

### Adding a new route

In `src/web.c`:

```c
// 1. Add handler function
static void route_status(int fd) {
    wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    wfmt(fd, "uptime: %s\n", g_last_scan);
}

// 2. Register in the dispatch block inside web_serve()
else if (strcmp(path, "/status") == 0)
    route_status(fd);
```

Then `make pi && make deploy`.

## Multi SCP panel (scanner.html)

Key JS variables:
- `scpSourceIP` — IP of selected source, or `"LOCAL"` for the Pi itself
- `scpPath` — current source browse path
- `scpSelected` — `Set` of absolute paths checked in the source file list

Key JS functions:

| Function | What it does |
|----------|-------------|
| `scpPopulateSources()` | Fill source dropdown (LOCAL + all hosts + custom) |
| `scpPopulateTargets()` | Fill target checkbox list from Luckfox hosts |
| `onSourceChange()` | Handle source dropdown change, kick off browse |
| `scpBrowse(path)` | Fetch `/api/browse?ip=…&path=…`, render file list |
| `scpRefreshTargetBrowsers()` | Re-browse all checked targets at current dest path |
| `deployMultiSCP()` | POST to `/api/multi-scp` with source files + target IPs |
| `scpSrcMkdir/Rm/Rename()` | File ops on source via POST endpoints |

## Optional scripts

```bash
# Quick test without deploying
sudo ./build/ipscanner -i eth0 -w 9999
curl http://localhost:9999/api/scan | python3 -m json.tool
curl 'http://localhost:9999/api/browse?ip=LOCAL&path=/root'
```

## Cross-references

- `/regen` also writes `/etc/nginx/conf.d/ipscan-proxy.conf` and
  `/var/www/ipscan/index.html` — see `skills/nginx-proxy/SKILL.md`
- `wstr(fd, s)` — write raw string to socket
- `wfmt(fd, fmt, ...)` — printf-style write to socket
- `json_str(fd, s)` — write a JSON-escaped quoted string
- See `skills/build/SKILL.md` for how scanner.html is embedded via xxd
- See `skills/multi-scp/SKILL.md` for the full SCP transfer flow
