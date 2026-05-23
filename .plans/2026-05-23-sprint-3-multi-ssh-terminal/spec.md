# Multi-IP SSH Terminal — Sprint 3 Spec

Sprint folder: `.plans/2026-05-23-sprint-3-multi-ssh-terminal/`

---

## Goal

Upgrade the SSH Terminal panel from a single-session model to a multi-session
tabbed model. Each connected device gets its own tab and independent output
history. The IP field becomes a dropdown (same pattern as `scp-source-sel`)
populated from the live scanned host list.

## Done looks like

1. The connect row shows a `<select>` with all scanned hosts + a "Custom IP…"
   option (like scp-source-sel). Selecting "Custom IP…" reveals a text input.
2. Clicking "Add" opens a new tab for that IP. Duplicate IPs are silently
   ignored (no second tab for the same IP).
3. Clicking a tab switches the output area to that device's history.
4. Clicking × on a tab closes that session. If it was the active tab, the
   adjacent tab is activated. If the last tab is closed, the output clears.
5. Command input targets the currently active tab's IP.
6. Output history per tab is preserved when switching between tabs.
7. SCP source selection pre-fills the dropdown (does not auto-add a session).
8. All changes build and deploy with `python3 web/build.py && make deploy2`.

---

## Background

Sprint 2 delivered a single-session SSH Terminal. The user needs to SSH into
multiple Luckfox/Pi devices simultaneously — checking service status, comparing
logs, running the same command across devices. Single-session makes this
impossible without repeatedly disconnecting and reconnecting.

---

## Key discoveries from codebase

| Location | Symbol | Significance |
|---|---|---|
| `web/src/assets/js/hosts.js:1` | `var allHosts = []` | Global array of scanned hosts; populated by `/api/scan` fetch |
| `web/src/assets/js/hosts.js:157` | `allHosts = data.hosts` | Set on every scan response — always current |
| `web/src/assets/js/scp.js:24` | `scpPopulateSources()` | Exact pattern to follow: iterates `allHosts`, builds `<option value=h.ip>` with comment + hostname, appends `__custom__` at end |
| `web/src/assets/js/scp.js:29–34` | option label format | `h.ip + (h.comment ? ' — ' + h.comment : '') + (h.hostname && h.hostname !== h.ip ? ' [' + h.hostname + ']' : '')` |
| `web/src/assets/js/ssh-terminal.js:1` | `var sshTermSessions` | Will replace `sshTermState` — must remain a `var` (not `let/const`) for hoisting guard pattern |
| `web/src/assets/js/ssh-terminal.js:64` | `if (!sshTermState) return` | Hoisting guard — must be updated to `if (!sshTermSessions) return` everywhere |
| `web/src/assets/js/sidebar.js:31` | `typeof sshTermSetPinned === 'function'` | sidebar.js calls `sshTermSetPinned` — function must remain in ssh-terminal.js with updated guard |
| `web/src/assets/js/scp.js:151–158` | `onSourceChange()` | Calls `syncSSHTabsFromSCP()` at line 158 — function signature unchanged |
| `web/src/scanner.html:227–250` | `ssh-terminal-card` | Current card HTML — connect row, output div, cmd row to be updated |
| `web/src/scanner.html:548` | ssh-terminal.js script tag | Already in Group 1 (before CDN) — no change needed |
| `web/build.py:18–19` | CSS_ORDER, JS_ORDER | Both already include `ssh-terminal` — no change needed |

---

## Architecture

### State model (replaces `sshTermState`)

```javascript
var sshTermSessions  = [];   // [{ ip, history, inFlight }]
var sshTermActiveIdx = -1;   // -1 = no open sessions
```

Each session object:
```javascript
{ ip: '192.168.1.5', history: '$ ls /root\napply_uart...\n', inFlight: false }
```

History is a plain string — appended on each command, rendered into the single
output div when the tab is activated.

### IP selector

`<select id="ssh-terminal-ip-sel">` populated by `sshTermPopulateSel()`:

```
— select device —     (value="")
192.168.1.5 — label  (value="192.168.1.5")
192.168.1.29          (value="192.168.1.29")
— custom IP… —        (value="__custom__")
```

When `__custom__` is selected, `<input id="ssh-terminal-ip-custom">` becomes
visible (same `display:''` / `display:'none'` pattern as scp.js custom bar).

`sshTermPopulateSel()` mirrors `scpPopulateSources()` exactly: iterates
`allHosts`, same label format (`h.ip + comment + hostname`), `__custom__` last.

`sshTermPopulateSel()` must be called:
- When the SSH Terminal panel is first opened (from `sshTermSetPinned(true)`)
- Whenever `allHosts` updates — hook into the existing hosts.js scan response
  by calling it at the end of `renderHosts()` (already calls SCP's populate)

### Tab strip

`<div id="ssh-terminal-tabs" class="ssh-terminal-tabs">` rendered by
`sshTermRenderTabs()`. Each session becomes:

```html
<div class="ssh-terminal-tab [active]" onclick="sshTermActivate(N)">
  192.168.1.5
  <span class="ssh-terminal-tab-close" onclick="sshTermClose(event, N)">×</span>
</div>
```

### Output

Single `<div id="ssh-terminal-output">`. Content is set to
`sshTermSessions[sshTermActiveIdx].history` on tab activation and on run
completion. No per-tab DOM elements — history lives in the JS session object.

### Add session flow

1. User selects IP from dropdown (or types custom IP)
2. Clicks "Add" → `sshTermAddSession(ip)`
3. Validates IP via `valid_ip`-equivalent JS regex (`/^\d{1,3}(\.\d{1,3}){3}$/`)
4. If `sshTermSessions.some(s => s.ip === ip)` → activates existing tab, returns
5. Pushes `{ ip, history: '', inFlight: false }`, re-renders tabs, activates new tab

### Close session flow

1. Click × → `sshTermClose(event, idx)` (stops propagation first)
2. `sshTermSessions.splice(idx, 1)`
3. New active: `Math.min(idx, sshTermSessions.length - 1)` (or -1 if empty)
4. Re-render tabs + output

### Run flow

`sshTermRun()`:
1. Guard: `sshTermActiveIdx === -1` → return
2. Guard: active session `inFlight` → return
3. Get `cmd` from input, clear input
4. Append `'$ ' + cmd + '\n'` to active session history, render output
5. Set `inFlight = true`, update UI
6. `fetch('/api/ssh-exec', { method:'POST', body: JSON.stringify({ ip, command: cmd }) })`
7. On response: append stdout + stderr to history, set status, render output
8. Finally: `inFlight = false`, update UI

### syncSSHTabsFromSCP()

Updated to set the dropdown value instead of the text input:
```javascript
function syncSSHTabsFromSCP() {
  if (!sshTermSessions) return;
  var sel = sshTermEl('ssh-terminal-ip-sel');
  if (!sel) return;
  var ip = (typeof scpSourceIP !== 'undefined' && scpSourceIP && scpSourceIP !== 'LOCAL')
         ? scpSourceIP : '';
  // Only set if the option exists in the dropdown
  sel.value = ip;
  if (sel.value !== ip) sel.value = '';   // not found — reset to placeholder
}
```

---

## In scope

1. `web/src/assets/js/ssh-terminal.js` — full rewrite (state, tabs, selector, run)
2. `web/src/assets/css/ssh-terminal.css` — add tab strip styles, update connect row
3. `web/src/scanner.html` — update card HTML (selector + custom input + tab strip div)

## Out of scope

- **Backend changes** — `route_api_ssh_exec()` is stateless per-command; no changes needed
- **build.py changes** — `ssh-terminal` already in both CSS_ORDER and JS_ORDER
- **sidebar.js changes** — panel registration unchanged
- **scp.js changes** — `syncSSHTabsFromSCP()` call sites unchanged (function signature unchanged)
- **Broadcast mode** — sending one command to all tabs simultaneously
- **Session persistence across page reload** — sessions are in-memory only
- **Per-tab command history** — arrow-key history within a session not in scope

---

## Affected files

| File | Change type | Reason |
|---|---|---|
| `web/src/assets/js/ssh-terminal.js` | Full rewrite | State model, tab management, selector, updated run/sync |
| `web/src/assets/css/ssh-terminal.css` | Edit — add tab strip rules | Tab strip requires new CSS classes |
| `web/src/scanner.html` | Edit — card HTML only | Replace IP input with selector + custom input; add tab strip div |

**Not touched:** `src/web.c`, `web/build.py`, `web/src/assets/js/sidebar.js`,
`web/src/assets/js/scp.js`, `web/src/assets/js/hosts.js`, all other files.

---

## Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| `allHosts` empty when panel first opens | Medium — depends on scan timing | `sshTermPopulateSel()` checks `if (!allHosts.length)` and adds a "Run Rescan first" note |
| Duplicate tab for same IP | Low | Check `sshTermSessions.some(s => s.ip === ip)` before push; activate existing |
| Hoisting guard breaks after state rename | High if missed | Every function callable from sidebar.js must use `if (!sshTermSessions) return` |
| History grows unbounded in a long session | Low for this use case | Cap at 50 KB: if `history.length > 51200`, trim from front |
| Custom IP input not validated | Medium | Validate with `/^\d{1,3}(\.\d{1,3}){3}$/` before creating session |

---

## Self-evaluation (plan-quality.md rubric)

| Criterion | Score | Notes |
|---|---|---|
| Goal is specific and binary | 2/2 | 8 verifiable acceptance criteria |
| Background explains why now | 1/1 | Single-session limits multi-device workflows |
| Discoveries reference actual file:line | 2/2 | 11 entries with file, function, line |
| Architecture decision recorded | 1/1 | History-in-JS-object vs per-tab DOM; selector pattern; syncSSHTabsFromSCP change |
| Scope has ≥3 explicit exclusions | 2/2 | 6 out-of-scope items |
| Affected files complete with change type | 1/1 | 3 files, all with type + reason |
| Risks identified with mitigations | 1/1 | 5 risks with mitigations |
| **Total** | **10/10** | |
