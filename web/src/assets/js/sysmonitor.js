var sysMonitorState = {
  running: false,
  timer: null,
  inFlight: false,
  token: 0,
  loadChart: null,
  tempChart: null
};

var SYSMON_MAX_POINTS = 20;

function sysMonitorEl(id) {
  return document.getElementById(id);
}

function sysMonitorSetBadge(id, text, cls) {
  var el = sysMonitorEl(id);
  if (!el) return;
  el.className = 'sysmonitor-pill' + (cls ? ' ' + cls : '');
  el.textContent = text;
}

function sysMonitorFormatBytes(bytes) {
  if (!bytes && bytes !== 0) return '--';
  var units = ['B', 'KB', 'MB', 'GB', 'TB'];
  var n = Number(bytes);
  var i = 0;
  while (n >= 1024 && i < units.length - 1) {
    n /= 1024;
    i++;
  }
  return (i === 0 ? n.toFixed(0) : n.toFixed(1)) + ' ' + units[i];
}

function sysMonitorFormatPercent(v) {
  if (typeof v !== 'number' || !isFinite(v)) return '--';
  return v.toFixed(1) + '%';
}

function sysMonitorFormatTemp(v) {
  if (typeof v !== 'number' || !isFinite(v) || v <= 0) return '--';
  return v.toFixed(1) + '\u00b0C';
}

function sysMonitorFormatUptime(seconds) {
  seconds = Math.max(0, Math.floor(seconds || 0));
  var days = Math.floor(seconds / 86400);
  seconds -= days * 86400;
  var hours = Math.floor(seconds / 3600);
  seconds -= hours * 3600;
  var mins = Math.floor(seconds / 60);
  var parts = [];
  if (days) parts.push(days + 'd');
  if (hours || parts.length) parts.push(hours + 'h');
  parts.push(mins + 'm');
  return parts.join(' ');
}

function sysMonitorResetCharts() {
  if (sysMonitorState.loadChart) {
    sysMonitorState.loadChart.data.labels = [];
    sysMonitorState.loadChart.data.datasets.forEach(function(ds) { ds.data = []; });
    sysMonitorState.loadChart.update('none');
  }
  if (sysMonitorState.tempChart) {
    sysMonitorState.tempChart.data.labels = [];
    sysMonitorState.tempChart.data.datasets.forEach(function(ds) { ds.data = []; });
    sysMonitorState.tempChart.update('none');
  }
}

function sysMonitorEnsureCharts() {
  if (!window.Chart) return;

  var loadCanvas = sysMonitorEl('sysmonitor-load-chart');
  var tempCanvas = sysMonitorEl('sysmonitor-temp-chart');
  if (!loadCanvas || !tempCanvas) return;

  if (!sysMonitorState.loadChart) {
    sysMonitorState.loadChart = new Chart(loadCanvas.getContext('2d'), {
      type: 'line',
      data: {
        labels: [],
        datasets: [
          {
            label: 'CPU %',
            data: [],
            borderColor: '#2563eb',
            backgroundColor: 'rgba(37, 99, 235, .12)',
            tension: .35,
            pointRadius: 0,
            borderWidth: 2,
            fill: false
          },
          {
            label: 'RAM %',
            data: [],
            borderColor: '#16a34a',
            backgroundColor: 'rgba(22, 163, 74, .12)',
            tension: .35,
            pointRadius: 0,
            borderWidth: 2,
            fill: false
          }
        ]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: {
          legend: {
            position: 'bottom',
            labels: { usePointStyle: true, boxWidth: 8, boxHeight: 8 }
          }
        },
        scales: {
          x: { grid: { color: 'rgba(229,231,235,.4)' } },
          y: {
            beginAtZero: true,
            suggestedMax: 100,
            ticks: { callback: function(v) { return v + '%'; } },
            grid: { color: 'rgba(229,231,235,.7)' }
          }
        }
      }
    });
  }

  if (!sysMonitorState.tempChart) {
    sysMonitorState.tempChart = new Chart(tempCanvas.getContext('2d'), {
      type: 'line',
      data: {
        labels: [],
        datasets: [
          {
            label: 'Temp °C',
            data: [],
            borderColor: '#ea580c',
            backgroundColor: 'rgba(234, 88, 12, .12)',
            tension: .35,
            pointRadius: 0,
            borderWidth: 2,
            fill: false
          }
        ]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        animation: false,
        plugins: {
          legend: {
            position: 'bottom',
            labels: { usePointStyle: true, boxWidth: 8, boxHeight: 8 }
          }
        },
        scales: {
          x: { grid: { color: 'rgba(229,231,235,.4)' } },
          y: {
            beginAtZero: true,
            ticks: { callback: function(v) { return v + '\u00b0'; } },
            grid: { color: 'rgba(229,231,235,.7)' }
          }
        }
      }
    });
  }
}

function sysMonitorPushPoint(label, cpuPct, ramPct, tempC) {
  if (sysMonitorState.loadChart) {
    var loadChart = sysMonitorState.loadChart;
    loadChart.data.labels.push(label);
    loadChart.data.datasets[0].data.push(cpuPct);
    loadChart.data.datasets[1].data.push(ramPct);
    while (loadChart.data.labels.length > SYSMON_MAX_POINTS) {
      loadChart.data.labels.shift();
      loadChart.data.datasets[0].data.shift();
      loadChart.data.datasets[1].data.shift();
    }
    loadChart.update('none');
  }

  if (sysMonitorState.tempChart) {
    var tempChart = sysMonitorState.tempChart;
    tempChart.data.labels.push(label);
    tempChart.data.datasets[0].data.push(tempC > 0 ? tempC : null);
    while (tempChart.data.labels.length > SYSMON_MAX_POINTS) {
      tempChart.data.labels.shift();
      tempChart.data.datasets[0].data.shift();
    }
    tempChart.update('none');
  }
}

function sysMonitorRenderProcesses(processes) {
  var tbody = sysMonitorEl('sysmonitor-proc-body');
  if (!tbody) return;

  tbody.innerHTML = '';
  if (!processes || !processes.length) {
    var tr = document.createElement('tr');
    tr.innerHTML = '<td colspan="4"><div class="sysmonitor-empty">No process data</div></td>';
    tbody.appendChild(tr);
    return;
  }

  processes.forEach(function(p) {
    var tr = document.createElement('tr');
    tr.innerHTML =
      '<td class="sysmonitor-pid">' + escHtml(p.pid) + '</td>' +
      '<td class="sysmonitor-name">' + escHtml(p.name || '—') + '</td>' +
      '<td class="sysmonitor-cpu">' + escHtml(sysMonitorFormatPercent(p.cpu_pct)) + '</td>' +
      '<td class="sysmonitor-state">' + escHtml(p.state || '—') + '</td>';
    tbody.appendChild(tr);
  });
}

function sysMonitorRender(data) {
  if (!data) return;

  var cpuPct = typeof data.cpu_pct === 'number' ? data.cpu_pct : 0;
  var memPct = data.mem && typeof data.mem.percent === 'number' ? data.mem.percent : 0;
  var tempC = typeof data.temp_c === 'number' ? data.temp_c : 0;
  var ts = data.timestamp || '';
  var label = ts && ts.length >= 19 ? ts.slice(11, 19) : new Date().toTimeString().slice(0, 8);
  var err = sysMonitorEl('sysmonitor-error');
  if (err) err.textContent = '';

  var uptime = sysMonitorEl('sysmonitor-uptime');
  if (uptime) uptime.textContent = 'Uptime: ' + sysMonitorFormatUptime(data.uptime_sec);

  var cpuEl = sysMonitorEl('sysmonitor-cpu');
  if (cpuEl) cpuEl.textContent = sysMonitorFormatPercent(cpuPct);

  var ramEl = sysMonitorEl('sysmonitor-ram');
  if (ramEl && data.mem) {
    ramEl.textContent = sysMonitorFormatBytes(data.mem.used) + ' / ' + sysMonitorFormatBytes(data.mem.total);
  }

  var diskEl = sysMonitorEl('sysmonitor-disk');
  if (diskEl && data.disk) {
    diskEl.textContent = sysMonitorFormatBytes(data.disk.used) + ' / ' + sysMonitorFormatBytes(data.disk.total);
  }

  var tempEl = sysMonitorEl('sysmonitor-temp');
  if (tempEl) tempEl.textContent = sysMonitorFormatTemp(tempC);

  var meta = sysMonitorEl('sysmonitor-proc-count');
  if (meta) meta.textContent = (data.process_count || 0) + ' tracked';

  sysMonitorRenderProcesses(data.processes || []);
  sysMonitorPushPoint(label, cpuPct, memPct, tempC);
  sysMonitorSetBadge('sysmonitor-status', 'Live', 'good');
}

function sysMonitorFetch() {
  if (!sysMonitorState.running || sysMonitorState.inFlight) return;

  var token = sysMonitorState.token;
  sysMonitorState.inFlight = true;
  sysMonitorSetBadge('sysmonitor-status', 'Updating...', 'warn');

  fetch('/api/sys-stats', { cache: 'no-store' })
    .then(function(r) { return r.json(); })
    .then(function(data) {
      if (token !== sysMonitorState.token || !sysMonitorState.running) return;
      sysMonitorEnsureCharts();
      sysMonitorRender(data);
    })
    .catch(function(err) {
      if (token !== sysMonitorState.token || !sysMonitorState.running) return;
      sysMonitorSetBadge('sysmonitor-status', 'Error', 'warn');
      var note = sysMonitorEl('sysmonitor-error');
      if (note) note.textContent = 'Update failed: ' + err;
    })
    .finally(function() {
      if (token === sysMonitorState.token) sysMonitorState.inFlight = false;
    });
}

function sysMonitorStart() {
  if (sysMonitorState.timer) return;

  sysMonitorState.running = true;
  sysMonitorState.token++;
  sysMonitorState.inFlight = false;
  sysMonitorEnsureCharts();
  sysMonitorResetCharts();
  var err = sysMonitorEl('sysmonitor-error');
  if (err) err.textContent = '';
  sysMonitorSetBadge('sysmonitor-status', 'Updating...', 'warn');
  sysMonitorFetch();
  sysMonitorState.timer = setInterval(sysMonitorFetch, 3000);
}

function sysMonitorStop() {
  sysMonitorState.running = false;
  sysMonitorState.token++;
  sysMonitorState.inFlight = false;
  if (sysMonitorState.timer) {
    clearInterval(sysMonitorState.timer);
    sysMonitorState.timer = null;
  }
  sysMonitorSetBadge('sysmonitor-status', 'Paused', 'idle');
}

function sysMonitorSetPinned(on) {
  if (!sysMonitorState) return;   /* hoisting guard: var not yet initialized */
  if (on) {
    if (!sysMonitorState.running) sysMonitorStart();
  } else {
    if (sysMonitorState.running) sysMonitorStop();
  }
}

if (typeof sysMonitorSetPinned === 'function') {
  sysMonitorSetPinned(typeof sidebarPinned !== 'undefined' && !!sidebarPinned['sys-monitor']);
}
