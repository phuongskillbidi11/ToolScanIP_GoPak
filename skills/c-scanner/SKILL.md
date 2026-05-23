---
skill: c-scanner
last_updated: 2026-05-22
updated_after_sprint: 0
confidence: high
status: active
note: "Core C scanner codebase. Update after any src/*.c change."
---

# Skill: c-scanner

## Purpose

Core C scanner codebase — ARP scanning, OUI lookup, active probing, web HTTP server,
and persistent comments. Load this skill for any task touching `src/*.c` or the Makefile.

## When to use

- Adding or changing C source files in `src/`
- Changing build targets in `Makefile`
- Modifying or adding HTTP routes in `src/web.c`
- Debugging scanner logic (ARP, OUI, probing)

## Files involved

| File | Role |
|------|------|
| `src/main.c` | Entry point — CLI args (`-i`, `-j`, `-w`), scan loop, web thread |
| `src/arp.c` | Raw AF_PACKET ARP sender + receiver, result dedup |
| `src/oui.c` | MAC → vendor string lookup (embedded OUI table) |
| `src/probe.c` | SSH banner, HTTP title, mDNS active probes |
| `src/scanner.c` | Orchestrates ARP + probe + display, holds global host list |
| `src/display.c` | Terminal table renderer |
| `src/comments.c` | Read/write `~/.ipscanner.comments` (key=value, MAC or IP keyed) |
| `src/web.c` | Embedded HTTP server: all routes, JSON builder |
| `src/web.h` | Public interface: `web_serve(iface, port, scanner)` |
| `src/scanner_html.h` | Auto-generated C byte array — **never edit** |
| `Makefile` | All build + deploy targets |

## Build system

```bash
make              # native x86_64 → ./ipscanner
make pi           # armhf → ./ipscanner-pi  (then: make deploy)
make aarch64      # aarch64 → ./ipscanner-aarch64 (then: make deploy3)
make build2       # make pi + make deploy2 in one step
```

HTML is embedded via:
```bash
python3 web/build.py          # web/src/ → web/scanner.html
xxd -i web/scanner.html | sed 's/web_scanner_html/scanner_html/g' > src/scanner_html.h
```
This happens automatically when `make` detects `web/src/` files are newer.

## Cross-compile targets

| Target | Compiler | GLIBC | Board |
|--------|----------|-------|-------|
| x86_64 | gcc (native) | 2.35+ | WSL2/Ubuntu |
| armhf | arm-linux-gnueabihf-gcc | 2.31 | Raspberry Pi 1 & 2 |
| aarch64 | aarch64-linux-gnu-gcc | 2.31 | Orange Pi (Rockchip64) |

**aarch64 pthread:** Link statically (`-Wl,-Bstatic -lpthread -Wl,-Bdynamic`) because
Ubuntu 22.04 toolchain targets GLIBC 2.34 where pthread merged into libc.so.6,
but the Orange Pi has GLIBC 2.31.

## HTTP routes (`src/web.c`)

| Method | Path | Handler |
|--------|------|---------|
| GET | `/` | `route_root` — serve embedded scanner.html |
| GET | `/api/scan` | `route_api_scan` — current hosts as JSON |
| GET | `/api/browse` | `route_api_browse` — `ls -la` a path on a device |
| GET | `/rescan` | `route_rescan` |
| GET | `/regen` | `route_regen` — rescan + rebuild nginx + landing page |
| POST | `/api/comment` | `route_api_comment` — save/delete label |
| POST | `/api/multi-scp` | `route_api_multi_scp` — SCP to N targets |
| POST | `/api/mkdir` | `route_api_mkdir` |
| POST | `/api/rm` | `route_api_rm` |
| POST | `/api/rename` | `route_api_rename` |
| POST | `/api/chmod` | `route_api_chmod` — chmod with optional -R |

Adding a new route:
```c
// 1. Handler function in src/web.c
static void route_foo(int fd, const char *req) {
    wstr(fd, "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\n");
    wfmt(fd, "result\n");
}

// 2. Dispatch in web_serve() dispatch block
else if (strcmp(path, "/foo") == 0)
    route_foo(fd, req);
```

## Constraints and gotchas

- `ip=LOCAL` in `/api/browse` and file ops = run directly on Pi via `popen()`, not SSH
- BusyBox `ls` (Luckfox remote) does NOT support `--time-style`; use plain `ls -la`
- Auto-detect date format in browse parser: token[5] matches `YYYY-MM-DD` → GNU; else BusyBox 9-token format
- `wstr(fd, s)` / `wfmt(fd, fmt, ...)` / `json_str(fd, s)` are the only write helpers — use these, not raw write()
- Never edit `src/scanner_html.h` — it is overwritten by `make`

## Step-by-step: adding a new route

1. Write handler `static void route_X(int fd, const char *req)` in `src/web.c`
2. Add `else if (strcmp(path, "/X") == 0) route_X(fd, req);` in the dispatch block
3. If route needs a UI button: add it in `web/src/assets/js/scp.js` or `hosts.js`
4. `python3 web/build.py` to rebuild `web/scanner.html`
5. `make pi` to cross-compile (triggers scanner_html.h regen)
6. `make deploy` to push to Pi

## Tests and verification

```bash
# Build test (native)
make

# Check HTML is embedded
ls -la src/scanner_html.h

# Test route locally
sudo ./ipscanner -i eth0 -w 9999 &
curl http://localhost:9999/api/scan | python3 -m json.tool
curl 'http://localhost:9999/api/browse?ip=LOCAL&path=/root'
kill %1
```

**Pass:** `make` exits 0; curl returns valid JSON
**Fail:** Compile error → paste to Planner; curl returns "Not found" → route not registered
