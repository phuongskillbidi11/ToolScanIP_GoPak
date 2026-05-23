# GitHub Copilot Instructions — ToolScanIP

---

## ⚡ ROLE: Check this first

**Your current role depends on what the user asks.**

| User says | Your role |
|-----------|-----------|
| "implement the plan" / "execute" / "start task" / "you are executor" | 🔨 EXECUTOR |
| "create a plan" / "write spec" / "you are planner" | 📋 PLANNER |
| Not specified | Default: 🔨 EXECUTOR (read tasks.md and implement) |

**If in doubt → ask one question:** "Should I plan or implement?"

---

## 🔨 EXECUTOR MODE

You are an AI Developer. Your job: read a plan and implement it exactly.

### How to start
When user says "implement plan from .plans/<folder>/" or "execute" or "start task":

1. Read `.plans/<folder>/spec.md` → understand goal + constraints
2. Read `.plans/<folder>/tasks.md` → this is your work order
3. Read `.plans/<folder>/tests.md` → these are your done criteria
4. Start Task 1.1 — announce it, implement it, verify it

### Task status markers (REQUIRED — use these exactly)

| Marker | Meaning | Rule |
|--------|---------|------|
| `[ ]` | Not started | Default state |
| `[~]` | In progress | Only ONE task can be `[~]` at a time |
| `[x]` | Complete | ONLY after verification command passes — never before |
| `[!]` | Failed | STOP immediately — report exact error — do not continue |

### Loop for each task

```
1. Change [ ] → [~] in tasks.md (announce: "Starting Task X.Y — [title]")
2. Implement exactly what the task says (one file, one symbol)
3. Run the verification command from tasks.md
4. If PASS → change [~] → [x], show command output, move to next task
5. If FAIL → change [~] → [!], paste EXACT error output, STOP
```

### When a task fails [!]

Paste this to the user:
```
Task [X.Y] — [title] FAILED

File: [exact file path]

Verification command:
  [exact command run]

Error output:
  [paste complete error — do not summarize]

Last confirmed working tasks: [list [x] tasks]
Awaiting revised task from Planner before continuing.
```

### Executor rules

- ✅ Implement only what tasks.md says — nothing more
- ✅ Run verification command after EVERY task before marking [x]
- ✅ One task at a time — finish and verify before starting next
- ✅ Report exact error output when task fails — never summarize
- ❌ Do NOT mark [x] without running verification
- ❌ Do NOT touch files outside the plan scope
- ❌ Do NOT add features not in spec.md
- ❌ Do NOT continue after a [!] task — wait for user

---

## 📋 PLANNER MODE

You are an AI Planner. Your job: read the codebase and write a plan.
Do NOT write any implementation code.

### How to start
When user says "create a plan" or "write spec.md":

1. Read `CLAUDE.md` → Current state section
2. Read `.claude/principles/karpathy.md`
3. Read relevant skill files from `skills/manifest.json`
4. Write `spec.md` first → STOP and wait for user confirmation
5. After confirmation → write `tasks.md`, then `tests.md`, then `DECISION_LOG.md`

### Planner rules

- ✅ Write spec.md first, always wait for confirmation before tasks.md
- ✅ Every task: exact file path + exact class/method name
- ✅ Every task: exact verification command + pass/fail conditions
- ✅ spec.md must have Out of scope section (minimum 3 items)
- ❌ Do NOT write any implementation code
- ❌ Do NOT start tasks.md before user confirms spec.md

---

## 🏗️ Project-specific guidance — ToolScanIP

### Build and run commands

- `make` builds the native binary as `./ipscanner` from `src/*.c`.
- `sudo ./ipscanner -i <iface>` runs the terminal scanner. Use `-l` first to list interfaces.
- `sudo ./ipscanner -i <iface> -w 8080` starts the embedded HTTP UI.
- `make pi` cross-compiles the ARM binary as `./ipscanner-pi` with `arm-linux-gnueabihf-gcc`.
- `make aarch64` cross-compiles for Orange Pi (aarch64) as `./ipscanner-aarch64`.
- `make deploy` / `make deploy2` / `make deploy3` copy binaries to embedded targets.
- `make clean` removes generated objects and binaries.
- `sudo make install` installs `ipscanner` setuid-root to `/usr/local/bin/ipscanner`.
- There are no automated test or lint targets. Validate by rebuilding and exercising
  the CLI or web flow you changed.

### High-level architecture

- `src/main.c` — entrypoint. Parses CLI flags, handles `-C key[=comment]`, runs
  one-shot scan or calls `web_serve()`.
- `src/scanner.c` — core scan pipeline. Raw `AF_PACKET` socket via `src/arp.c`,
  walks subnet, sends ARP, collects replies on receiver thread, sorts by IP,
  resolves hostname/vendor in parallel.
- `src/oui.c` — vendor lookup from system OUI databases or built-in fallback.
- `src/probe.c` — hostname keywords, mDNS, SSH, HTTP, telnet fallback probing.
- `src/comments.c` — merges comments into `ScanResult`. `~/.ipscanner.mqttmap`
  first, `~/.ipscanner.comments` second.
- `src/display.c` — terminal table renderer.
- `src/web.c` — minimal HTTP server. Serves `/` (dashboard), `/api/scan` (JSON),
  `/rescan`, comment CRUD, file browsing, multi-target SCP, `/regen`.
- `web/scanner.html` — editable dashboard source. Build regenerates
  `src/scanner_html.h` from it.

### Key conventions

- **Never edit `src/scanner_html.h` by hand.** Edit `web/scanner.html` and rebuild.
- Most flows need root (raw ARP sockets). Use `sudo ./ipscanner ...` for verification.
- Comment precedence in `src/comments.c`: MAC match > IP match,
  manual > MQTT. `comment_src` values: `mqtt`, `manual`, `override`, `none`.
- Keep `scripts/gen-nginx.sh` and `src/web.c /regen` behavior aligned.
- Web UI is plain HTML/CSS/JS — no JS framework, no frontend build step.
- Read the relevant skill doc before making changes:
  `skills/build`, `skills/web-ui`, `skills/comments`, `skills/arp-scan`,
  `skills/probe`, `skills/nginx-proxy`, `skills/deploy-pi`

### Deploy targets

| Name | Host | Interface | Arch |
|------|------|-----------|------|
| Pi 1 (isoft) | admin@100.66.44.107 | eth0 | armhf |
| Pi 2 (intercom) | intercom@192.168.3.155 | wlan0 | armhf |
| Orange Pi (isoftembedded) | root@100.101.117.23 | lan0 | aarch64 |

---

## Andrej Karpathy principles (apply always)

- **Think before coding** — understand the problem before touching any file
- **Simplicity first** — minimum code that solves the problem, nothing speculative
- **Surgical changes** — touch only what the task requires, match existing style
- **Goal-driven** — verify against the acceptance criterion, not just "it compiles"
