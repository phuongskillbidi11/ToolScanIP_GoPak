# ToolScanIP

ARP-based network scanner with terminal UI, web dashboard, and nginx reverse-proxy.
Built in C for Linux / Raspberry Pi / Orange Pi. Designed for factory/IoT environments running Luckfox devices.

**Author:** I-Soft Tran Hoang Phuong — Embedded IIoT

---

## Features

- **ARP scanner** — discovers all hosts on a LAN, OUI vendor lookup, SSH/HTTP/mDNS probing
- **Web dashboard** — host list, comments, file browser, multi-SCP transfer, system monitor
- **SSH Terminal** — multi-tab terminal panel: open sessions to multiple devices simultaneously, each with independent output history
- **System Monitor** — live CPU/RAM/temp charts and process table (Chart.js, 3 s refresh)
- **nginx reverse-proxy** — auto-generated per-device config via `gen-nginx.sh`
- **Persistent comments** — label hosts by MAC or IP, survives reboots

---

## Quick start

```bash
# Build native (x86_64 / WSL2)
make
sudo ./ipscanner -i eth0 -w 8080

# Build for Raspberry Pi (armhf)
make pi && make deploy

# Build for Orange Pi (aarch64 — compile natively on board)
make native3

# Rebuild web dashboard after editing web/src/
python3 web/build.py && make
```

---

## Web dashboard

| Panel | What it does |
|-------|-------------|
| Host List | ARP scan results — IP, MAC, vendor, hostname, SSH probe status |
| SSH Terminal | Multi-tab terminal — connect to any scanned host, run commands |
| SCP Transfer | Multi-device file push/pull with chmod modal |
| System Monitor | Live CPU %, RAM, disk, temperature charts + process table |

---

## Deploy targets

| Target | Command | Board | Tailscale |
|--------|---------|-------|-----------|
| Pi 1 (armhf) | `make deploy` | Raspberry Pi — admin@100.66.44.107 | SSH key required |
| Pi 2 (armhf) | `make deploy2` | Raspberry Pi — intercom@100.70.31.100 | sshpass |
| Orange Pi (aarch64) | `make native3` | root@100.101.117.23 | compile on-board |

After deploy, start the full stack:
```bash
cd ~/ToolScanIP && sudo ./gen-nginx.sh wlan0   # Pi 2
cd ~/ToolScanIP && sudo ./gen-nginx.sh lan0    # Orange Pi
```

---

## Architecture

```
src/
  main.c       ← entry point, scan loop, web server thread
  arp.c        ← raw AF_PACKET ARP sender/receiver
  oui.c        ← MAC → vendor (OUI database)
  probe.c      ← SSH banner / HTTP / mDNS probes
  web.c        ← embedded HTTP server (11 routes, JSON builder)
  comments.c   ← persistent host labels

web/
  build.py         ← inliner: web/src/{css,js} → web/scanner.html
  src/
    scanner.html   ← HTML skeleton (edit this)
    assets/css/    ← base, sidebar, components, table, scp, sysmonitor, ssh-terminal
    assets/js/     ← utils, hosts, comments, sidebar, scp, ssh-terminal, sysmonitor, main

scripts/
  gen-nginx.sh     ← scan + generate nginx config + landing page
```

**Embed flow:** `web/src/` → `python3 web/build.py` → `web/scanner.html` → `xxd -i` (via `make`) → `src/scanner_html.h` → compiled into binary.

---

## Skill system

AI agent documentation is split into focused skills. Read the manifest first.

**Manifest:** [`skills/manifest.json`](skills/manifest.json)

| Skill | What it covers |
|-------|---------------|
| [`c-scanner`](skills/c-scanner/SKILL.md) | C source structure, adding routes, JSON builder |
| [`web-ui`](skills/web-ui/SKILL.md) | Dashboard editing, build.py, JS/CSS conventions |
| [`deploy-pi`](skills/deploy-pi/SKILL.md) | SCP deploy, gen-nginx.sh, Pi/Orange Pi setup |
| [`deploy`](skills/deploy/SKILL.md) | Cross-compile targets, Makefile, GLIBC notes |
| [`probe`](skills/probe/SKILL.md) | SSH/HTTP/mDNS probes, adding probe types |
| [`arp-scan`](skills/arp-scan/SKILL.md) | Raw ARP engine, OUI lookup |
| [`nginx-proxy`](skills/nginx-proxy/SKILL.md) | Port mapping, reverse proxy, /regen route |
| [`multi-scp`](skills/multi-scp/SKILL.md) | Multi-device SCP file transfer panel |

### For AI agents

```bash
./scripts/load_skill.sh list          # list all skills
./scripts/load_skill.sh web-ui        # print web-ui skill
```

Read `CLAUDE.md` for full project conventions, coding rules, and sprint history.
Never edit `web/scanner.html` or `src/scanner_html.h` directly — both are generated.
