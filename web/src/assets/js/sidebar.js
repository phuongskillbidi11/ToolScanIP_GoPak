/* ── Sidebar ─────────────────────────────────────────────────────────────── */

/* pin state: which panels are currently shown */
var sidebarPinned = { hostlist: true, scp: true, 'sys-monitor': false, 'ssh-terminal': false };
var sidebarActive = 'hostlist';

/* Map panel id → the DOM element to show/hide */
function getPanelEl(id) {
  if (id === 'hostlist') return document.getElementById('hostlist-body').closest('.card');
  if (id === 'scp')      return document.getElementById('scp-card');
  if (id === 'sys-monitor') return document.getElementById('sysmonitor-card');
  if (id === 'ssh-terminal') return document.getElementById('ssh-terminal-card');
  return null;
}

function applyPinState() {
  ['hostlist', 'scp', 'sys-monitor', 'ssh-terminal'].forEach(function(id) {
    var el  = getPanelEl(id);
    var pin = document.getElementById('pin-' + id);
    if (!el) return;
    el.style.display = sidebarPinned[id] ? '' : 'none';
    if (pin) {
      if (sidebarPinned[id]) pin.classList.add('pinned');
      else                   pin.classList.remove('pinned');
    }
  });

  if (typeof sysMonitorSetPinned === 'function') {
    sysMonitorSetPinned(!!sidebarPinned['sys-monitor']);
  }
  if (typeof sshTermSetPinned === 'function') {
    sshTermSetPinned(!!sidebarPinned['ssh-terminal']);
  }
}

function navClick(id) {
  /* toggle pin on click */
  sidebarPinned[id] = !sidebarPinned[id];
  /* always keep at least one panel visible */
  var anyPinned = Object.keys(sidebarPinned).some(function(k){ return sidebarPinned[k]; });
  if (!anyPinned) sidebarPinned[id] = true;
  sidebarActive = id;
  /* update active highlight */
  document.querySelectorAll('.nav-item').forEach(function(el){ el.classList.remove('active'); });
  var nav = document.getElementById('nav-' + id);
  if (nav) nav.classList.add('active');
  applyPinState();
}

function pinClick(e, id) {
  e.stopPropagation();   /* don't trigger navClick */
  sidebarPinned[id] = !sidebarPinned[id];
  var anyPinned = Object.keys(sidebarPinned).some(function(k){ return sidebarPinned[k]; });
  if (!anyPinned) sidebarPinned[id] = true;
  applyPinState();
}

function toggleSidebar() {
  var sb   = document.getElementById('sidebar');
  var icon = document.getElementById('sidebar-toggle-icon');
  sb.classList.toggle('collapsed');
  var collapsed = sb.classList.contains('collapsed');
  icon.className = collapsed
    ? 'fa-solid fa-angles-right'
    : 'fa-solid fa-angles-left';
}

/* Apply default state on load (both panels visible) */
applyPinState();

var scpPath          = '/root';
var scpSelected      = new Set();   /* set of absolute paths */
var scpCurrentEntries = [];         /* last fetched entries for re-sort */
var scpCurrentPath    = '/root';
var scpSortKey        = 'name';
var scpSortAsc        = true;
var scpMode      = 'multi-target';
var scpMode2IP   = '';
var scpMode2Path = '/root';
var scpMode2Dirs = new Set();   /* selected destination directories (mode 2) */

/* ── Target panel state ── */
var scpTgtIP   = '';
var scpTgtPath = '/root';
var scpTgtSel  = new Set();

function toggleSCPPanel() {
  scpOpen = !scpOpen;
  document.getElementById('scp-body').style.display = scpOpen ? '' : 'none';
  document.getElementById('scp-chevron').style.transform = scpOpen ? 'rotate(180deg)' : '';
  if (scpOpen) {
    scpPopulateSources();
    scpPopulateTargets();
  }
}
