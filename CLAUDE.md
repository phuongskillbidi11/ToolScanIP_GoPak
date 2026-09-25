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

**Last updated:** 2026-09-25
**Completed sprints:** Sprint 2 — SSH Terminal card (2026-05-23) | Sprint 3 — Multi-IP tabbed SSH Terminal (2026-05-23) | Multi SCP / Web Reliability Bugfixes (2026-09-11, `.plans/2026-09-10-multi-scp-reliability-fixes/`) | Service Logs Panel (2026-09-11, `.plans/2026-09-11-service-logs-continuation/` — continuation/recovery of the never-fully-landed `.plans/2026-06-01-sprint-service-logs/`) | gen-nginx.sh systemd-conflict fix (2026-09-15, `.plans/2026-09-11-gen-nginx-systemd-conflict-fix/`) | Regenerate stale web/scanner.html (2026-09-15, `.plans/2026-09-15-fix-stale-scanner-html/`) | Centralized log collection for the Luckfox fleet (2026-09-25, `.plans/2026-09-25-centralized-log-collection/`) | Trim Pi 1's local log buffer (2026-09-25, `.plans/2026-09-25-trim-pi1-local-logs/`) | Fleet SSH key + unattended log collection (2026-09-25, `.plans/2026-09-25-distribute-an-ssh-key-from-pi-1-to-the-l/`)

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
- [x] `route_regen()` empty-scan guard, seeded from the existing nginx proxy conf at startup (`g_last_good_dev_count`, `seed_last_good_dev_count()`) — a bad ARP scan (or a scan before comments have loaded) no longer wipes the live nginx reverse-proxy config
- [x] SSH host-key-changed auto-recovery (`ssh_run_with_hostkey_retry()` shared helper for `scp_worker()`'s 3 remote-SSH sub-steps, plus inline retries in `ssh_exec()`, `route_api_browse()`, `route_api_ssh_exec()`) — a board with a changed host key self-heals on the next Multi SCP action instead of failing forever
- [x] `ip=LOCAL` support in `route_api_mkdir()`/`route_api_rm()`/`route_api_rename()` — create/delete/rename now work on the Pi's own filesystem, matching `route_api_chmod()`'s existing pattern
- [x] `scpFriendlyError()` in `web/src/assets/js/scp.js` — short human-readable SSH error text in the Multi SCP UI instead of raw OpenSSH banners
- [x] Checkbox on directory rows in the Multi SCP target file browser (`renderTargetBrowserFiles()`), with a click-target guard so it doesn't also trigger folder navigation
- [x] Service Logs sidebar panel — live `journalctl -u ipscanner` tail (with `is-active` status + `NRestarts` count) via `GET /api/service-logs` (`route_api_service_logs()` in `src/web.c`), rendered with per-line error/warn/info coloring, 10s auto-refresh while pinned (`web/src/assets/js/service-logs.js`, `web/src/assets/css/service-logs.css`)
- [x] `scripts/gen-nginx.sh` auto-detects whether `ipscanner` is a systemd-managed unit before restarting it — `systemctl restart ipscanner` when it is, the original manual `pkill`+`nohup` only when it isn't — instead of always doing the manual restart regardless
- [x] **Centralized log collection for the Luckfox node fleet** (separate from `ipscanner` itself — new fleet-operations tooling, not an app feature): `scripts/collect-node-logs.sh` runs on Pi 1, pulls new `/var/log/isoft-node-oee.log` lines from every currently-scanned node (via `ipscanner`'s own `/api/scan`, same `'Line'+'[GM'` filter as `gen-nginx.sh`) over SSH, with per-node offset tracking, host-key-mismatch auto-retry, and pruning of local files for IPs that drop out of the current scan. Promtail (on Pi 1) tails the collected local files and ships them to Loki (on a separate RK3568 board, `100.82.22.47`, data under `/userdata/loki`), viewable via Grafana (same board, port 3000). Nothing installed on the Luckfox nodes themselves. See `.plans/2026-09-25-centralized-log-collection/` for full design/decisions.
  - **Runs unattended every 1 min** via `node-log-collector.timer` (units in `monitoring/`, installed in `/etc/systemd/system/` on Pi 1), executing the root-owned copy at `/usr/local/sbin/collect-node-logs.sh` (not `/home/admin/`; deploy with `sudo install -m 755 -o root -g root`). Pi 1 logs in to nodes with a dedicated key, `/root/.ssh/id_ed25519_fleet` (comment `ipscanner-log-collector@pi1`, `BatchMode=yes`). The key is installed as one line in each node's `/root/.ssh/authorized_keys`, and password auth on nodes is untouched (ipscanner's own `sshpass -p luckfox` features still use it).
  - **New or reflashed node without the key:** run `sudo /usr/local/sbin/distribute-fleet-key.sh` on Pi 1 (idempotent; source `scripts/distribute-fleet-key.sh`). It tries the candidate root passwords in the root-only `/root/.fleet-passwords` and **never auto-clears a changed host key** (it reports it instead). Revoke procedure: `.plans/2026-09-25-distribute-an-ssh-key-from-pi-1-to-the-l/tasks.md` → Rollback.
  - Local files are truncated once Promtail's `positions.yaml` shows them fully shipped (`.plans/2026-09-25-trim-pi1-local-logs/`). Orphan pruning skips empty scans and only removes an IP's files after 24h without a successful collection.
  - Collection warnings are expected for `.125` (key not installed, root password unknown) and for `.110 .113 .120` (no OEE app installed on those boards, so there's no `/var/log/isoft-node-oee.log`).

### Known issues / tech debt
- `web/src/assets/js/scp.js` is ~1380 lines — largest JS file, candidate for future split
- No automated tests — all verification is manual (deploy + browser)
- `make deploy` for Pi 1 still requires interactive SSH key (no sshpass)
- Bare `make` only rebuilds `web/scanner.html` (its first Makefile rule, before `all:`) — use `make all` for a real native build; `.agent/project.yaml`'s `stack.test_cmd: make test` also doesn't match the Makefile (no `test` target exists)
- Persistent comments (`src/comments.c`) match primarily by MAC, and several Luckfox boards' MACs were observed changing between ARP scans in production — causing "Luckfox Nodes" to intermittently show fewer devices than are actually online. Not something this project's code was changed to fix; a workaround (re-adding affected boards' comments keyed by IP instead of MAC) was applied operationally on Pi 1, not in code
- `.gitignore` has an untracked-relative-to-plan `.agent/logs/` addition from harness setup — flagged by `eng verify` as outside any single plan's write_scope; not itself a bug
- Log-level detection (error/warn/info by substring match) is duplicated between `write_landing_page()`'s mini preview and `route_api_service_logs()` in `src/web.c` — pre-existing in the recovered 2026-06-01 code, not de-duplicated (out of scope for the recovery sprint)
- The `aarch64-linux-gnu-gcc` cross-compile toolchain is not installed in this WSL2 dev environment — `make aarch64`/Orange Pi build could not be verified in the 2026-09-11 Service Logs sprint (native + armhf both verified clean)
- Pi 1 was previously (wrongly) assumed to run `ipscanner` manually, not under systemd. Confirmed live on 2026-09-11 that Pi 1 actually runs `ipscanner.service` under systemd — this caused a real restart-crash-loop production incident when `gen-nginx.sh`'s old unconditional `pkill`+`nohup` raced systemd's own `Restart=` policy (fixed 2026-09-15, see `.plans/2026-09-11-gen-nginx-systemd-conflict-fix/`). Don't assume any board's systemd status — check with `systemctl status ipscanner` before writing deploy/restart logic that touches the process
- **`uname -m` alone does not tell you which binaries will run on a board.** Pi 1's kernel reports `aarch64` (`uname -m`), but its actual userspace is 32-bit armhf (`dpkg --print-architecture` → `armhf`; only `/lib/ld-linux-armhf.so.3` exists, not the aarch64 interpreter) — a 64-bit-capable kernel with 32-bit userspace is a valid, real Raspberry Pi OS configuration. Discovered 2026-09-25 installing Promtail (a 64-bit arm64 binary failed with "required file not found" before this was caught). Always check `dpkg --print-architecture` (or which `/lib/ld-linux-*.so*` exists) before assuming a board's binary-compatible architecture from `uname -m`
- The Luckfox node fleet does **not** all share one root password. Verified 2026-09-25 over the 24 scanned nodes: two root passwords (both kept in Pi 1's root-only `/root/.fleet-passwords`, never in the repo) cover 23/24. `.125` (Line FX2 [GM1R]) accepts neither and is intentionally skipped. Don't assume a single shared credential
- Pi 1's `admin` has **NOPASSWD sudo**. Use `sudo -n`, and never pipe the sudo password ahead of other stdin: it won't be consumed, so it lands in the command's input (this happened once while creating `/root/.fleet-passwords` and was caught before any use)
- Luckfox nodes' BusyBox has no `stat` and no `ls -e`. Use `ls -ld` / `ls -l --full-time`
- Not every scanned "Line…[GM…]" board runs the OEE app: `.110`, `.113`, `.120` have no `S99nodeOEE` and no OEE log (checked 2026-09-25)
- The RK3568 monitoring board (`100.82.22.47`)'s root partition (`/`) is tight (~1.2GB free of 5.9GB) — installing anything there via a standard `.deb`/`.rpm` package (which hard-codes root-partition install paths) risks filling it completely (happened installing Grafana 2026-09-25, recovered via `dpkg --purge` + cleanup). Prefer the standalone/tarball form of anything installed there, extracted under `/userdata/` (which has ~19GB free), matching how Loki/Grafana are both actually installed
- Grafana on the RK3568 board (`100.82.22.47:3000`) is still running its **default `admin`/`admin` credentials** — the operator saw Grafana's own security warning and explicitly chose to keep them for now (2026-09-25). Revisit before this board's network exposure changes beyond Tailscale-only
- Promtail is no longer published in current Loki GitHub releases (Grafana is migrating toward Grafana Alloy) — the last release confirmed to still ship a `promtail-linux-arm*.zip` asset is **v3.6.5**; that's what's installed on Pi 1, even though the Loki *server* itself is newer (v3.7.8) — the push API between them is stable across this gap

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
| Regen guard | Skip nginx rewrite on 0-device scan if last known count was >0, seeded from existing conf at startup | Scan retries / blocking startup on first good scan | Multi SCP fixes (2026-09-11) |
| Host-key auto-recovery | Shared helper for `scp_worker()`'s 3 sub-steps; simple inline retry for the 3 single-`popen()` routes | One helper for every SSH call site in the file | Multi SCP fixes (2026-09-11) |
| Live-device test gating | Split `tests.md` into Executor-run (local) vs. human-approved (live-device) sections | Executor deploys/tests directly against production | Multi SCP fixes (2026-09-11) |
| Service Logs source | `journalctl -u ipscanner`, parsed server-side into JSON | Reading a raw log file directly | Service Logs (2026-06-01 design, landed 2026-09-11) |
| Service Logs scope (continuation) | Pi 1/local verification only; Pi 2 + Orange Pi deploy/smoke-test deferred | Deploying to all boards before calling the sprint done | Service Logs (2026-09-11) |
| systemd-conflict restart fix | Auto-detect via `systemctl is-enabled`/`is-active` + `/run/systemd/system`, branch to `systemctl restart` | Always assume systemd; always assume manual | gen-nginx.sh fix (2026-09-15) |
| Log collection topology | Pi 1 pulls via SSH (no agent on nodes) → Promtail → Loki+Grafana on a separate RK3568 board | Agent installed on each 87MB-RAM node; Grafana Cloud (data leaves network); Windows mini PC (too little free RAM) | Log collection (2026-09-25) |
| Log collection node list | Reuse `ipscanner`'s own `/api/scan` + `gen-nginx.sh`'s comment filter | Separately maintained static IP list | Log collection (2026-09-25) |
| SSH key distribution to fleet | Split into its own separate plan | Bundled into the log-collection plan | Log collection (2026-09-25) |
| Monitoring-board install method | Standalone tarball under `/userdata/` | Standard `.deb`/`.rpm` (fills the board's tight root partition) | Log collection (2026-09-25) |
| Pi 1 log trim | Truncate once Promtail's `positions.yaml` shows a file fully shipped | Time-based buffer; dropping Promtail for direct push | Trim Pi 1 logs (2026-09-25) |
| Fleet SSH auth | Dedicated passphrase-less ed25519 key (`id_ed25519_fleet`) for Pi 1 root, one line appended per node | Default key path; passphrase + agent; disabling node password auth | Fleet SSH key (2026-09-25) |
| Key distribution | Idempotent `distribute-fleet-key.sh`, passwords from a root-only file via `sshpass -e`, 2-node canary then fleet | Manual `ssh-copy-id`; hardcoded passwords; `sshpass -p` | Fleet SSH key (2026-09-25) |
| Unattended schedule | systemd timer, 1 min, root-owned script in `/usr/local/sbin` | cron; running the admin-owned copy | Fleet SSH key (2026-09-25) |
| Orphan pruning under the timer | Skip on empty scan; prune only after 24h stale (`.offset` mtime) | Prune on first missing scan (would re-pull + duplicate in Loki on every dropout) | Fleet SSH key (2026-09-25) |

### What NOT to regenerate
- `web/scanner.html` — generated by `web/build.py`, do not edit directly
- `src/scanner_html.h` — generated by `make`, do not edit directly
