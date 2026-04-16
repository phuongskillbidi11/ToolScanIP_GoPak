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
- **Table** — rendered from `/api/scan` JSON
- **SSH panel** — auto-generates sshpass command for Luckfox devices

### 2. Rebuild to embed changes

```bash
make
# scanner_html.h is regenerated, web.c recompiled, binary updated
```

### 3. Test locally

```bash
sudo ./build/ipscanner -i eth0 -w 8080
# Open http://localhost:8080/
```

### 4. Deploy to Pi

```bash
make deploy
# Then on Pi: sudo ./gen-nginx.sh
```

## HTTP routes (`src/web.c`)

| Method | Path | Handler | Description |
|--------|------|---------|-------------|
| GET | `/` | `route_root` | Serve embedded scanner.html |
| GET | `/api/scan` | `route_api_scan` | Current scan as JSON |
| GET | `/rescan` | `route_rescan` | Rescan network, return `ok` |
| GET | `/regen` | `route_regen` | Rescan + rebuild nginx + landing page |

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

Then rebuild and the route is live.

## Optional scripts (if any)

```bash
# Quick test without deploying — serve on a custom port
sudo ./build/ipscanner -i eth0 -w 9999
curl http://localhost:9999/api/scan | python3 -m json.tool
```

## Advanced examples / cross-references

- The `/regen` route also writes `/etc/nginx/conf.d/ipscan-proxy.conf` and
  `/var/www/ipscan/index.html` — see `skills/nginx-proxy/SKILL.md`
- `wstr(fd, s)` — write raw string to socket
- `wfmt(fd, fmt, ...)` — printf-style write to socket
- `json_str(fd, s)` — write a JSON-escaped quoted string
- See `skills/build/SKILL.md` for how scanner.html is embedded via xxd
