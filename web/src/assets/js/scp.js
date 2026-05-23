function scpUpdateSelCount() {
  var n = scpSelected.size;
  document.getElementById('scp-sel-count').textContent = n ? n + ' selected' : '';
  var rmBtn = document.getElementById('scp-src-rm-btn');
  var rnBtn = document.getElementById('scp-src-rename-btn');
  if (rmBtn) rmBtn.disabled = n === 0;
  if (rnBtn) rnBtn.disabled = n !== 1;
  var permBtn = document.getElementById('scp-perm-btn');
  if (permBtn) permBtn.disabled = n === 0;
  var ulBtn = document.getElementById('scp-upload-btn');
  if (ulBtn) ulBtn.disabled = !(n > 0 && scpTgtIP);
}

function scpUpdateDeploy() {
  var hasFiles   = scpSelected.size > 0;
  var hasTargets = document.querySelectorAll('.scp-tcb:checked').length > 0;
  document.getElementById('scp-deploy-btn').disabled = !(hasFiles && hasTargets);

  var t = document.querySelectorAll('.scp-tcb:checked').length;
  document.getElementById('scp-summary').textContent =
    (hasFiles || t) ? scpSelected.size + ' file(s)  \u2192  ' + t + ' target(s)' : '';
}

function scpPopulateSources() {
  var sel = document.getElementById('scp-source-sel');
  var prev = sel.value;
  sel.innerHTML = '<option value="">— select source device —</option>'
    + '<option value="LOCAL">[Pi] Local Server (Raspberry Pi)</option>';
  allHosts.forEach(function(h) {
    var opt = document.createElement('option');
    opt.value = h.ip;
    opt.textContent = h.ip + (h.comment ? '  \u2014 ' + h.comment : '')
                           + (h.hostname && h.hostname !== h.ip ? '  [' + h.hostname + ']' : '');
    sel.appendChild(opt);
  });
  var cust = document.createElement('option');
  cust.value = '__custom__';
  cust.textContent = '— custom IP... —';
  sel.appendChild(cust);
  if (prev) sel.value = prev;
}

function scpPopulateTargets() {
  var list  = document.getElementById('scp-target-list');
  var hosts = allHosts.filter(function(h){ return isLuckfox(h.comment); });
  if (!hosts.length) {
    list.innerHTML = '<div class="scp-empty">No Luckfox devices found. Run Rescan first.</div>';
    return;
  }
  list.innerHTML = '';
  hosts.forEach(function(h) {
    var row = document.createElement('div');
    row.className = 'target-row';
    row.style.flexDirection = 'column';
    row.style.alignItems    = 'flex-start';
    row.style.gap           = '0';
    row.style.padding       = '10px 14px';

    var safeIP = escHtml(h.ip);
    var browserId = 'tbp-' + h.ip.replace(/\./g, '-');

    row.innerHTML =
      '<div style="display:flex;align-items:center;gap:10px;width:100%">' +
        '<input type="checkbox" class="scp-tcb" value="' + safeIP + '">' +
        '<div style="flex:1">' +
          '<div class="target-ip">' +
            '<span class="target-ip-link" onclick="toggleTargetBrowser(event,\'' + safeIP + '\',\'' + browserId + '\')">' +
              safeIP +
              '&nbsp;<i class="fa-solid fa-folder-open tbrowse-icon"></i>' +
            '</span>' +
          '</div>' +
          '<div class="target-cmt">' + escHtml(h.comment) + '</div>' +
        '</div>' +
      '</div>' +
      '<div class="target-browser" id="' + browserId + '">' +
        '<div class="target-browser-bar" id="' + browserId + '-bc">' +
          '<i class="fa-solid fa-spinner fa-spin-custom"></i>' +
        '</div>' +
        '<div class="target-file-list" id="' + browserId + '-list"></div>' +
        '<div class="target-actions-bar">' +
          '<button class="scp-act-btn" title="Refresh" onclick="tgtRefresh(\'' + safeIP + '\',\'' + browserId + '\')">' +
            '<i class="fa-solid fa-rotate"></i> Refresh</button>' +
          '<button class="scp-act-btn" title="New Folder" onclick="tgtMkdir(\'' + safeIP + '\',\'' + browserId + '\')">' +
            '<i class="fa-solid fa-folder-plus"></i> New Folder</button>' +
          '<button class="scp-act-btn scp-act-btn-danger" id="' + browserId + '-rm" disabled title="Delete selected"' +
            ' onclick="tgtRm(\'' + safeIP + '\',\'' + browserId + '\')">' +
            '<i class="fa-solid fa-trash"></i> Delete</button>' +
          '<button class="scp-act-btn scp-act-btn-danger" id="' + browserId + '-ren" disabled title="Rename selected"' +
            ' onclick="tgtRename(\'' + safeIP + '\',\'' + browserId + '\')">' +
            '<i class="fa-solid fa-pen"></i> Rename</button>' +
        '</div>' +
      '</div>';

    var cb = row.querySelector('input.scp-tcb');
    cb.addEventListener('change', function() {
      scpUpdateDeploy();
      syncSSHTabsFromSCP();
      var browser = document.getElementById(browserId);
      if (cb.checked) {
        if (browser) browser.classList.add('open');
        var dest = document.getElementById('scp-tgt-dest-input').value.trim() || '/root/';
        renderTargetBrowserFiles(h.ip, dest, browser);
      } else {
        if (browser) { browser.classList.remove('open'); browser.querySelector('.target-file-list').innerHTML = ''; }
      }
    });

    /* Clicking the row body (not the browser area or checkbox) toggles checkbox */
    row.addEventListener('click', function(e) {
      if (e.target.classList.contains('target-ip-link') ||
          e.target.closest('.target-ip-link') ||
          e.target.classList.contains('target-browser') ||
          e.target.closest('.target-browser') ||
          e.target === cb) return;
      cb.checked = !cb.checked;
      cb.dispatchEvent(new Event('change'));
    });

    list.appendChild(row);
  });
}

function scpSelectAll() {
  document.querySelectorAll('.scp-tcb').forEach(function(c){
    c.checked = true; c.dispatchEvent(new Event('change'));
  });
}
function scpDeselectAll() {
  document.querySelectorAll('.scp-tcb').forEach(function(c){
    c.checked = false; c.dispatchEvent(new Event('change'));
  });
}

function scpRefreshTargetBrowsers() {
  var dest = document.getElementById('scp-tgt-dest-input').value.trim() || '/root/';
  document.querySelectorAll('.scp-tcb:checked').forEach(function(cb) {
    var ip = cb.value;
    var browserId = 'tbp-' + ip.replace(/\./g, '-');
    var panel = document.getElementById(browserId);
    if (panel) renderTargetBrowserFiles(ip, dest, panel);
  });
}

function onSourceChange() {
  var val = document.getElementById('scp-source-sel').value;
  var customBar = document.getElementById('scp-custom-bar');
  if (val === '__custom__') {
    customBar.style.display = '';
    document.getElementById('scp-custom-ip').focus();
  } else {
    customBar.style.display = 'none';
    if (val) {
      scpSourceIP = val;
      scpPath     = val === 'LOCAL' ? '/' : '/root';
      scpSelected.clear();
      scpUpdateSelCount();
      scpBrowse(scpPath);
      syncSSHTabsFromSCP();
    }
  }
}

function scpBrowse(path) {
  var ip = document.getElementById('scp-source-sel').value === '__custom__'
         ? document.getElementById('scp-custom-ip').value.trim()
         : scpSourceIP;
  if (!ip || ip === '__custom__') return;
  scpSourceIP = ip;
  scpPath     = path;

  scpRenderBreadcrumb(path);
  document.getElementById('scp-refresh-btn').style.display = '';
  document.getElementById('scp-src-path-bar').style.display = '';
  document.getElementById('scp-src-actions').style.display = '';
  document.getElementById('scp-src-path-input').value = path;

  document.getElementById('scp-filelist').innerHTML =
    '<div class="scp-loading"><i class="fa-solid fa-spinner fa-spin-custom"></i>&nbsp; Loading...</div>';

  fetch('/api/browse?ip=' + encodeURIComponent(ip) + '&path=' + encodeURIComponent(path))
    .then(function(r){ return r.json(); })
    .then(function(data) {
      if (data.error) {
        document.getElementById('scp-filelist').innerHTML =
          '<div class="scp-empty"><i class="fa-solid fa-triangle-exclamation" style="color:#dc2626;margin-right:6px"></i>' +
          escHtml(data.error.substring(0, 200)) + '</div>';
        return;
      }
      scpRenderFiles(data.entries, path);
    })
    .catch(function(e) {
      document.getElementById('scp-filelist').innerHTML =
        '<div class="scp-empty">Error: ' + escHtml(e.message) + '</div>';
    });
}

function scpRenderBreadcrumb(path) {
  var parts = path.split('/').filter(function(p){ return p !== ''; });
  var bc = document.getElementById('scp-breadcrumb');
  bc.innerHTML = '';

  var root = document.createElement('a');
  root.innerHTML = '<i class="fa-solid fa-hard-drive"></i>&nbsp;/';
  root.onclick = function(){ scpBrowse('/'); };
  bc.appendChild(root);

  var cum = '';
  parts.forEach(function(part, i) {
    cum += '/' + part;
    bc.appendChild(document.createTextNode('\u00a0/\u00a0'));
    if (i === parts.length - 1) {
      var cur = document.createElement('span');
      cur.textContent = part;
      cur.style.fontWeight = '600';
      cur.style.color = '#111827';
      bc.appendChild(cur);
    } else {
      var a = document.createElement('a');
      a.textContent = part;
      (function(p){ a.onclick = function(){ scpBrowse(p); }; })(cum);
      bc.appendChild(a);
    }
  });
}

function scpRenderFiles(entries, currentPath) {
  var list = document.getElementById('scp-filelist');
  list.innerHTML = '';

  /* Column header with sort — each span carries the same class as the data column
     so widths/flex match without duplication */
  var hdr = document.createElement('div');
  hdr.className = 'scp-col-hdr';
  var scpColClass = {name:'scp-fname',size:'scp-fsize',date:'scp-fdate',perms:'scp-fperms',owner:'scp-fowner'};
  function sortHdr(col, label) {
    var arrow = (scpSortKey === col) ? (scpSortAsc ? ' ▲' : ' ▼') : '';
    return '<span class="scp-col-sort ' + scpColClass[col] + '" data-col="' + col + '">' + label + arrow + '</span>';
  }
  hdr.innerHTML =
    '<span style="width:15px;flex-shrink:0"></span>' +
    '<span style="width:16px;flex-shrink:0"></span>' +
    sortHdr('name',  'Name') +
    sortHdr('size',  'Size') +
    sortHdr('date',  'Changed') +
    sortHdr('perms', 'Rights') +
    sortHdr('owner', 'Owner');
  hdr.querySelectorAll('.scp-col-sort').forEach(function(el) {
    el.addEventListener('click', function() {
      var col = el.dataset.col;
      if (scpSortKey === col) scpSortAsc = !scpSortAsc;
      else { scpSortKey = col; scpSortAsc = true; }
      scpRenderFiles(scpCurrentEntries, scpCurrentPath);
    });
  });
  list.appendChild(hdr);

  /* Parent dir link */
  if (currentPath !== '/') {
    var parent = currentPath.replace(/\/[^/]+\/?$/, '') || '/';
    var back = document.createElement('div');
    back.className = 'scp-file-row';
    back.innerHTML =
      '<span style="width:15px;flex-shrink:0"></span>' +
      '<span class="scp-ico-dir"><i class="fa-solid fa-turn-up"></i></span>' +
      '<span class="scp-fname scp-fname-dir">..</span>' +
      '<span class="scp-fsize"></span><span class="scp-fdate"></span>' +
      '<span class="scp-fperms"></span><span class="scp-fowner"></span>';
    back.onclick = function(){ scpBrowse(parent); };
    list.appendChild(back);
  }

  /* Save for re-sort on column click */
  scpCurrentEntries = entries || [];
  scpCurrentPath    = currentPath;

  if (!entries || !entries.length) {
    var empty = document.createElement('div');
    empty.className = 'scp-empty';
    empty.innerHTML = '<i class="fa-solid fa-folder-open" style="margin-right:6px"></i>Empty directory';
    list.appendChild(empty);
    return;
  }

  /* Sort entries by scpSortKey; dirs always before files */
  function sortEntries(arr) {
    return arr.slice().sort(function(a, b) {
      var av, bv;
      if      (scpSortKey === 'size')  { av = a.size  || 0;  bv = b.size  || 0; }
      else if (scpSortKey === 'date')  { av = a.date  || ''; bv = b.date  || ''; }
      else if (scpSortKey === 'perms') { av = a.perms || ''; bv = b.perms || ''; }
      else if (scpSortKey === 'owner') { av = a.owner || ''; bv = b.owner || ''; }
      else                             { av = a.name  || ''; bv = b.name  || ''; }
      if (av < bv) return scpSortAsc ? -1 : 1;
      if (av > bv) return scpSortAsc ?  1 : -1;
      return 0;
    });
  }
  var dirs  = sortEntries(entries.filter(function(e){ return e.type === 'd'; }));
  var files = sortEntries(entries.filter(function(e){ return e.type !== 'd'; }));
  dirs.concat(files).forEach(function(e) {
    var fullPath = currentPath.replace(/\/$/, '') + '/' + e.name;
    var isDir    = e.type === 'd';
    var icoClass = isDir ? 'scp-ico-dir' : (e.type === 'l' ? 'scp-ico-link' : 'scp-ico-file');
    var icoName  = isDir ? 'fa-folder'   : (e.type === 'l' ? 'fa-link'      : 'fa-file');
    var sizeStr  = isDir ? '' : scpFmtSize(e.size);
    var checked  = scpSelected.has(fullPath);

    /* Deploy history badge */
    var deployBadge = '';
    if (!isDir) {
      var okCount = 0, failCount = 0;
      allHosts.filter(function(h){ return isLuckfox(h.comment); }).forEach(function(h) {
        var hist = scpHistoryGet(h.ip, fullPath);
        if (hist) { if (hist.status === 'ok') okCount++; else failCount++; }
      });
      if (okCount || failCount) {
        deployBadge = '&nbsp;<span class="deploy-badge' + (failCount && !okCount ? ' fail' : '') + '">' +
          '<i class="fa-solid fa-circle-check"></i>&nbsp;' + okCount +
          (failCount ? '&nbsp;<i class="fa-solid fa-circle-xmark" style="color:#dc2626"></i>&nbsp;' + failCount : '') +
        '</span>';
      }
    }

    var row = document.createElement('div');
    row.className = 'scp-file-row';
    row.innerHTML =
      '<input type="checkbox"' + (checked ? ' checked' : '') + '>' +
      '<span class="' + icoClass + '"><i class="fa-solid ' + icoName + '"></i></span>' +
      '<span class="scp-fname' + (isDir ? ' scp-fname-dir' : '') + '">' +
        escHtml(e.name) + (isDir ? '/' : '') + deployBadge +
      '</span>' +
      '<span class="scp-fsize">' + sizeStr + '</span>' +
      '<span class="scp-fdate">' + escHtml(scpFmtDate(e.date  || '')) + '</span>' +
      '<span class="scp-fperms">' + escHtml((e.perms || '').substring(1)) + '</span>' +
      '<span class="scp-fowner">' + escHtml(e.owner || '') + '</span>';

    var cb = row.querySelector('input');
    cb.addEventListener('change', function(ev) {
      ev.stopPropagation();
      if (cb.checked) scpSelected.add(fullPath); else scpSelected.delete(fullPath);
      scpUpdateSelCount(); scpUpdateDeploy();
    });
    row.onclick = function(ev) {
      if (ev.target === cb) return;
      if (isDir) {
        scpBrowse(fullPath);
      } else {
        cb.checked = !cb.checked;
        if (cb.checked) scpSelected.add(fullPath); else scpSelected.delete(fullPath);
        scpUpdateSelCount(); scpUpdateDeploy();
      }
    };
    list.appendChild(row);
  });
}

function scpRefresh() {
  if (!scpSourceIP || !scpPath) return;
  var icon = document.getElementById('scp-refresh-icon');
  icon.className = 'fa-solid fa-rotate fa-spin-custom';
  setTimeout(function(){ icon.className = 'fa-solid fa-rotate'; }, 900);
  scpBrowse(scpPath);
}

function deployMultiSCP() {
  var files = Array.from(scpSelected);
  if (!files.length || !scpSourceIP) return;

  var btn  = document.getElementById('scp-deploy-btn');
  var icon = document.getElementById('scp-deploy-icon');
  btn.disabled = true;
  icon.className = 'fa-solid fa-spinner fa-spin-custom';
  document.getElementById('scp-results').style.display = 'none';

  var body;
  if (scpMode === 'multi-target') {
    var targets = Array.from(document.querySelectorAll('.scp-tcb:checked')).map(function(c){ return c.value; });
    var dest    = document.getElementById('scp-dest').value.trim() || '/root/';
    if (!targets.length) { btn.disabled = false; icon.className = 'fa-solid fa-upload'; return; }
    document.getElementById('scp-status').textContent =
      'Transferring ' + files.length + ' file(s) to ' + targets.length + ' device(s)\u2026';
    body = JSON.stringify({
      mode:         'multi-target',
      source_ip:    scpSourceIP,
      source_paths: files,
      target_ips:   targets,
      target_dir:   dest
    });
  } else {
    var dirs = Array.from(scpMode2Dirs);
    if (!scpMode2IP || !dirs.length) { btn.disabled = false; icon.className = 'fa-solid fa-upload'; return; }
    document.getElementById('scp-status').textContent =
      'Transferring ' + files.length + ' file(s) to ' + dirs.length + ' director(ies) on ' + scpMode2IP + '\u2026';
    body = JSON.stringify({
      mode:         'multi-dir',
      source_ip:    scpSourceIP,
      source_paths: files,
      target_ip:    scpMode2IP,
      target_dirs:  dirs
    });
  }

  fetch('/api/multi-scp', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: body
  })
  .then(function(r){ return r.json(); })
  .then(function(data) {
    if (data.error) {
      document.getElementById('scp-status').textContent = 'Error: ' + data.error;
      return;
    }
    scpRenderResults(data.results);
    var ok   = data.results.filter(function(r){ return r.status==='ok'; }).length;
    var fail = data.results.length - ok;
    document.getElementById('scp-status').textContent =
      ok + ' succeeded' + (fail ? ', ' + fail + ' failed' : ' \u2014 all done');
  })
  .catch(function(e) {
    document.getElementById('scp-status').textContent = 'Request error: ' + e.message;
  })
  .finally(function() {
    btn.disabled = false;
    icon.className = 'fa-solid fa-upload';
    scpUpdateDeploy();
  });
}

function scpRenderResults(results) {
  var panel = document.getElementById('scp-results');
  var list  = document.getElementById('scp-results-list');
  var now   = scpNowStr();
  list.innerHTML = '';
  results.forEach(function(r) {
    var ok = r.status === 'ok';

    /* Save to localStorage only for multi-target mode (keyed by target IP) */
    if (scpMode === 'multi-target') {
      Array.from(scpSelected).forEach(function(fp) {
        scpHistorySave(r.target, fp, ok ? 'ok' : 'fail', now);
      });
    }

    var row = document.createElement('div');
    row.className = 'result-row';
    row.innerHTML =
      '<span class="' + (ok ? 'result-ok' : 'result-fail') + '">' +
        '<i class="fa-solid ' + (ok ? 'fa-circle-check' : 'fa-circle-xmark') + '"></i>' +
      '</span>' +
      '<div style="flex:1">' +
        '<div style="display:flex;align-items:center;gap:8px;flex-wrap:wrap">' +
          '<span class="result-ip">' + escHtml(r.target) + '</span>' +
          '<span class="' + (ok ? 'badge-ok' : 'badge-fail') + '">' + (ok ? 'OK' : 'FAILED') + '</span>' +
          '<span class="fdone-time"><i class="fa-regular fa-clock"></i>&nbsp;' + now + '</span>' +
        '</div>' +
        (r.output && r.output.trim()
          ? '<div class="result-out">' + escHtml(r.output.trim().substring(0, 400)) + '</div>'
          : '') +
      '</div>';
    list.appendChild(row);
  });
  panel.style.display = '';

  /* Refresh any open target browsers (multi-target mode only) */
  if (scpMode === 'multi-target') {
    document.querySelectorAll('.target-browser.open').forEach(function(el) {
      var ip = el.id.replace('tbp-','').replace(/-/g,'.');
      var pathEl = el.querySelector('.target-browser-bar');
      if (pathEl && pathEl.dataset.path)
        renderTargetBrowserFiles(ip, pathEl.dataset.path, el);
    });
  }
}

/* ── Mode toggle ────────────────────────────────────────────────────────── */

function scpSetMode(mode) {
  scpMode = mode;
  var is1 = mode === 'multi-target';
  document.getElementById('scp-mode1-content').style.display  = is1 ? '' : 'none';
  document.getElementById('scp-mode2-content').style.display  = is1 ? 'none' : '';
  document.getElementById('scp-mode1-allnone').style.display  = is1 ? 'flex' : 'none';
  document.getElementById('scp-dest-label').style.display     = is1 ? '' : 'none';
  document.getElementById('scp-dest').style.display           = is1 ? '' : 'none';
  document.getElementById('scp-mode-btn-1').className = 'mode-btn' + (is1  ? ' active' : '');
  document.getElementById('scp-mode-btn-2').className = 'mode-btn' + (!is1 ? ' active' : '');
  if (!is1) scpMode2PopulateTargets();
  scpUpdateDeploy();
}

function scpMode2PopulateTargets() {
  var list  = document.getElementById('scp-mode2-device-list');
  var hosts = allHosts.filter(function(h){ return isLuckfox(h.comment); });
  if (!hosts.length) {
    list.innerHTML = '<div class="scp-empty">No Luckfox devices found.</div>';
    return;
  }
  list.innerHTML = '';
  hosts.forEach(function(h) {
    var row = document.createElement('div');
    row.className = 'target-row';
    var safeIP  = escHtml(h.ip);
    var checked = (scpMode2IP === h.ip);
    row.innerHTML =
      '<input type="radio" name="scp-mode2-radio" value="' + safeIP + '"' + (checked ? ' checked' : '') + '>' +
      '<div style="flex:1">' +
        '<div class="target-ip">' + safeIP + '</div>' +
        '<div class="target-cmt">' + escHtml(h.comment) + '</div>' +
      '</div>';
    var rb = row.querySelector('input');
    rb.addEventListener('change', function() {
      if (rb.checked) {
        scpMode2IP   = h.ip;
        scpMode2Path = '/root';
        scpMode2Dirs.clear();
        scpMode2UpdateDirCount();
        document.getElementById('scp-mode2-browser').style.display = '';
        scpMode2Browse('/root');
      }
      scpUpdateDeploy();
    });
    row.addEventListener('click', function(e) {
      if (e.target === rb) return;
      rb.checked = true;
      rb.dispatchEvent(new Event('change'));
    });
    list.appendChild(row);
  });
}

function scpMode2Browse(path) {
  if (!scpMode2IP) return;
  scpMode2Path = path;
  scpMode2RenderBreadcrumb(path);
  document.getElementById('scp-mode2-filelist').innerHTML =
    '<div class="scp-loading"><i class="fa-solid fa-spinner fa-spin-custom"></i>&nbsp; Loading...</div>';
  fetch('/api/browse?ip=' + encodeURIComponent(scpMode2IP) + '&path=' + encodeURIComponent(path))
    .then(function(r){ return r.json(); })
    .then(function(data) {
      if (data.error) {
        document.getElementById('scp-mode2-filelist').innerHTML =
          '<div class="scp-empty"><i class="fa-solid fa-triangle-exclamation" style="color:#dc2626;margin-right:6px"></i>' +
          escHtml(data.error.substring(0, 200)) + '</div>';
        return;
      }
      scpMode2RenderDirs(data.entries, path);
    })
    .catch(function(e) {
      document.getElementById('scp-mode2-filelist').innerHTML =
        '<div class="scp-empty">Error: ' + escHtml(e.message) + '</div>';
    });
}

function scpMode2RenderBreadcrumb(path) {
  var parts = path.split('/').filter(Boolean);
  var bc = document.getElementById('scp-mode2-breadcrumb');
  bc.innerHTML = '';
  var root = document.createElement('a');
  root.innerHTML = '<i class="fa-solid fa-hard-drive"></i>&nbsp;/';
  root.onclick = function(){ scpMode2Browse('/'); };
  bc.appendChild(root);
  var cum = '';
  parts.forEach(function(part, i) {
    cum += '/' + part;
    bc.appendChild(document.createTextNode('\u00a0/\u00a0'));
    if (i === parts.length - 1) {
      var s = document.createElement('span');
      s.textContent      = part;
      s.style.color      = '#111827';
      s.style.fontWeight = '600';
      bc.appendChild(s);
    } else {
      var a = document.createElement('a');
      a.textContent = part;
      (function(p){ a.onclick = function(){ scpMode2Browse(p); }; })(cum);
      bc.appendChild(a);
    }
  });
}

function scpMode2RenderDirs(entries, currentPath) {
  var list = document.getElementById('scp-mode2-filelist');
  list.innerHTML = '';

  /* ".." back row */
  if (currentPath !== '/') {
    var parent = currentPath.replace(/\/[^/]+\/?$/, '') || '/';
    var back = document.createElement('div');
    back.className = 'scp-file-row';
    back.innerHTML =
      '<span style="width:15px;flex-shrink:0"></span>' +
      '<span class="scp-ico-dir"><i class="fa-solid fa-turn-up"></i></span>' +
      '<span class="scp-fname scp-fname-dir">..</span>';
    back.onclick = function(){ scpMode2Browse(parent); };
    list.appendChild(back);
  }

  /* "Select this folder" row for the current directory */
  var normCurrent = (currentPath === '/' ? '/' : currentPath.replace(/\/$/, '') + '/');
  var checkedSelf = scpMode2Dirs.has(normCurrent);
  var selfRow = document.createElement('div');
  selfRow.className = 'scp-file-row';
  selfRow.style.background = '#f0fdf4';
  selfRow.innerHTML =
    '<input type="checkbox"' + (checkedSelf ? ' checked' : '') + '>' +
    '<span class="scp-ico-dir"><i class="fa-solid fa-folder-open" style="color:#059669"></i></span>' +
    '<span class="scp-fname scp-fname-dir">. (this folder)</span>' +
    '<span class="scp-fsize" style="color:#059669">select as destination</span>';
  var cbSelf = selfRow.querySelector('input');
  cbSelf.addEventListener('change', function(ev) {
    ev.stopPropagation();
    if (cbSelf.checked) scpMode2Dirs.add(normCurrent); else scpMode2Dirs.delete(normCurrent);
    scpMode2UpdateDirCount(); scpUpdateDeploy();
  });
  selfRow.addEventListener('click', function(ev) {
    if (ev.target === cbSelf) return;
    cbSelf.checked = !cbSelf.checked;
    cbSelf.dispatchEvent(new Event('change'));
  });
  list.appendChild(selfRow);

  if (!entries || !entries.length) {
    var empty = document.createElement('div');
    empty.className = 'scp-empty';
    empty.innerHTML = '<i class="fa-solid fa-folder-open" style="margin-right:6px"></i>Empty directory';
    list.appendChild(empty);
    return;
  }

  var dirs  = entries.filter(function(e){ return e.type === 'd'; });
  var files = entries.filter(function(e){ return e.type !== 'd'; });

  /* Subdirectory rows — click navigates, checkbox selects as destination */
  dirs.forEach(function(e) {
    var fullPath = currentPath.replace(/\/$/, '') + '/' + e.name + '/';
    var checked  = scpMode2Dirs.has(fullPath);
    var row = document.createElement('div');
    row.className = 'scp-file-row';
    row.title = 'Click to navigate \u2022 Use checkbox to select as destination';
    row.innerHTML =
      '<input type="checkbox"' + (checked ? ' checked' : '') + '>' +
      '<span class="scp-ico-dir"><i class="fa-solid fa-folder"></i></span>' +
      '<span class="scp-fname scp-fname-dir">' + escHtml(e.name) + '/</span>';
    var cb = row.querySelector('input');
    cb.addEventListener('change', function(ev) {
      ev.stopPropagation();
      if (cb.checked) scpMode2Dirs.add(fullPath); else scpMode2Dirs.delete(fullPath);
      scpMode2UpdateDirCount(); scpUpdateDeploy();
    });
    row.addEventListener('click', function(ev) {
      if (ev.target === cb) return;
      scpMode2Browse(currentPath.replace(/\/$/, '') + '/' + e.name);
    });
    list.appendChild(row);
  });

  /* File rows — dimmed, not selectable as destinations */
  files.forEach(function(e) {
    var row = document.createElement('div');
    row.className = 'scp-file-row';
    row.style.opacity = '0.45';
    row.style.cursor  = 'default';
    var icoClass = e.type === 'l' ? 'scp-ico-link' : 'scp-ico-file';
    var icoName  = e.type === 'l' ? 'fa-link'      : 'fa-file';
    row.innerHTML =
      '<span style="width:15px;flex-shrink:0"></span>' +
      '<span class="' + icoClass + '"><i class="fa-solid ' + icoName + '"></i></span>' +
      '<span class="scp-fname">' + escHtml(e.name) + '</span>' +
      '<span class="scp-fsize">' + scpFmtSize(e.size) + '</span>';
    list.appendChild(row);
  });
}

function scpMode2UpdateDirCount() {
  var n  = scpMode2Dirs.size;
  var el = document.getElementById('scp-mode2-dir-count');
  if (el) el.textContent = n ? n + ' dir(s) selected' : '';
}

/* ── Source panel path/actions ──────────────────────────────────────────── */

function scpSrcGo() {
  var v = document.getElementById('scp-src-path-input').value.trim();
  if (!v) return;
  if (v[0] !== '/') v = '/' + v;
  scpBrowse(v);
}

function scpSrcMkdir() {
  if (!scpSourceIP) return;
  var name = prompt('New folder name:');
  if (!name || !name.trim()) return;
  name = name.trim().replace(/['";&|`$\n\r]/g, '');
  if (!name) return;
  var path = scpPath.replace(/\/$/, '') + '/' + name;
  fetch('/api/mkdir', { method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'ip=' + encodeURIComponent(scpSourceIP) + '&path=' + encodeURIComponent(path)
  }).then(function(r){ return r.json(); })
    .then(function(d){ if (d.error) alert('Error: ' + d.error); else scpBrowse(scpPath); })
    .catch(function(e){ alert('Error: ' + e.message); });
}

function scpSrcRm() {
  var files = Array.from(scpSelected);
  if (!files.length || !scpSourceIP) return;
  if (!confirm('Remove ' + files.length + ' item(s) from ' + scpSourceIP + '?\n\n' + files.slice(0,6).join('\n'))) return;
  Promise.all(files.map(function(p) {
    return fetch('/api/rm', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'ip=' + encodeURIComponent(scpSourceIP) + '&path=' + encodeURIComponent(p)
    }).then(function(r){ return r.json(); });
  })).then(function(res) {
    var errs = res.filter(function(d){ return d.error; });
    if (errs.length) alert('Some failed:\n' + errs.map(function(d){ return d.error; }).join('\n'));
    scpSelected.clear(); scpUpdateSelCount(); scpBrowse(scpPath);
  }).catch(function(e){ alert('Error: ' + e.message); });
}

function scpSrcRename() {
  var files = Array.from(scpSelected);
  if (files.length !== 1 || !scpSourceIP) return;
  var oldPath = files[0];
  var oldName = oldPath.split('/').pop();
  var newName = prompt('Rename to:', oldName);
  if (!newName || !newName.trim() || newName === oldName) return;
  newName = newName.trim().replace(/['";&|`$\n\r\/]/g, '');
  var newPath = oldPath.substring(0, oldPath.lastIndexOf('/') + 1) + newName;
  fetch('/api/rename', { method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'ip=' + encodeURIComponent(scpSourceIP) + '&from=' + encodeURIComponent(oldPath) + '&to=' + encodeURIComponent(newPath)
  }).then(function(r){ return r.json(); })
    .then(function(d){ if (d.error) alert('Error: ' + d.error); else { scpSelected.clear(); scpUpdateSelCount(); scpBrowse(scpPath); }})
    .catch(function(e){ alert('Error: ' + e.message); });
}

/* ── Permissions (chmod) modal ──────────────────────────────────────────── */

function openChmodModal() {
  if (scpSelected.size === 0) return;
  document.getElementById('chmod-octal').value = '0644';
  chmodOctalInput('0644');
  document.getElementById('chmod-recursive').checked = false;
  var m = document.getElementById('chmod-modal');
  m.style.display = 'flex';
}

function closeChmodModal() {
  document.getElementById('chmod-modal').style.display = 'none';
}

function chmodSync() {
  var total = 0;
  document.querySelectorAll('.chmod-cb').forEach(function(cb) {
    if (cb.checked) total += parseInt(cb.dataset.bit);
  });
  document.getElementById('chmod-octal').value = total.toString(8).padStart(4, '0');
}

function chmodOctalInput(val) {
  if (!/^[0-7]{1,4}$/.test(val)) return;
  var dec = parseInt(val, 8);
  document.querySelectorAll('.chmod-cb').forEach(function(cb) {
    cb.checked = !!(dec & parseInt(cb.dataset.bit));
  });
}

function applyChmod() {
  var mode = document.getElementById('chmod-octal').value;
  if (!/^[0-7]{1,4}$/.test(mode)) { alert('Invalid octal value (e.g. 0755)'); return; }
  /* zero-pad to 4 digits */
  mode = mode.padStart(4, '0');
  var recursive = document.getElementById('chmod-recursive').checked;
  var paths = Array.from(scpSelected);
  if (paths.length === 0) { alert('No files selected.'); return; }
  var ip = scpSourceIP;
  if (!ip) { alert('No source device selected.'); return; }
  if (ip === 'CUSTOM') {
    ip = (document.getElementById('scp-custom-ip') || {}).value || '';
    ip = ip.trim();
  }
  fetch('/api/chmod', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ip: ip, paths: paths, mode: mode, recursive: recursive })
  })
  .then(function(r) { return r.json(); })
  .then(function(d) {
    if (d.error) { alert('chmod failed: ' + d.error); return; }
    closeChmodModal();
    scpSelected.clear();
    scpUpdateSelCount();
    scpBrowse(scpPath);
  })
  .catch(function(e) { alert('Request failed: ' + e.message); });
}

/* ── Target panel ───────────────────────────────────────────────────────── */

function scpPopulateTargetSel() {
  var sel = document.getElementById('scp-target-sel');
  if (!sel) return;
  var prev = sel.value;
  sel.innerHTML = '<option value="">— select target device —</option>';
  allHosts.forEach(function(h) {
    var opt = document.createElement('option');
    opt.value = h.ip;
    opt.textContent = h.ip +
      (h.comment  ? '  \u2014 ' + h.comment : '') +
      (h.hostname && h.hostname !== h.ip ? '  [' + h.hostname + ']' : '');
    sel.appendChild(opt);
  });
  var cust = document.createElement('option');
  cust.value = '__custom__'; cust.textContent = '— custom IP... —';
  sel.appendChild(cust);
  if (prev) sel.value = prev;
}

function onTargetChange() {
  var val = document.getElementById('scp-target-sel').value;
  if (!val || val === '__custom__') return;
  scpTgtIP   = val;
  scpTgtPath = '/root';
  scpTgtSel.clear();
  scpTgtUpdateSelCount();
  document.getElementById('scp-tgt-path-bar').style.display    = '';
  document.getElementById('scp-tgt-actions').style.display     = '';
  document.getElementById('scp-tgt-refresh-btn').style.display = '';
  scpTgtBrowse('/root');
  /* re-evaluate upload/download now that we have a target */
  scpUpdateSelCount();
}

function scpTgtBrowse(path) {
  if (!scpTgtIP) return;
  scpTgtPath = path;
  document.getElementById('scp-tgt-path-input').value = path;
  scpTgtRenderBreadcrumb(path);
  document.getElementById('scp-tgt-filelist').innerHTML =
    '<div class="scp-loading"><i class="fa-solid fa-spinner fa-spin-custom"></i>&nbsp; Loading...</div>';
  fetch('/api/browse?ip=' + encodeURIComponent(scpTgtIP) + '&path=' + encodeURIComponent(path))
    .then(function(r){ return r.json(); })
    .then(function(data) {
      if (data.error) {
        document.getElementById('scp-tgt-filelist').innerHTML =
          '<div class="scp-empty"><i class="fa-solid fa-triangle-exclamation" style="color:#dc2626;margin-right:6px"></i>' +
          escHtml(data.error.substring(0, 200)) + '</div>';
        return;
      }
      scpTgtRenderFiles(data.entries, path);
    })
    .catch(function(e) {
      document.getElementById('scp-tgt-filelist').innerHTML =
        '<div class="scp-empty">Error: ' + escHtml(e.message) + '</div>';
    });
}

function scpTgtRenderBreadcrumb(path) {
  var parts = path.split('/').filter(Boolean);
  var bc = document.getElementById('scp-tgt-breadcrumb');
  bc.innerHTML = '';
  var root = document.createElement('a');
  root.innerHTML = '<i class="fa-solid fa-microchip"></i>&nbsp;/';
  root.onclick = function(){ scpTgtBrowse('/'); };
  bc.appendChild(root);
  var cum = '';
  parts.forEach(function(part, i) {
    cum += '/' + part;
    bc.appendChild(document.createTextNode('\u00a0/\u00a0'));
    if (i === parts.length - 1) {
      var s = document.createElement('span');
      s.textContent = part; s.style.fontWeight = '600'; s.style.color = '#111827';
      bc.appendChild(s);
    } else {
      var a = document.createElement('a');
      a.textContent = part;
      (function(p){ a.onclick = function(){ scpTgtBrowse(p); }; })(cum);
      bc.appendChild(a);
    }
  });
}

function scpTgtRenderFiles(entries, currentPath) {
  var list = document.getElementById('scp-tgt-filelist');
  list.innerHTML = '';

  var hdr = document.createElement('div');
  hdr.className = 'scp-col-hdr';
  hdr.innerHTML =
    '<span style="width:15px;flex-shrink:0"></span>' +
    '<span style="width:14px;flex-shrink:0"></span>' +
    '<span class="scp-fname">Name</span>' +
    '<span class="scp-fsize">Size</span>' +
    '<span class="scp-fdate">Modified</span>' +
    '<span class="scp-fperms">Rights</span>' +
    '<span class="scp-fowner">Owner</span>';
  list.appendChild(hdr);

  if (currentPath !== '/') {
    var parent = currentPath.replace(/\/[^/]+\/?$/, '') || '/';
    var back = document.createElement('div');
    back.className = 'scp-file-row';
    back.innerHTML =
      '<span style="width:15px;flex-shrink:0"></span>' +
      '<span class="scp-ico-dir"><i class="fa-solid fa-turn-up"></i></span>' +
      '<span class="scp-fname scp-fname-dir">..</span>' +
      '<span class="scp-fsize"></span><span class="scp-fdate"></span>' +
      '<span class="scp-fperms"></span><span class="scp-fowner"></span>';
    back.onclick = function(){ scpTgtBrowse(parent); };
    list.appendChild(back);
  }

  if (!entries || !entries.length) {
    var empty = document.createElement('div');
    empty.className = 'scp-empty';
    empty.innerHTML = '<i class="fa-solid fa-folder-open" style="margin-right:6px"></i>Empty directory';
    list.appendChild(empty);
    return;
  }

  var dirs  = entries.filter(function(e){ return e.type === 'd'; });
  var files = entries.filter(function(e){ return e.type !== 'd'; });
  dirs.concat(files).forEach(function(e) {
    var fullPath = currentPath.replace(/\/$/, '') + '/' + e.name;
    var isDir    = e.type === 'd';
    var icoClass = isDir ? 'scp-ico-dir' : (e.type === 'l' ? 'scp-ico-link' : 'scp-ico-file');
    var icoName  = isDir ? 'fa-folder'   : (e.type === 'l' ? 'fa-link'      : 'fa-file');
    var sizeStr  = isDir ? '' : scpFmtSize(e.size);
    var checked  = scpTgtSel.has(fullPath);

    var row = document.createElement('div');
    row.className = 'scp-file-row';
    row.innerHTML =
      '<input type="checkbox"' + (checked ? ' checked' : '') + '>' +
      '<span class="' + icoClass + '"><i class="fa-solid ' + icoName + '"></i></span>' +
      '<span class="scp-fname' + (isDir ? ' scp-fname-dir' : '') + '">' +
        escHtml(e.name) + (isDir ? '/' : '') +
      '</span>' +
      '<span class="scp-fsize">' + sizeStr + '</span>' +
      '<span class="scp-fdate">'  + escHtml(e.date  || '') + '</span>' +
      '<span class="scp-fperms">' + escHtml(e.perms || '') + '</span>' +
      '<span class="scp-fowner">' + escHtml(e.owner || '') + '</span>';

    var cb = row.querySelector('input');
    cb.addEventListener('change', function(ev) {
      ev.stopPropagation();
      if (cb.checked) scpTgtSel.add(fullPath); else scpTgtSel.delete(fullPath);
      scpTgtUpdateSelCount();
    });
    row.onclick = function(ev) {
      if (ev.target === cb) return;
      if (isDir) {
        scpTgtBrowse(fullPath);
      } else {
        cb.checked = !cb.checked;
        if (cb.checked) scpTgtSel.add(fullPath); else scpTgtSel.delete(fullPath);
        scpTgtUpdateSelCount();
      }
    };
    list.appendChild(row);
  });
}

function scpTgtUpdateSelCount() {
  var n = scpTgtSel.size;
  document.getElementById('scp-tgt-sel-count').textContent = n ? n + ' selected' : '';
  var rmBtn = document.getElementById('scp-tgt-rm-btn');
  var rnBtn = document.getElementById('scp-tgt-rename-btn');
  if (rmBtn) rmBtn.disabled = n === 0;
  if (rnBtn) rnBtn.disabled = n !== 1;
}

function scpTgtRefresh() {
  if (!scpTgtIP || !scpTgtPath) return;
  var icon = document.getElementById('scp-tgt-refresh-icon');
  if (icon) { icon.className = 'fa-solid fa-rotate fa-spin-custom'; setTimeout(function(){ icon.className = 'fa-solid fa-rotate'; }, 900); }
  scpTgtBrowse(scpTgtPath);
}

function scpTgtGo() {
  var v = document.getElementById('scp-tgt-path-input').value.trim();
  if (!v) return;
  if (v[0] !== '/') v = '/' + v;
  scpTgtBrowse(v);
}

function scpTgtMkdir() {
  if (!scpTgtIP) return;
  var name = prompt('New folder name:');
  if (!name || !name.trim()) return;
  name = name.trim().replace(/['";&|`$\n\r]/g, '');
  if (!name) return;
  var path = scpTgtPath.replace(/\/$/, '') + '/' + name;
  fetch('/api/mkdir', { method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'ip=' + encodeURIComponent(scpTgtIP) + '&path=' + encodeURIComponent(path)
  }).then(function(r){ return r.json(); })
    .then(function(d){ if (d.error) alert('Error: ' + d.error); else scpTgtBrowse(scpTgtPath); })
    .catch(function(e){ alert('Error: ' + e.message); });
}

function scpTgtRm() {
  var files = Array.from(scpTgtSel);
  if (!files.length || !scpTgtIP) return;
  if (!confirm('Remove ' + files.length + ' item(s) from ' + scpTgtIP + '?\n\n' + files.slice(0,6).join('\n'))) return;
  Promise.all(files.map(function(p) {
    return fetch('/api/rm', { method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'ip=' + encodeURIComponent(scpTgtIP) + '&path=' + encodeURIComponent(p)
    }).then(function(r){ return r.json(); });
  })).then(function(res) {
    var errs = res.filter(function(d){ return d.error; });
    if (errs.length) alert('Some failed:\n' + errs.map(function(d){ return d.error; }).join('\n'));
    scpTgtSel.clear(); scpTgtUpdateSelCount(); scpTgtBrowse(scpTgtPath);
  }).catch(function(e){ alert('Error: ' + e.message); });
}

function scpTgtRename() {
  var files = Array.from(scpTgtSel);
  if (files.length !== 1 || !scpTgtIP) return;
  var oldPath = files[0];
  var oldName = oldPath.split('/').pop();
  var newName = prompt('Rename to:', oldName);
  if (!newName || !newName.trim() || newName === oldName) return;
  newName = newName.trim().replace(/['";&|`$\n\r\/]/g, '');
  var newPath = oldPath.substring(0, oldPath.lastIndexOf('/') + 1) + newName;
  fetch('/api/rename', { method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'ip=' + encodeURIComponent(scpTgtIP) + '&from=' + encodeURIComponent(oldPath) + '&to=' + encodeURIComponent(newPath)
  }).then(function(r){ return r.json(); })
    .then(function(d){ if (d.error) alert('Error: ' + d.error); else { scpTgtSel.clear(); scpTgtUpdateSelCount(); scpTgtBrowse(scpTgtPath); }})
    .catch(function(e){ alert('Error: ' + e.message); });
}

function deployToHere() {
  var files = Array.from(scpSelected);
  if (!files.length || !scpSourceIP || !scpTgtIP) return;
  var dest = scpTgtPath || '/root';
  if (dest[dest.length - 1] !== '/') dest += '/';

  var dlBtn = document.getElementById('scp-download-btn');
  var ulBtn = document.getElementById('scp-upload-btn');
  if (dlBtn) dlBtn.disabled = true;
  if (ulBtn) ulBtn.disabled = true;

  fetch('/api/multi-scp', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ mode: 'multi-target', source_ip: scpSourceIP,
      source_paths: files, target_ips: [scpTgtIP], target_dir: dest })
  })
  .then(function(r){ return r.json(); })
  .then(function(data) {
    if (data.error) { alert('Transfer error: ' + data.error); return; }
    var r = data.results[0];
    if (r.status === 'ok') {
      scpTgtBrowse(scpTgtPath);
    } else {
      alert('Transfer failed:\n' + r.output.substring(0, 400));
    }
  })
  .catch(function(e){ alert('Error: ' + e.message); })
  .finally(function(){ scpUpdateSelCount(); });
}

function toggleMultiDeploy() {
  var body = document.getElementById('multi-deploy-body');
  var chev = document.getElementById('multi-deploy-chevron');
  var open = body.style.display !== 'none';
  body.style.display = open ? 'none' : '';
  chev.style.transform = open ? '' : 'rotate(90deg)';
  if (!open) { scpPopulateTargets(); scpUpdateSelCount(); }
}

/* ── SCP Source button ──────────────────────────────────────────────────── */

function setAsSCPSource() {
  if (!selectedHost) return;
  /* Expand Multi SCP panel if not open */
  if (!scpOpen) toggleSCPPanel();
  /* Set source IP */
  scpSourceIP = selectedHost.ip;
  scpPath     = '/root';
  scpSelected.clear();
  scpUpdateSelCount();
  /* Update the dropdown */
  var sel = document.getElementById('scp-source-sel');
  if (sel) {
    for (var i = 0; i < sel.options.length; i++) {
      if (sel.options[i].value === selectedHost.ip) { sel.selectedIndex = i; break; }
    }
  }
  document.getElementById('scp-custom-bar').style.display = 'none';
  scpBrowse('/root');
  /* Scroll to Multi SCP panel */
  document.getElementById('scp-card').scrollIntoView({ behavior: 'smooth', block: 'start' });
}

/* ── Target inline browser ──────────────────────────────────────────────── */

function toggleTargetBrowser(e, ip, browserId) {
  e.stopPropagation();
  var panel = document.getElementById(browserId);
  if (!panel) return;
  var isOpen = panel.classList.contains('open');
  if (isOpen) {
    panel.classList.remove('open');
  } else {
    panel.classList.add('open');
    renderTargetBrowserFiles(ip, '/root', panel);
  }
}

function renderTargetBrowserFiles(ip, path, panel) {
  var bcEl   = panel.querySelector('.target-browser-bar');
  var listEl = panel.querySelector('.target-file-list');
  if (!bcEl || !listEl) return;

  /* Store current path on bar element for refresh */
  bcEl.dataset.path = path;

  /* Breadcrumb */
  bcEl.innerHTML = '';
  var parts = path.split('/').filter(Boolean);
  var rootA = document.createElement('a');
  rootA.innerHTML = '<i class="fa-solid fa-hard-drive"></i>&nbsp;/';
  rootA.onclick   = function(){ renderTargetBrowserFiles(ip, '/', panel); };
  bcEl.appendChild(rootA);
  var cum = '';
  parts.forEach(function(part, i) {
    cum += '/' + part;
    bcEl.appendChild(document.createTextNode('\u00a0/\u00a0'));
    if (i === parts.length - 1) {
      var s = document.createElement('span');
      s.textContent   = part;
      s.style.color   = '#111827';
      s.style.fontWeight = '600';
      bcEl.appendChild(s);
    } else {
      var a = document.createElement('a');
      a.textContent = part;
      (function(p){ a.onclick = function(){ renderTargetBrowserFiles(ip, p, panel); }; })(cum);
      bcEl.appendChild(a);
    }
  });

  listEl.innerHTML = '<div style="padding:10px;text-align:center;color:#2563eb;font-size:12px">' +
    '<i class="fa-solid fa-spinner fa-spin-custom"></i>&nbsp;Loading...</div>';

  fetch('/api/browse?ip=' + encodeURIComponent(ip) + '&path=' + encodeURIComponent(path))
    .then(function(r){ return r.json(); })
    .then(function(data) {
      if (data.error) {
        listEl.innerHTML = '<div style="padding:10px;font-size:12px;color:#dc2626">' +
          '<i class="fa-solid fa-triangle-exclamation"></i>&nbsp;' + escHtml(data.error.substring(0,120)) + '</div>';
        return;
      }
      listEl.innerHTML = '';

      /* Parent dir */
      if (path !== '/') {
        var parent = path.replace(/\/[^/]+\/?$/, '') || '/';
        var back = document.createElement('div');
        back.className = 'target-file-row';
        back.innerHTML = '<span class="scp-ico-dir"><i class="fa-solid fa-turn-up"></i></span>' +
                         '<span style="font-weight:600;color:#374151">..</span>';
        back.onclick = function(){ renderTargetBrowserFiles(ip, parent, panel); };
        listEl.appendChild(back);
      }

      if (!data.entries || !data.entries.length) {
        listEl.innerHTML += '<div style="padding:10px;text-align:center;font-size:12px;color:#9ca3af">Empty</div>';
        return;
      }

      var dirs  = data.entries.filter(function(e){ return e.type === 'd'; });
      var files = data.entries.filter(function(e){ return e.type !== 'd'; });
      dirs.concat(files).forEach(function(entry) {
        var fullPath = path.replace(/\/$/, '') + '/' + entry.name;
        var isDir    = entry.type === 'd';
        var icoClass = isDir ? 'scp-ico-dir' : (entry.type === 'l' ? 'scp-ico-link' : 'scp-ico-file');
        var icoName  = isDir ? 'fa-folder'   : (entry.type === 'l' ? 'fa-link'      : 'fa-file');

        /* Check deploy history for this target+file */
        var hist    = scpHistoryGet(ip, fullPath);
        var doneHtml = '';
        if (hist) {
          var isOk = hist.status === 'ok';
          doneHtml =
            '<span class="' + (isOk ? 'fdone-ok' : 'fdone-fail') + '">' +
              '<i class="fa-solid ' + (isOk ? 'fa-circle-check' : 'fa-circle-xmark') + '"></i>' +
            '</span>' +
            '<span class="fdone-time">' + hist.time + '</span>';
        }

        var frow = document.createElement('div');
        frow.className = 'target-file-row';
        frow.innerHTML =
          (!isDir ? '<input type="checkbox" class="tgt-fcb" data-path="' + escHtml(fullPath) + '">' : '<span style="width:14px;flex-shrink:0"></span>') +
          '<span class="' + icoClass + '"><i class="fa-solid ' + icoName + '"></i></span>' +
          '<span style="flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;' +
               (isDir ? 'font-weight:600;color:#374151' : '') + '">' +
            escHtml(entry.name) + (isDir ? '/' : '') +
          '</span>' +
          (isDir ? '' : '<span class="scp-fsize">' + scpFmtSize(entry.size) + '</span>') +
          doneHtml;

        if (isDir) {
          frow.onclick = function(){ renderTargetBrowserFiles(ip, fullPath, panel); };
        } else {
          (function(fp, fr) {
            var cb = fr.querySelector('.tgt-fcb');
            cb.addEventListener('change', function(e) {
              e.stopPropagation();
              fr.classList.toggle('selected', cb.checked);
              tgtUpdateBtns(panel.id);
            });
            fr.onclick = function(e) {
              if (e.target === cb) return;
              cb.checked = !cb.checked;
              cb.dispatchEvent(new Event('change'));
            };
          })(fullPath, frow);
        }
        listEl.appendChild(frow);
      });
    })
    .catch(function(err) {
      listEl.innerHTML = '<div style="padding:10px;font-size:12px;color:#dc2626">Error: ' + escHtml(err.message) + '</div>';
    });
}

/* ── Target browser actions ─────────────────────────────────────────────── */

function tgtUpdateBtns(browserId) {
  var panel   = document.getElementById(browserId);
  var checked = panel ? panel.querySelectorAll('.tgt-fcb:checked') : [];
  var hasSel  = checked.length > 0;
  var one     = checked.length === 1;
  var rmBtn   = document.getElementById(browserId + '-rm');
  var renBtn  = document.getElementById(browserId + '-ren');
  if (rmBtn)  rmBtn.disabled  = !hasSel;
  if (renBtn) renBtn.disabled = !one;
}

function tgtRefresh(ip, browserId) {
  var panel = document.getElementById(browserId);
  var bcEl  = panel ? panel.querySelector('.target-browser-bar') : null;
  if (!panel) return;
  renderTargetBrowserFiles(ip, bcEl ? (bcEl.dataset.path || '/root') : '/root', panel);
}

function tgtMkdir(ip, browserId) {
  var panel = document.getElementById(browserId);
  var bcEl  = panel ? panel.querySelector('.target-browser-bar') : null;
  var path  = bcEl ? (bcEl.dataset.path || '/root') : '/root';
  var name  = prompt('New folder name:');
  if (!name || !name.trim()) return;
  fetch('/api/mkdir', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify({ip: ip, path: path.replace(/\/$/, '') + '/' + name.trim()})
  }).then(function(r){ return r.json(); })
    .then(function(d){ if (d.error) alert('Error: ' + d.error); else renderTargetBrowserFiles(ip, path, panel); });
}

function tgtRm(ip, browserId) {
  var panel   = document.getElementById(browserId);
  var checked = panel ? Array.from(panel.querySelectorAll('.tgt-fcb:checked')) : [];
  if (!checked.length) return;
  var names = checked.map(function(c){ return c.dataset.path.split('/').pop(); }).join(', ');
  if (!confirm('Delete ' + checked.length + ' item(s) on ' + ip + '?\n' + names)) return;
  var bcEl = panel.querySelector('.target-browser-bar');
  var path = bcEl ? (bcEl.dataset.path || '/root') : '/root';
  var done = 0;
  checked.forEach(function(c) {
    fetch('/api/rm', {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ip: ip, path: c.dataset.path})
    }).then(function(r){ return r.json(); })
      .then(function(d){
        if (d.error) alert('Error deleting ' + c.dataset.path + ': ' + d.error);
        if (++done === checked.length) renderTargetBrowserFiles(ip, path, panel);
      });
  });
}

function tgtRename(ip, browserId) {
  var panel   = document.getElementById(browserId);
  var checked = panel ? panel.querySelectorAll('.tgt-fcb:checked') : [];
  if (checked.length !== 1) return;
  var selPath = checked[0].dataset.path;
  var oldName = selPath.split('/').pop();
  var newName = prompt('Rename to:', oldName);
  if (!newName || newName === oldName) return;
  var dir     = selPath.replace(/\/[^/]+$/, '') || '/';
  var newPath = dir + '/' + newName;
  var bcEl    = panel.querySelector('.target-browser-bar');
  var path    = bcEl ? (bcEl.dataset.path || '/root') : '/root';
  fetch('/api/rename', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify({ip: ip, path: selPath, newpath: newPath})
  }).then(function(r){ return r.json(); })
    .then(function(d){
      if (d.error) { alert('Error: ' + d.error); return; }
      renderTargetBrowserFiles(ip, path, panel);
    });
}

/* ── SCP History (localStorage) ─────────────────────────────────────────── */

function scpHistoryKey(targetIP, filePath) {
  return targetIP + '|' + filePath;
}

function scpHistorySave(targetIP, filePath, status, time) {
  var db = {};
  try { db = JSON.parse(localStorage.getItem('ipscan_scp_history') || '{}'); } catch(e){}
  db[scpHistoryKey(targetIP, filePath)] = { status: status, time: time };
  try { localStorage.setItem('ipscan_scp_history', JSON.stringify(db)); } catch(e){}
}

function scpHistoryGet(targetIP, filePath) {
  var db = {};
  try { db = JSON.parse(localStorage.getItem('ipscan_scp_history') || '{}'); } catch(e){}
  return db[scpHistoryKey(targetIP, filePath)] || null;
}


