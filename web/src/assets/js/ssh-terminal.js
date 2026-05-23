var sshTermSessions  = [];   /* [{ ip, history, inFlight }] */
var sshTermActiveIdx = -1;

function sshTermEl(id) { return document.getElementById(id); }

function sshTermValidIP(ip) {
  return /^\d{1,3}(\.\d{1,3}){3}$/.test(ip);
}

/* -- Selector -------------------------------------------------------------- */

function sshTermPopulateSel() {
  var sel = sshTermEl('ssh-terminal-ip-sel');
  if (!sel) return;
  var prev = sel.value;
  sel.innerHTML = '<option value="">-- select device --</option>';
  if (typeof allHosts !== 'undefined') {
    allHosts.forEach(function(h) {
      var opt = document.createElement('option');
      opt.value = h.ip;
      opt.textContent = h.ip
        + (h.comment  ? '  -- ' + h.comment : '')
        + (h.hostname && h.hostname !== h.ip ? '  [' + h.hostname + ']' : '');
      sel.appendChild(opt);
    });
  }
  var cust = document.createElement('option');
  cust.value = '__custom__';
  cust.textContent = '-- custom IP... --';
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

/* -- Sessions -------------------------------------------------------------- */

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
  } else if (idx === sshTermActiveIdx) {
    sshTermActiveIdx = Math.min(idx, sshTermSessions.length - 1);
  } else if (idx < sshTermActiveIdx) {
    sshTermActiveIdx -= 1;
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

/* -- Render ---------------------------------------------------------------- */

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
    x.textContent = 'x';
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

/* -- Command execution ----------------------------------------------------- */

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

/* -- SCP sync -------------------------------------------------------------- */

function syncSSHTabsFromSCP() {
  if (!sshTermSessions) return;   /* hoisting guard */
  var sel = sshTermEl('ssh-terminal-ip-sel');
  if (!sel) return;
  if (sshTermActiveIdx !== -1 && sshTermSessions.length) return;
  var ip = (typeof scpSourceIP !== 'undefined' && scpSourceIP && scpSourceIP !== 'LOCAL')
         ? scpSourceIP : '';
  sel.value = ip;
  if (sel.value !== ip) sel.value = '';   /* option not in list -- reset */
  sshTermSelChange();
}

/* -- Sidebar integration --------------------------------------------------- */

function sshTermSetPinned(on) {
  if (!sshTermSessions) return;   /* hoisting guard */
  if (on) sshTermPopulateSel();   /* refresh selector when panel is shown */
}
