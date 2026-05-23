# SSH Terminal Card — tasks.md

Sprint: 2026-05-22-sprint-2-ssh-terminal
Executor reads this after spec.md and tests.md. Implement tasks in order.
One task = one atomic commit. Mark each `[x]` when done.

---

## Rollback map

| File | Rollback action |
|---|---|
| `web/build.py:19` | Revert JS_ORDER to `['utils','hosts','comments','sidebar','scp','sysmonitor','main']` |
| `web/src/scanner.html` | Remove link tag, script tag, revert nav item to disabled, remove card HTML |
| `web/src/assets/js/sidebar.js` | Remove `'ssh-terminal'` from `sidebarPinned`, `getPanelEl`, `applyPinState` forEach |
| `web/src/assets/js/scp.js` | Remove two `syncSSHTabsFromSCP()` call lines (lines 158, ~100) |
| `src/web.c:~1398` | Remove single-quote strip loop (4 lines before deny check) |
| `src/web.c:1746` | Remove `else if (strcmp(path, "/api/ssh-exec") == 0)` dispatch line |
| `web/src/assets/css/ssh-terminal.css` | Delete file |
| `web/src/assets/js/ssh-terminal.js` | Delete file |

---

## T1 — build.py: add 'ssh-terminal' to JS_ORDER

**File:** `web/build.py:19`
**Change type:** Edit — insert one element into an existing list

Current line 19:
```python
JS_ORDER  = ['utils', 'hosts', 'comments', 'sidebar', 'scp', 'sysmonitor', 'main']
```

New line 19:
```python
JS_ORDER  = ['utils', 'hosts', 'comments', 'sidebar', 'scp', 'ssh-terminal', 'sysmonitor', 'main']
```

**Why this must be done first:** build.py concatenates JS_ORDER into a single `js_block` and replaces ALL `assets/js/*.js` script tag groups with it. If `ssh-terminal` is absent from JS_ORDER, its code is silently dropped from the deployed binary even though a `<script src>` tag points to it.

**Verify:**
```bash
grep "ssh-terminal" web/build.py
# must return: JS_ORDER  = [...'ssh-terminal'...]
```

---

## T2 — Create web/src/assets/css/ssh-terminal.css

**File:** `web/src/assets/css/ssh-terminal.css` (new file)
**Rollback:** delete file

Minimum CSS for the terminal card. Scope all rules to `.ssh-terminal-card` to avoid
colliding with the existing `.ssh-panel` (hostlist inline SSH widget, lines 120–132 of scanner.html).

```css
/* SSH Terminal panel — all rules scoped to avoid clash with .ssh-panel */
.ssh-terminal-card {
  margin-top: 0;
}

.ssh-terminal-output {
  font-family: 'Courier New', monospace;
  font-size: 12px;
  background: #1e1e1e;
  color: #d4d4d4;
  padding: 12px 14px;
  min-height: 220px;
  max-height: 400px;
  overflow-y: auto;
  white-space: pre-wrap;
  word-break: break-all;
  border-bottom: 1px solid #e5e7eb;
}

.ssh-terminal-connect-row {
  display: flex;
  gap: 8px;
  padding: 12px 14px;
  align-items: center;
  flex-wrap: wrap;
}

.ssh-terminal-ip-input {
  font-family: 'Courier New', monospace;
  font-size: 13px;
  border: 1px solid #d1d5db;
  border-radius: 6px;
  padding: 5px 10px;
  width: 160px;
}

.ssh-terminal-cmd-row {
  display: flex;
  gap: 8px;
  padding: 0 14px 12px;
  align-items: center;
}

.ssh-terminal-cmd-input {
  flex: 1;
  font-family: 'Courier New', monospace;
  font-size: 13px;
  border: 1px solid #d1d5db;
  border-radius: 6px;
  padding: 5px 10px;
}

.ssh-terminal-btn {
  font-size: 12px;
  padding: 5px 12px;
  border-radius: 6px;
  border: 1px solid #d1d5db;
  background: #f9fafb;
  cursor: pointer;
  white-space: nowrap;
}

.ssh-terminal-btn.primary {
  background: #2563eb;
  border-color: #2563eb;
  color: #fff;
}

.ssh-terminal-btn.primary:hover { background: #1d4ed8; }
.ssh-terminal-btn:disabled { opacity: .5; cursor: default; }

.ssh-terminal-status {
  font-size: 11px;
  color: #6b7280;
  padding: 0 14px 8px;
}

.ssh-terminal-error {
  font-size: 12px;
  color: #dc2626;
  padding: 0 14px 6px;
}

.ssh-terminal-error:empty { display: none; }
```

---

## T3 — Create web/src/assets/js/ssh-terminal.js

**File:** `web/src/assets/js/ssh-terminal.js` (new file)
**Rollback:** delete file

Module state and all terminal logic. Follow the same hoisting-guard pattern
established in sysmonitor.js (guard at top of any function callable from sidebar.js
before `sshTermState` is initialized).

```javascript
var sshTermState = {
  ip: '',
  connected: false,
  inFlight: false
};

function sshTermEl(id) { return document.getElementById(id); }

function sshTermSetStatus(msg) {
  var el = sshTermEl('ssh-terminal-status');
  if (el) el.textContent = msg;
}

function sshTermSetError(msg) {
  var el = sshTermEl('ssh-terminal-error');
  if (el) el.textContent = msg || '';
}

function sshTermAppend(text) {
  var out = sshTermEl('ssh-terminal-output');
  if (!out) return;
  out.textContent += text;
  out.scrollTop = out.scrollHeight;
}

function sshTermClear() {
  var out = sshTermEl('ssh-terminal-output');
  if (out) out.textContent = '';
}

function sshTermUpdateUI() {
  var connectBtn = sshTermEl('ssh-terminal-connect-btn');
  var sendBtn    = sshTermEl('ssh-terminal-send-btn');
  var ipInput    = sshTermEl('ssh-terminal-ip');
  var cmdInput   = sshTermEl('ssh-terminal-cmd');
  if (connectBtn) connectBtn.textContent = sshTermState.connected ? 'Disconnect' : 'Connect';
  if (sendBtn)    sendBtn.disabled = !sshTermState.connected || sshTermState.inFlight;
  if (ipInput)    ipInput.disabled = sshTermState.connected;
  if (cmdInput)   cmdInput.disabled = !sshTermState.connected;
}

function sshTermConnect() {
  var ipInput = sshTermEl('ssh-terminal-ip');
  var ip = ipInput ? ipInput.value.trim() : '';
  if (sshTermState.connected) {
    /* Disconnect */
    sshTermState.connected = false;
    sshTermState.ip = '';
    sshTermSetStatus('Disconnected.');
    sshTermSetError('');
    sshTermUpdateUI();
    return;
  }
  if (!ip) { sshTermSetError('Enter an IP address.'); return; }
  sshTermState.ip = ip;
  sshTermState.connected = true;
  sshTermSetStatus('Connected to ' + ip + '. Type a command and press Run.');
  sshTermSetError('');
  sshTermClear();
  sshTermUpdateUI();
}

function sshTermRun() {
  if (!sshTermState) return;   /* hoisting guard */
  if (!sshTermState.connected || sshTermState.inFlight) return;
  var cmdInput = sshTermEl('ssh-terminal-cmd');
  var cmd = cmdInput ? cmdInput.value.trim() : '';
  if (!cmd) return;

  sshTermState.inFlight = true;
  sshTermSetError('');
  sshTermUpdateUI();
  sshTermAppend('$ ' + cmd + '\n');
  if (cmdInput) cmdInput.value = '';

  fetch('/api/ssh-exec', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ip: sshTermState.ip, command: cmd }),
    cache: 'no-store'
  })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.stdout) sshTermAppend(data.stdout);
      if (data.stderr) sshTermAppend('[stderr] ' + data.stderr);
      if (!data.stdout && !data.stderr) sshTermAppend('(no output)\n');
      sshTermSetStatus('exit ' + data.exit_code + '  (' + data.elapsed_ms + 'ms)');
    })
    .catch(function(err) {
      sshTermSetError('Request failed: ' + err);
    })
    .finally(function() {
      sshTermState.inFlight = false;
      sshTermUpdateUI();
    });
}

/* Called from scp.js onSourceChange() and checkbox listener to sync selected IP */
function syncSSHTabsFromSCP() {
  if (!sshTermState) return;   /* hoisting guard */
  if (sshTermState.connected) return;   /* don't hijack an active session */
  var ipInput = sshTermEl('ssh-terminal-ip');
  if (!ipInput) return;
  var ip = (typeof scpSourceIP !== 'undefined' && scpSourceIP && scpSourceIP !== 'LOCAL')
         ? scpSourceIP : '';
  ipInput.value = ip;
}

function sshTermSetPinned(on) {
  if (!sshTermState) return;   /* hoisting guard: var not yet initialized */
  /* No polling to start/stop — terminal is stateless between commands */
}

/* Key handler: Enter submits command */
function sshTermHandleKey(e) {
  if (e.key === 'Enter') sshTermRun();
}
```

---

## T4 — scanner.html: link tag, script tag, nav item, card HTML

**File:** `web/src/scanner.html`
**Rollback:** four separate changes to revert (link, script, nav item, card)

### 4a — Add CSS link tag

After line 13 (`<link rel="stylesheet" href="assets/css/sysmonitor.css">`), insert:
```html
<link rel="stylesheet" href="assets/css/ssh-terminal.css">
```

### 4b — Add JS script tag (before CDN, in Group 1)

After line 518 (`<script src="assets/js/scp.js"></script>`), insert:
```html
<script src="assets/js/ssh-terminal.js"></script>
```

This keeps `ssh-terminal.js` in the pre-CDN consecutive group so build.py captures it
in the first `re.sub` replacement. ssh-terminal has no Chart.js dependency.

### 4c — Enable nav item (lines 69–72)

Replace the disabled nav item:
```html
<div class="nav-item disabled" title="SSH Terminal (coming soon)">
  <i class="fa-solid fa-terminal"></i>
  <span class="nav-text">SSH Terminal</span>
</div>
```

With:
```html
<div class="nav-item" id="nav-ssh-terminal" title="SSH Terminal"
     onclick="navClick('ssh-terminal')">
  <i class="fa-solid fa-terminal"></i>
  <span class="nav-text">SSH Terminal</span>
  <i class="fa-solid fa-thumbtack pin-icon" id="pin-ssh-terminal"
     onclick="pinClick(event,'ssh-terminal')" title="Pin panel"></i>
</div>
```

### 4d — Add card HTML

After the closing `</div>` of the sysmonitor-card (which starts at line 134), add:
```html
<div class="card ssh-terminal-card" id="ssh-terminal-card">
  <div class="card-header">
    <div class="card-header-left">
      <i class="fa-solid fa-terminal card-icon"></i>
      <span class="card-title">SSH Terminal</span>
    </div>
  </div>
  <div class="ssh-terminal-connect-row">
    <input id="ssh-terminal-ip" class="ssh-terminal-ip-input"
           type="text" placeholder="192.168.1.x" autocomplete="off">
    <button id="ssh-terminal-connect-btn" class="ssh-terminal-btn primary"
            onclick="sshTermConnect()">Connect</button>
  </div>
  <div id="ssh-terminal-output" class="ssh-terminal-output"></div>
  <div class="ssh-terminal-cmd-row">
    <input id="ssh-terminal-cmd" class="ssh-terminal-cmd-input"
           type="text" placeholder="command" disabled
           onkeydown="sshTermHandleKey(event)" autocomplete="off">
    <button id="ssh-terminal-send-btn" class="ssh-terminal-btn primary"
            onclick="sshTermRun()" disabled>Run</button>
  </div>
  <div id="ssh-terminal-status" class="ssh-terminal-status"></div>
  <div id="ssh-terminal-error" class="ssh-terminal-error"></div>
</div>
```

---

## T5 — sidebar.js: register 'ssh-terminal' in pin system

**File:** `web/src/assets/js/sidebar.js`
**Rollback:** remove the three additions below

Three surgical edits, no other lines touched.

### 5a — sidebarPinned (line 4)

```javascript
// before
var sidebarPinned = { hostlist: true, scp: true, 'sys-monitor': false };

// after
var sidebarPinned = { hostlist: true, scp: true, 'sys-monitor': false, 'ssh-terminal': false };
```

### 5b — getPanelEl (after the `'sys-monitor'` branch)

```javascript
// before
  if (id === 'sys-monitor') return document.getElementById('sysmonitor-card');
  return null;

// after
  if (id === 'sys-monitor') return document.getElementById('sysmonitor-card');
  if (id === 'ssh-terminal') return document.getElementById('ssh-terminal-card');
  return null;
```

### 5c — applyPinState forEach array (line ~16)

```javascript
// before
  ['hostlist', 'scp', 'sys-monitor'].forEach(function(id) {

// after
  ['hostlist', 'scp', 'sys-monitor', 'ssh-terminal'].forEach(function(id) {
```

---

## T6 — scp.js: call syncSSHTabsFromSCP() at two call sites

**File:** `web/src/assets/js/scp.js`
**Rollback:** remove the two added lines

### 6a — End of onSourceChange() (line 158)

```javascript
// before (line 157–158)
    scpBrowse(scpPath);
  }
}

// after
    scpBrowse(scpPath);
    syncSSHTabsFromSCP();
  }
}
```

### 6b — End of target checkbox change listener (~line 100)

Locate the `cb.addEventListener('change', function() {` block. At its closing `});`, add the call:

```javascript
    scpUpdateDeploy();
    syncSSHTabsFromSCP();   // ← add this line
  });
```

`syncSSHTabsFromSCP()` is defined in ssh-terminal.js. It checks `if (!sshTermState) return` so it is safe to call before the SSH terminal panel is initialized.

---

## T7 — web.c: add single-quote strip in route_api_ssh_exec()

**File:** `src/web.c`
**Location:** line ~1398, before the deny-char loop
**Rollback:** remove the 4-line strip block

`route_api_ssh_exec()` at line 1371 already exists and is complete except for
single-quote sanitization. The command is placed inside single-quotes on the
sshpass invocation (line 1427: `root@%s '%s' 2>/tmp/...`). A bare `'` in the
command field would escape that context.

Add a strip loop immediately before the existing deny-char block at line 1398:

```c
/* Strip single-quotes (Option A): command is wrapped in single-quotes for ssh */
{
    char *src = command, *dst = command;
    while (*src) { if (*src != '\'') *dst++ = *src; src++; }
    *dst = '\0';
}

/* Reject obviously dangerous shell metacharacters */
const char *deny = ";&|`$(){}><";
```

The in-place strip rewrites `command[]` before it reaches either the deny check
or the snprintf. No allocation needed — reuses the existing stack buffer.

**Verify:** compile cleanly with `make` (no warnings on the new block).

---

## T8 — web.c: register /api/ssh-exec in POST dispatch

**File:** `src/web.c`
**Location:** line 1745–1746 (after the `/api/chmod` branch)
**Rollback:** remove the added `else if` line

```c
// before
            else if (strcmp(path, "/api/chmod") == 0)
                route_api_chmod(fd, req);
            else
                route_404(fd);

// after
            else if (strcmp(path, "/api/chmod") == 0)
                route_api_chmod(fd, req);
            else if (strcmp(path, "/api/ssh-exec") == 0)
                route_api_ssh_exec(fd, req);
            else
                route_404(fd);
```

---

## T9 — Build and deploy verification

```bash
# 1. Rebuild inlined HTML
python3 web/build.py

# 2. Compile (native for quick check)
make

# 3. Deploy to target board
make deploy    # Pi 1
make deploy2   # Pi 2
make deploy3   # Orange Pi

# 4. Manual browser test (see tests.md)
```

**Binary sanity check before deploy:**
```bash
grep -c "ssh-terminal" web/scanner.html   # must be > 0
grep -c "sshTermRun"   web/scanner.html   # must be > 0
```

---

## Task order summary

```
T1 (build.py)  →  T2 (css)  →  T3 (js)  →  T4 (scanner.html)
                                           →  T5 (sidebar.js)
                                           →  T6 (scp.js)
T7 (web.c sanitize)  →  T8 (web.c dispatch)  →  T9 (build+deploy)
```

T1–T3 have no dependencies. T4–T6 depend on T1–T3 (files must exist
before scanner.html references them). T7–T8 are independent of all JS tasks.
T9 is the final gate.
