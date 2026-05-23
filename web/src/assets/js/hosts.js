var allHosts    = [];
var selectedIdx = -1;

function renderTable(hosts) {
  var tbody = document.getElementById('tbody');
  tbody.innerHTML = '';

  if (!hosts.length) {
    var tr = document.createElement('tr');
    tr.className = 'no-results';
    tr.innerHTML = '<td colspan="8"><i class="fa-solid fa-magnifying-glass" style="margin-right:6px"></i>No hosts match your search</td>';
    tbody.appendChild(tr);
    return;
  }

  hosts.forEach(function(h, i) {
    var lk = isLuckfox(h.comment);
    var tr = document.createElement('tr');
    var editKey = h.mac && h.mac !== '—' && h.mac !== '' ? h.mac : h.ip;
    var csrc = h.csrc || 'none';

    /* source badge */
    var srcBadge = '';
    if (csrc === 'mqtt') {
      srcBadge = '<span class="src-badge src-badge-mqtt"><i class="fa-solid fa-tower-broadcast"></i>MQTT</span>';
    } else if (csrc === 'manual') {
      srcBadge = '<span class="src-badge src-badge-manual"><i class="fa-solid fa-pen-to-square"></i>Manual</span>';
    } else if (csrc === 'override') {
      srcBadge = '<span class="src-badge src-badge-override"><i class="fa-solid fa-pen-to-square"></i>Manual&#8593;</span>';
    }

    /* action buttons depend on source */
    var actionBtns = '<button class="btn-icon btn-icon-edit" title="Edit comment (saves to manual)" onclick="editRow(event,\'' + escAttr(editKey) + '\',\'' + escAttr(h.comment) + '\')">' +
      '<i class="fa-solid fa-pen"></i></button>';
    if (h.comment) {
      if (csrc === 'mqtt') {
        /* MQTT-only: no delete (read-only label); offer Pin to manual */
        actionBtns += '<button class="btn-icon" title="Pin as manual comment" style="color:#2563eb" onclick="pinMqtt(event,\'' + escAttr(editKey) + '\',\'' + escAttr(h.comment) + '\')">' +
          '<i class="fa-solid fa-thumbtack"></i></button>';
      } else if (csrc === 'override') {
        /* Manual overrides MQTT: can unpin (delete manual entry, revert to MQTT) or fully delete */
        actionBtns += '<button class="btn-icon" title="Unpin manual override (revert to MQTT label)" style="color:#ea580c" onclick="unpinRow(event,\'' + escAttr(editKey) + '\')">' +
          '<i class="fa-solid fa-thumbtack" style="text-decoration:line-through"></i></button>';
        actionBtns += '<button class="btn-icon btn-icon-del" title="Delete manual comment" onclick="delRow(event,\'' + escAttr(editKey) + '\',\'manual\')">' +
          '<i class="fa-solid fa-trash"></i></button>';
      } else {
        /* manual or none: normal delete */
        actionBtns += '<button class="btn-icon btn-icon-del" title="Delete comment" onclick="delRow(event,\'' + escAttr(editKey) + '\',\'' + csrc + '\')">' +
          '<i class="fa-solid fa-trash"></i></button>';
      }
    }

    tr.innerHTML =
      '<td class="td-num">' + (i+1) + '</td>' +
      '<td><span class="status-dot"></span></td>' +
      '<td class="td-hostname"><i class="fa-solid fa-computer" style="color:#d1d5db;margin-right:6px"></i>' + escHtml(h.hostname) + '</td>' +
      '<td class="td-ip">' + escHtml(h.ip) + '</td>' +
      '<td class="td-vendor">' + escHtml(h.vendor) + '</td>' +
      '<td class="td-mac">' + escHtml(h.mac) + '</td>' +
      '<td>' +
        (h.comment
          ? '<span class="tag-comment"><i class="fa-solid fa-tag"></i>' + escHtml(h.comment) + '</span>'
          : '<span style="color:#d1d5db">—</span>') +
        srcBadge +
        (lk ? '<span class="tag-luckfox"><i class="fa-solid fa-bolt"></i>luckfox</span>' : '') +
      '</td>' +
      '<td class="td-actions">' + actionBtns + '</td>';

    tr.onclick = function() { selectRow(i, h, tr); };
    tbody.appendChild(tr);
  });
}

function filterTable(q) {
  q = q.toLowerCase().trim();
  if (!q) {
    renderTable(allHosts);
    document.getElementById('scan-footer-text').textContent = allHosts.length + ' host(s) found';
    return;
  }
  var filtered = allHosts.filter(function(h) {
    return (h.ip       && h.ip.toLowerCase().indexOf(q)       !== -1) ||
           (h.mac      && h.mac.toLowerCase().indexOf(q)      !== -1) ||
           (h.comment  && h.comment.toLowerCase().indexOf(q)  !== -1) ||
           (h.vendor   && h.vendor.toLowerCase().indexOf(q)   !== -1) ||
           (h.hostname && h.hostname.toLowerCase().indexOf(q) !== -1);
  });
  renderTable(filtered);
  document.getElementById('scan-footer-text').textContent =
    filtered.length + ' / ' + allHosts.length + ' host(s)';
}

/* ── Row select (SSH panel) ─────────────────────────────────────────────── */

var selectedHost = null;

function selectRow(idx, h, tr) {
  document.querySelectorAll('tbody tr').forEach(function(r){ r.classList.remove('selected'); });
  if (selectedIdx === idx) {
    selectedIdx   = -1;
    selectedHost  = null;
    document.getElementById('ssh-panel').style.display = 'none';
    document.getElementById('btn-scp-src').disabled = true;
    document.getElementById('btn-scp-src-label').textContent = 'Set SCP Source';
    return;
  }
  selectedIdx  = idx;
  selectedHost = h;
  tr.classList.add('selected');

  /* Enable "Set SCP Source" button */
  var btn = document.getElementById('btn-scp-src');
  btn.disabled = false;
  document.getElementById('btn-scp-src-label').textContent = h.ip;

  var lk  = isLuckfox(h.comment);
  var cmd = lk ? 'sshpass -p luckfox ssh root@' + h.ip : 'ssh root@' + h.ip;

  document.getElementById('ssh-cmd-box').textContent = cmd;
  document.getElementById('ssh-host-info').innerHTML =
    '<i class="fa-solid fa-location-dot"></i>&nbsp;' + h.ip +
    (h.comment ? '&nbsp;&nbsp;<i class="fa-solid fa-tag"></i>&nbsp;' + escHtml(h.comment) : '') +
    (lk ? '&nbsp;&nbsp;<i class="fa-solid fa-bolt" style="color:#d97706"></i>&nbsp;Auto-login enabled' : '');

  document.getElementById('ssh-panel').style.display = 'block';
  var cb = document.getElementById('btn-copy');
  cb.innerHTML = '<i class="fa-regular fa-copy"></i> Copy';
  cb.className = 'btn-copy';
}

function copyCmd() {
  var cmd = document.getElementById('ssh-cmd-box').textContent;
  navigator.clipboard.writeText(cmd).then(function() {
    var cb = document.getElementById('btn-copy');
    cb.innerHTML = '<i class="fa-solid fa-check"></i> Copied!';
    cb.className = 'btn-copy copied';
    setTimeout(function(){ cb.innerHTML='<i class="fa-regular fa-copy"></i> Copy'; cb.className='btn-copy'; }, 2000);
  });
}

/* ── Modal ──────────────────────────────────────────────────────────────── */

function setLoading(on) {
  var icon = document.getElementById('rescan-icon');
  var btn  = document.getElementById('btn-rescan');
  var spin = document.getElementById('spinner');
  icon.className = on ? 'fa-solid fa-rotate fa-spin-custom' : 'fa-solid fa-rotate';
  btn.disabled   = on;
  spin.style.display = on ? '' : 'none';
}

function load() {
  setLoading(true);
  fetch('/api/scan')
    .then(function(r){ return r.json(); })
    .then(function(data) {
      allHosts = data.hosts;
      document.getElementById('badge-iface').innerHTML =
        '<i class="fa-solid fa-ethernet"></i>&nbsp;' + data.iface;
      document.getElementById('stat-total').textContent   = data.count;
      document.getElementById('stat-online').textContent  = data.hosts.filter(function(h){ return h.online; }).length;
      document.getElementById('stat-luckfox').textContent = data.hosts.filter(function(h){ return isLuckfox(h.comment); }).length;
      document.getElementById('stat-time').textContent    = data.last_scan;
      document.getElementById('err').textContent = '';

      var q = document.getElementById('search-input').value;
      if (q.trim()) {
        filterTable(q);
      } else {
        document.getElementById('scan-footer-text').textContent = data.count + ' host(s) found';
        renderTable(allHosts);
      }
      if (typeof sshTermPopulateSel === 'function') sshTermPopulateSel();
    })
    .catch(function(e) {
      document.getElementById('err').innerHTML =
        '<i class="fa-solid fa-triangle-exclamation"></i> Error: ' + e;
    })
    .finally(function(){ setLoading(false); });
}

function rescan() {
  setLoading(true);
  document.getElementById('scan-footer-text').textContent = 'Scanning...';
  fetch('/rescan')
    .then(function(){ return load(); })
    .catch(function(e) {
      document.getElementById('err').innerHTML =
        '<i class="fa-solid fa-triangle-exclamation"></i> Rescan error: ' + e;
      setLoading(false);
    });
}

/* ── Keyboard shortcut: Escape closes modal ─────────────────────────────── */
document.addEventListener('keydown', function(e) {
  if (e.key === 'Escape') closeModal();
});

/* ── Multi SCP ──────────────────────────────────────────────────────────── */

var scpOpen      = false;
var scpSourceIP      = '';
function toggleHostList() {
  hostListOpen = !hostListOpen;
  document.getElementById('hostlist-body').style.display = hostListOpen ? '' : 'none';
  document.getElementById('hostlist-chevron').style.transform =
    hostListOpen ? '' : 'rotate(-90deg)';
}

