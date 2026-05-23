# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

## Project-specific: Enable Planner Mode
Read and follow .claude/planner-instructions.md strictly.
Do not write code. Only create plans in .plans/.

## PLANNER MODE ACTIVE
You are an AI Planner. Follow .claude/planner-instructions.md strictly. Do NOT write code.

---

## Project overview

**Project:** ToolScanIP (ipscanner)
**Type:** C network scanner, cross-compiled for embedded Linux
**Language(s):** C (scanner core), HTML/CSS/JS (web UI), Bash (scripts), Python (build tools)
**Description:** ARP-based IP scanner with a web dashboard, nginx reverse-proxy, and file-transfer UI for Luckfox IoT devices on a LAN.

---

## Commands

```bash
# Build (native x86_64 — WSL2)
make

# Build for Raspberry Pi (armhf)
make pi

# Build for Orange Pi (aarch64)
make aarch64

# Rebuild embedded HTML (from web/src/ sources)
python3 web/build.py

# Deploy to Pi 1 (admin@100.66.44.107)
make deploy

# Deploy to Pi 2 (intercom@100.70.31.100, uses sshpass)
make deploy2   # or: make build2  (compile + deploy in one step)

# Deploy to Orange Pi (root@100.101.117.23)
make deploy3

# Run locally (x86_64, web UI on port 8080)
sudo ./ipscanner -i eth0 -w 8080
```

---

## Architecture

```
src/
  main.c       ← entry point, arg parsing, scan loop
  arp.c        ← raw AF_PACKET ARP sender/receiver
  oui.c        ← MAC → vendor lookup (OUI database)
  probe.c      ← SSH banner / HTTP / mDNS active probes
  scanner.c    ← orchestrates ARP + probing + display
  display.c    ← terminal table renderer
  comments.c   ← persistent host labels (~/.ipscanner.comments)
  web.c        ← embedded HTTP server (routes + JSON builder)
  scanner_html.h  ← auto-generated from web/scanner.html via xxd

web/
  scanner.html     ← inlined output (never edit directly)
  build.py         ← inliner: web/src/ → web/scanner.html
  src/
    scanner.html   ← HTML skeleton (edit this)
    assets/css/    ← base, sidebar, components, table, scp, sysmonitor, ssh-terminal
    assets/js/     ← utils, hosts, comments, sidebar, scp, ssh-terminal, sysmonitor, main

scripts/
  gen-nginx.sh              ← scan + generate nginx config + landing page
  fetch-machine-names.sh    ← populate comments from MQTT/DNS
  mqtt-sync-comments.sh     ← sync comments via MQTT
  snapshot.sh               ← save/restore scanner state
```

**Data flow:** ARP scan → OUI lookup → active probing → in-memory host list → JSON via `/api/scan` → web dashboard.  
**Embed flow:** `web/src/*.css + *.js` → `web/build.py` → `web/scanner.html` → `xxd -i` → `src/scanner_html.h` → compiled into binary.

---

## Key files

| File | Purpose |
|------|---------|
| `src/main.c` | Entry point — CLI args, scan interval, web server thread |
| `src/arp.c` | Raw socket ARP implementation |
| `src/web.c` | HTTP server: all routes, JSON builder, nginx helpers |
| `web/src/assets/js/scp.js` | Multi-SCP file transfer UI logic (1330 lines) |
| `web/src/assets/js/hosts.js` | Host list render + filter + select |
| `web/src/assets/css/scp.css` | SCP panel styles |
| `Makefile` | All build + deploy targets |
| `scripts/gen-nginx.sh` | Full nginx config regen + daemon restart |

---

## Coding conventions

- C11 (`-std=c11 -D_GNU_SOURCE`), `-Wall -Wextra -O2`
- No dynamic allocation in hot paths — stack buffers preferred
- HTML/JS: plain globals, no frameworks, no ES modules
- `wstr(fd, s)` / `wfmt(fd, fmt, ...)` for HTTP responses in `web.c`
- Never edit `web/scanner.html` or `src/scanner_html.h` directly — both are generated
- BusyBox `ls` (Luckfox remote) does not support `--time-style`; use plain `ls -la`

---

## Skill system

Skills are in `skills/` — read `skills/manifest.json` before any plan.

```bash
./scripts/load_skill.sh list          # see all skills
./scripts/load_skill.sh web-ui        # print a skill
./scripts/update-manifest.sh          # regenerate manifest after adding a skill
```

---

## Cross-compile targets

| Target | Compiler | Board | Tailscale |
|--------|----------|-------|-----------|
| x86_64 | gcc (native) | WSL2/Ubuntu | — |
| armhf | arm-linux-gnueabihf-gcc | Raspberry Pi 1 & 2 | 100.66.44.107 / 100.70.31.100 |
| aarch64 | aarch64-linux-gnu-gcc | Orange Pi (Rockchip64) | 100.101.117.23 |

GLIBC on boards: 2.31 (Pi & Orange Pi). Link pthread statically for aarch64.

---

## Out-of-scope for Claude to implement directly

- Production deployments
- Modifying nginx configs on live boards without `make deploy`
- Secrets / credentials management

---

## Current state

> **Update this section at the end of every sprint.**
> Claude Code reads this before writing any plan.

**Last updated:** 2026-05-23
**Completed sprints:** Sprint 2 — SSH Terminal card (2026-05-23) | Sprint 3 — Multi-IP tabbed SSH Terminal (2026-05-23)

### What exists
- [x] ARP scanner with OUI lookup, SSH/HTTP probing (`src/arp.c`, `src/probe.c`)
- [x] Embedded HTTP server with 11 routes (`src/web.c`) — includes `POST /api/ssh-exec`
- [x] Web dashboard: host list, SSH panel, comments modal (`web/src/`)
- [x] Multi-SCP file transfer panel with chmod modal (`web/src/assets/js/scp.js`)
- [x] Collapsible sidebar with pin system (`web/src/assets/js/sidebar.js`)
- [x] File browser: dual-format ls parser (GNU + BusyBox), sort, permissions
- [x] Persistent comments by MAC or IP (`src/comments.c`, `/api/comment`)
- [x] nginx reverse-proxy config generator (`scripts/gen-nginx.sh`)
- [x] Cross-compile for armhf (Pi 1 & 2) and aarch64 (Orange Pi)
- [x] `make deploy2` via sshpass (password: `admin@123`)
- [x] Web source split: `web/src/assets/{css,js}/` → `web/build.py` inliner
- [x] SSH Terminal sidebar panel — multi-session tabbed, one tab per IP, output history per session; IP selector dropdown from `allHosts` + custom entry; `POST /api/ssh-exec` backend (`web/src/assets/js/ssh-terminal.js`, `web/src/assets/css/ssh-terminal.css`)

### Known issues / tech debt
- `web/src/assets/js/scp.js` is 1330 lines — largest JS file, candidate for future split
- No automated tests — all verification is manual (deploy + browser)
- `make deploy` for Pi 1 still requires interactive SSH key (no sshpass)

### Decisions made (summary)
_See `.plans/*/DECISION_LOG.md` for full reasoning_

| Decision | Chosen | Rejected | Sprint |
|----------|--------|----------|--------|
| HTML embedding | `xxd -i` → C byte array | Serving from filesystem | Initial |
| BusyBox ls compat | Auto-detect by token[5] format | Separate code paths | Informal |
| Sidebar state | Pin system (≥1 always visible) | Tab navigation | Informal |
| Pi 2 deploy auth | sshpass with hardcoded password | SSH key setup | Informal |
| Web source split | `web/src/` + `build.py` inliner | Single file, separate server | 2026-05-22 |
| SSH Terminal transport | REST POST `/api/ssh-exec` | WebSocket / persistent PTY | Sprint 2 |
| SSH Terminal injection | Strip `'` via strchr loop (Option A) | `'\''` escaping, whitelist | Sprint 2 |
| SSH creds location | Hardcoded `-p luckfox` in C only | Credential field in JS | Sprint 2 |
| ssh-terminal.js build | In `JS_ORDER`, `<script>` before CDN | Standalone after CDN | Sprint 2 |
| SSH Terminal output layout | Tabs per IP, history in JS object | Split panels, broadcast | Sprint 3 |
| SSH IP selector | `allHosts` dropdown + custom option | Text input only | Sprint 3 |
| SSH session state | `var sshTermSessions[]` array | Per-tab DOM elements | Sprint 3 |

### What NOT to regenerate
- `web/scanner.html` — generated by `web/build.py`, do not edit directly
- `src/scanner_html.h` — generated by `make`, do not edit directly
