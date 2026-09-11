/* ── Service Logs panel ─────────────────────────────────────────────────── */

var svcLogsInterval = null;
var svcLogsAutoRefresh = true;

function svcLogsSetPinned(on) {
  if (!on) {
    stopSvcLogsAutoRefresh();
  } else {
    initServiceLogs();
  }
}

function initServiceLogs() {
  fetchServiceLogs();
  if (svcLogsAutoRefresh) startSvcLogsAutoRefresh();
}

function fetchServiceLogs() {
  fetch('/api/service-logs?lines=100')
    .then(function(r) { return r.json(); })
    .then(function(data) { renderSvcLogs(data); })
    .catch(function(e) {
      var out = document.getElementById('svc-logs-output');
      if (out) out.innerHTML = '<div class="svc-log-line error">[fetch error] ' + escHtml(String(e)) + '</div>';
    });
}

function renderSvcLogs(data) {
  var out    = document.getElementById('svc-logs-output');
  var badge  = document.getElementById('svc-status-dot');
  var label  = document.getElementById('svc-status-label');
  var rcount = document.getElementById('svc-restart-count');
  if (!out) return;

  var status = data.status || 'inactive';
  var knownStatuses = ['active', 'inactive', 'failed'];
  var statusClass = (knownStatuses.indexOf(status) !== -1) ? status : 'inactive';
  if (badge)  { badge.className  = 'svc-status-dot ' + statusClass; }
  if (label)  { label.className  = 'svc-status-label ' + statusClass; label.textContent = status; }
  if (rcount) { rcount.textContent = (data.restart_count >= 0) ? data.restart_count : '—'; }

  var lines = data.lines || [];
  if (!lines.length) {
    out.innerHTML = '<div class="svc-logs-empty">No log entries found for ipscanner.service</div>';
    return;
  }
  out.innerHTML = lines.map(function(l) {
    return '<div class="svc-log-line ' + escHtml(l.level || 'info') + '">' +
      escHtml(l.ts + '  ' + l.msg) + '</div>';
  }).join('');
  out.scrollTop = out.scrollHeight;
}

function startSvcLogsAutoRefresh() {
  if (svcLogsInterval) return;
  svcLogsInterval = setInterval(fetchServiceLogs, 10000);
}

function stopSvcLogsAutoRefresh() {
  if (svcLogsInterval) { clearInterval(svcLogsInterval); svcLogsInterval = null; }
}

function toggleSvcLogsAutoRefresh() {
  svcLogsAutoRefresh = !svcLogsAutoRefresh;
  var btn = document.getElementById('svc-autorefresh-btn');
  if (svcLogsAutoRefresh) {
    startSvcLogsAutoRefresh();
    if (btn) { btn.className = 'svc-logs-btn active-refresh'; btn.innerHTML = '<i class="fa-solid fa-rotate"></i> Auto-refresh: ON'; }
  } else {
    stopSvcLogsAutoRefresh();
    if (btn) { btn.className = 'svc-logs-btn'; btn.innerHTML = '<i class="fa-solid fa-rotate"></i> Auto-refresh: OFF'; }
  }
}

function svcLogsRefresh() {
  fetchServiceLogs();
}
