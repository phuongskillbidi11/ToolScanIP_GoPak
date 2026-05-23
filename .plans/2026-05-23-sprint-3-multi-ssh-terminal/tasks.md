# Multi-IP SSH Terminal — tasks.md

Executor: implement tasks in order. One task = one atomic commit.
Mark each `[x]` when done. Log any error encountered to `errors.log`.

---

## Rollback map

| File | Rollback action |
|---|---|
| `web/src/assets/js/ssh-terminal.js` | Restore Sprint 2 version from git (`git checkout HEAD~1 -- web/src/assets/js/ssh-terminal.js`) |
| `web/src/assets/css/ssh-terminal.css` | Remove tab strip rules (`.ssh-terminal-tabs`, `.ssh-terminal-tab`, `.ssh-terminal-tab-close`, `.ssh-terminal-ip-sel`) |
| `web/src/scanner.html` | Revert connect row and tab strip div to Sprint 2 card HTML |

---

## [x] T1 — scanner.html: update card HTML

**File:** `web/src/scanner.html`
**Location:** Lines 227–250 (the `ssh-terminal-card` div)

Replace the connect row and add the tab strip div. Do not touch the output div,
cmd row, status, or error divs — they are unchanged.

### 1a — Replace connect row

Current (Sprint 2):
```html
<div class="ssh-terminal-connect-row">
  <input id="ssh-terminal-ip" class="ssh-terminal-ip-input"
         type="text" placeholder="192.168.1.x" autocomplete="off">
  <button id="ssh-terminal-connect-btn" class="ssh-terminal-btn primary"
          onclick="sshTermConnect()">Connect</button>
</div>
```

Replace with:
```html
<div class="ssh-terminal-connect-row">
  <select id="ssh-terminal-ip-sel" class="ssh-terminal-ip-sel">
    <option value="">— select device —</option>
  </select>
  <input id="ssh-terminal-ip-custom" class="ssh-terminal-ip-input"
         type="text" placeholder="custom IP" autocomplete="off"
         style="display:none">
  <button class="ssh-terminal-btn primary" onclick="sshTermAddSession()">Add</button>
</div>
```

### 1b — Add tab strip div

Immediately after the closing `</div>` of the connect row, insert:
```html
<div id="ssh-terminal-tabs" class="ssh-terminal-tabs"></div>
```

**Verify:**
```bash
grep -n "ssh-terminal-ip-sel\|ssh-terminal-tabs" web/src/scanner.html
# must return 2 lines
```

---

## [x] T2 — ssh-terminal.css: add tab strip and selector styles

**File:** `web/src/assets/css/ssh-terminal.css`
**Change type:** Append new rules — do not remove any existing rules

Append to the end of the file:

```css
.ssh-terminal-ip-sel {
  flex: 1;
  font-size: 13px;
  border: 1px solid #d1d5db;
  border-radius: 6px;
  padding: 5px 8px;
  min-width: 0;
}

.ssh-terminal-tabs {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
  padding: 8px 14px 0;
  border-bottom: 1px solid #e5e7eb;
  min-height: 36px;
}

.ssh-terminal-tabs:empty {
  display: none;
}

.ssh-terminal-tab {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 4px 10px;
  border-radius: 6px 6px 0 0;
  font-size: 12px;
  font-family: 'Courier New', monospace;
  border: 1px solid #e5e7eb;
  border-bottom: none;
  background: #f3f4f6;
  cursor: pointer;
  white-space: nowrap;
  color: #6b7280;
}

.ssh-terminal-tab.active {
  background: #1e1e1e;
  border-color: #374151;
  color: #d4d4d4;
}

.ssh-terminal-tab-close {
  font-size: 14px;
  line-height: 1;
  opacity: .6;
  padding: 0 2px;
}

.ssh-terminal-tab-close:hover {
  opacity: 1;
}
```

**Verify:**
```bash
grep -c "ssh-terminal-tab" web/src/assets/css/ssh-terminal.css
# must return ≥ 5
```

---

## [x] T3 — ssh-terminal.js: full rewrite

**File:** `web/src/assets/js/ssh-terminal.js`
**Change type:** Full replacement

Replace the entire file with the implementation below.

Key invariants that must be preserved:
- `var sshTermSessions` must be `var` (not `let`/`const`) — hoisting guard depends on it
- `sshTermSetPinned(on)` must remain defined — called by sidebar.js line 31
- `syncSSHTabsFromSCP()` must remain defined — called by scp.js lines 158 and ~100
- `sshTermHandleKey(e)` must remain defined — referenced in scanner.html `onkeydown`
- Every function callable before this file's vars initialize must guard with
  `if (!sshTermSessions) return`

```javascript
var sshTermSessions  = [];   /* [{ ip, history, inFlight }] */
var sshTermActiveIdx = -1;

function sshTermEl(id) { return document.getElementById(id); }

function sshTermValidIP(ip) {
  return /^\d{1,3}(\.\d{1,3}){3}$/.test(ip);
}

/* ── Selector ──────────────────────────────────────────────────────────── */

function sshTermPopulateSel() {
  var sel = sshTermEl('ssh-terminal-ip-sel');
  if (!sel) return;
  var prev = sel.value;
  sel.innerHTML = '<option value="">— select device —</option>';
  if (typeof allHosts !== 'undefined') {
    allHosts.forEach(function(h) {
      var opt = document.createElement('option');
      opt.value = h.ip;
      opt.textContent = h.ip
        + (h.comment  ? '  — ' + h.comment : '')
        + (h.hostname && h.hostname !== h.ip ? '  [' + h.hostname + ']' : '');
      sel.appendChild(opt);
    });
  }
  var cust = document.createElement('option');
  cust.value = '__custom__';
  cust.textContent = '— custom IP… —';
  sel.appendChild(cust);
  if (prev) sel.value = prev;
}

function sshTermSelChange() {
  var sel    = sshTermEl('ssh-terminal-ip-sel');
  var custom = sshTermEl('ssh-terminal-ip-custom');
  if (!sel || !custom) return;
  custom.style.display = sel.value === '__custom__' ? '' : 'none';
  if (sel.value !== '__custom__') custom.value = '';
}

/* ── Sessions ──────────────────────────────────────────────────────────── */

function sshTermAddSession() {
  var sel    = sshTermEl('ssh-terminal-ip-sel');
  var custom = sshTermEl('ssh-terminal-ip-custom');
  var ip = sel && sel.value === '__custom__'
         ? (custom ? custom.value.trim() : '')
         : (sel ? sel.value : '');
  if (!ip || ip === '__custom__') {
    sshTermSetError('Select a device or enter a custom IP.');
    return;
  }
  if (!sshTermValidIP(ip)) {
    sshTermSetError('Invalid IP address: ' + ip);
    return;
  }
  /* Activate existing tab if already open */
  for (var i = 0; i < sshTermSessions.length; i++) {
    if (sshTermSessions[i].ip === ip) { sshTermActivate(i); return; }
  }
  sshTermSessions.push({ ip: ip, history: '', inFlight: false });
  sshTermActiveIdx = sshTermSessions.length - 1;
  sshTermSetError('');
  sshTermRenderTabs();
  sshTermRenderOutput();
  sshTermUpdateUI();
}

function sshTermClose(e, idx) {
  e.stopPropagation();
  sshTermSessions.splice(idx, 1);
  if (!sshTermSessions.length) {
    sshTermActiveIdx = -1;
  } else {
    sshTermActiveIdx = Math.min(idx, sshTermSessions.length - 1);
  }
  sshTermRenderTabs();
  sshTermRenderOutput();
  sshTermUpdateUI();
}

function sshTermActivate(idx) {
  sshTermActiveIdx = idx;
  sshTermRenderTabs();
  sshTermRenderOutput();
  sshTermUpdateUI();
}

/* ── Render ────────────────────────────────────────────────────────────── */

function sshTermRenderTabs() {
  var wrap = sshTermEl('ssh-terminal-tabs');
  if (!wrap) return;
  wrap.innerHTML = '';
  sshTermSessions.forEach(function(s, i) {
    var tab = document.createElement('div');
    tab.className = 'ssh-terminal-tab' + (i === sshTermActiveIdx ? ' active' : '');
    tab.onclick = function() { sshTermActivate(i); };
    tab.textContent = s.ip;
    var x = document.createElement('span');
    x.className = 'ssh-terminal-tab-close';
    x.textContent = '×';
    x.onclick = function(e) { sshTermClose(e, i); };
    tab.appendChild(x);
    wrap.appendChild(tab);
  });
}

function sshTermRenderOutput() {
  var out = sshTermEl('ssh-terminal-output');
  if (!out) return;
  if (sshTermActiveIdx === -1 || !sshTermSessions.length) {
    out.textContent = '';
    return;
  }
  out.textContent = sshTermSessions[sshTermActiveIdx].history;
  out.scrollTop = out.scrollHeight;
}

function sshTermUpdateUI() {
  var sendBtn  = sshTermEl('ssh-terminal-send-btn');
  var cmdInput = sshTermEl('ssh-terminal-cmd');
  var active   = sshTermActiveIdx !== -1 && sshTermSessions.length > 0;
  var inFlight = active && sshTermSessions[sshTermActiveIdx].inFlight;
  if (sendBtn)  sendBtn.disabled  = !active || inFlight;
  if (cmdInput) cmdInput.disabled = !active;
}

function sshTermSetStatus(msg) {
  var el = sshTermEl('ssh-terminal-status');
  if (el) el.textContent = msg || '';
}

function sshTermSetError(msg) {
  var el = sshTermEl('ssh-terminal-error');
  if (el) el.textContent = msg || '';
}

/* ── Command execution ─────────────────────────────────────────────────── */

function sshTermRun() {
  if (!sshTermSessions) return;   /* hoisting guard */
  if (sshTermActiveIdx === -1 || !sshTermSessions.length) return;
  var session = sshTermSessions[sshTermActiveIdx];
  if (session.inFlight) return;
  var cmdInput = sshTermEl('ssh-terminal-cmd');
  var cmd = cmdInput ? cmdInput.value.trim() : '';
  if (!cmd) return;
  if (cmdInput) cmdInput.value = '';

  session.history += '$ ' + cmd + '\n';
  sshTermRenderOutput();
  session.inFlight = true;
  sshTermSetError('');
  sshTermUpdateUI();

  fetch('/api/ssh-exec', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ip: session.ip, command: cmd }),
    cache: 'no-store'
  })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (data.error) {
        session.history += '[error] ' + data.error + '\n';
      } else {
        if (data.stdout) session.history += data.stdout;
        if (data.stderr) session.history += '[stderr] ' + data.stderr;
        if (!data.stdout && !data.stderr) session.history += '(no output)\n';
      }
      /* Cap history at 50 KB to prevent unbounded growth */
      if (session.history.length > 51200)
        session.history = session.history.slice(session.history.length - 51200);
      sshTermSetStatus('exit ' + data.exit_code + '  (' + data.elapsed_ms + 'ms)');
      sshTermRenderOutput();
    })
    .catch(function(err) {
      session.history += '[request failed] ' + err + '\n';
      sshTermSetError('Request failed: ' + err);
      sshTermRenderOutput();
    })
    .finally(function() {
      session.inFlight = false;
      sshTermUpdateUI();
    });
}

function sshTermHandleKey(e) {
  if (e.key === 'Enter') sshTermRun();
}

/* ── SCP sync ──────────────────────────────────────────────────────────── */

function syncSSHTabsFromSCP() {
  if (!sshTermSessions) return;   /* hoisting guard */
  var sel = sshTermEl('ssh-terminal-ip-sel');
  if (!sel) return;
  var ip = (typeof scpSourceIP !== 'undefined' && scpSourceIP && scpSourceIP !== 'LOCAL')
         ? scpSourceIP : '';
  sel.value = ip;
  if (sel.value !== ip) sel.value = '';   /* option not in list — reset */
  sshTermSelChange();
}

/* ── Sidebar integration ───────────────────────────────────────────────── */

function sshTermSetPinned(on) {
  if (!sshTermSessions) return;   /* hoisting guard */
  if (on) sshTermPopulateSel();   /* refresh selector when panel is shown */
}
```

**Verify:**
```bash
node --check web/src/assets/js/ssh-terminal.js && echo "syntax OK"
grep -c "hoisting guard" web/src/assets/js/ssh-terminal.js
# must return 3 (one per externally-callable function)
```

---

## [x] T4 — Build and deploy verification

```bash
# Rebuild inlined HTML
python3 web/build.py

# Compile and deploy
make deploy2   # or deploy / deploy3 for other targets

# Content checks
grep -c "sshTermAddSession\|ssh-terminal-tabs\|ssh-terminal-ip-sel" web/scanner.html
# must return ≥ 3

# Restart process on board, then run browser tests from tests.md
```

---

## Task order summary

```
T1 (scanner.html HTML) → T2 (css) → T3 (js rewrite) → T4 (build + deploy)
```

T1 and T2 have no dependencies on each other and can be done in parallel.
T3 depends on understanding the final HTML structure (T1).
T4 gates everything.
