# SSH Terminal Card — DECISION_LOG.md

Sprint: 2026-05-22-sprint-2-ssh-terminal

---

## D1 — REST (request/response) vs WebSocket for command execution

**Decision:** REST — each command is a POST `/api/ssh-exec`, response is JSON.

**Rejected:** WebSocket (streaming, persistent server-side PTY).

**Reasoning:**
The existing codebase has zero WebSocket infrastructure. Every current API
(`/api/scan`, `/api/browse`, `/api/multi-scp`, etc.) is request/response via
a plain blocking accept loop in `web.c`. Adding WebSocket would require:
- An event loop or thread-per-connection model
- Client-side WebSocket reconnect logic
- A persistent `popen()` / PTY process on the server

Scope far exceeds the feature value. REST with `popen()` + `fread()` is the
existing pattern (used by SCP browse, ls, multi-scp) and is sufficient for the
target use case: `ls`, `cat`, `systemctl status`, `df -h`, `ps aux`.

Interactive programs (vim, top, journalctl -f) remain out of scope.

---

## D2 — Shell injection mitigation: Option A (strip single-quotes)

**Decision:** Strip all `'` characters from the `command` field via an in-place
`strchr` loop before the `snprintf` call in `route_api_ssh_exec()`.

**Rejected options:**
- Option B (`'` → `'\''`): Correct POSIX escaping, but if the outer quoting in
  the snprintf format string ever changes, Option B silently creates injection.
  A bug in Option B creates a security hole; a bug in Option A cannot.
- Option C (character whitelist): Too restrictive for sysadmin commands
  that include paths, flags, and arguments with spaces.

**Rationale for Option A:** The target commands on Luckfox/Pi boards do not
require single-quotes. `ls -la /root`, `cat /etc/os-release`, `systemctl status
mosquitto` — none need them. Silent stripping is acceptable; rejection would
confuse users who reflexively quote paths.

The existing deny list (`;&|` etc.) remains as-is for other metacharacters.
The strip runs first, then the deny check, so the two layers compose cleanly.

---

## D3 — Credentials: hardcoded in C, JS never holds them

**Decision:** `route_api_ssh_exec()` hardcodes `-p luckfox` (matching all other
sshpass calls in web.c). JS sends only `{ip, command}`. No password field in the
UI, no credential selection logic in JS.

**Rejected:** credential field in UI, per-host password selection.

**Reasoning:** This is the existing pattern for all SCP operations. scp.js
contains zero credential logic — confirmed by grep. The "same logic as SCP panel"
phrasing in spec.md referred to the C-side hardcoding pattern, not any JS
credential selection. Adding a password UI would create a security concern
(password visible in browser, stored in JS) for no gain on a private LAN tool.

---

## D4 — build.py JS_ORDER: spec was wrong, corrected before tasks.md

**Decision:** `ssh-terminal` MUST be in `JS_ORDER` in `web/build.py`.
Spec.md incorrectly stated "ssh-terminal.js is NOT added to build.py JS_ORDER".

**How the error was caught:** Before writing tasks.md, build.py was read to
understand what it actually does. Finding:

```python
js_block = '\n'.join(slurp(f'{JS_DIR}/{n}.js') for n in JS_ORDER)
html = re.sub(r'(?:<script src="assets/js/[^"]+\.js"></script>\n)+',
              lambda m: js_repl, html)
```

The regex matches ALL consecutive `assets/js/*.js` script tag groups and replaces
each group with the full `js_block`. If `ssh-terminal` is absent from `JS_ORDER`,
`js_block` does not contain its code. The `<script src>` tag pointing to it gets
consumed by the regex match and replaced with the js_block — which lacks
ssh-terminal code. Result: ssh-terminal.js is silently absent from the binary.

The spec assumption ("like sysmonitor.js before the fix") was based on an outdated
mental model. When the spec was written, sysmonitor.js was believed to be excluded
from JS_ORDER. By the time tasks.md was written, build.py showed sysmonitor in
JS_ORDER. ssh-terminal follows the same pattern.

**Corrected in tasks.md as T1 (first task, highest priority).**

Position in JS_ORDER: between `'scp'` and `'sysmonitor'`.
Position of `<script src>` tag in scanner.html: before the CDN tag (Group 1),
since ssh-terminal has no Chart.js dependency.

---

## D5 — ssh-terminal.js script tag: before CDN (Group 1), not after

**Decision:** `<script src="assets/js/ssh-terminal.js">` goes after scp.js
(line 518) and before the CDN Chart.js tag (line 519).

**Rejected:** placing it after the CDN (alongside sysmonitor.js, main.js).

**Reasoning:** ssh-terminal.js has no Chart.js dependency (`new Chart()` is never
called). Placing it in Group 2 (after CDN) would mean:
- Group 1 replacement (js_block) runs without ssh-terminal — ssh-terminal
  globals undefined when sidebar.js init fires at end of Group 1 block
- Group 2 replacement re-runs js_block (including ssh-terminal) with Chart available

While the re-execution of Group 2 would eventually define ssh-terminal globals,
the `applyPinState()` call at the end of sidebar.js runs during Group 1. If
`sshTermSetPinned` doesn't exist yet, the `typeof sshTermSetPinned === 'function'`
guard in sidebar.js would pass silently. Placing ssh-terminal before the CDN
ensures all globals are available before sidebar.js init fires.
