function openAddModal() {
  document.getElementById('modal-title').innerHTML =
    '<i class="fa-solid fa-plus"></i> Add Comment';
  document.getElementById('modal-key').value     = '';
  document.getElementById('modal-comment').value = '';
  document.getElementById('modal-key').readOnly  = false;
  document.getElementById('modal-err').textContent = '';
  document.getElementById('modal-overlay').classList.add('show');
  setTimeout(function(){ document.getElementById('modal-key').focus(); }, 50);
}

function openEditModal(key, comment) {
  document.getElementById('modal-title').innerHTML =
    '<i class="fa-solid fa-pen"></i> Edit Comment';
  document.getElementById('modal-key').value     = key;
  document.getElementById('modal-comment').value = comment;
  document.getElementById('modal-key').readOnly  = true;
  document.getElementById('modal-err').textContent = '';
  document.getElementById('modal-overlay').classList.add('show');
  setTimeout(function(){ document.getElementById('modal-comment').focus(); }, 50);
}

function closeModal() {
  document.getElementById('modal-overlay').classList.remove('show');
}

function overlayClick(e) {
  if (e.target === document.getElementById('modal-overlay')) closeModal();
}

/* ── CRUD ───────────────────────────────────────────────────────────────── */

function editRow(e, key, comment) {
  e.stopPropagation();
  openEditModal(key, comment);
}

function delRow(e, key, src) {
  e.stopPropagation();
  if (!confirm('Delete comment for: ' + key + '?')) return;
  deleteComment(key, src || 'manual');
}

function pinMqtt(e, key, comment) {
  e.stopPropagation();
  /* Save current MQTT label as a manual entry → becomes "override" */
  var body = 'key=' + encodeURIComponent(key) + '&comment=' + encodeURIComponent(comment);
  fetch('/api/comment', { method: 'POST', headers: {'Content-Type':'application/x-www-form-urlencoded'}, body: body })
    .then(function(r) { if (!r.ok) throw new Error('Server error ' + r.status); return r.json(); })
    .then(function() { load(); })
    .catch(function(err) { alert('Pin error: ' + err.message); });
}

function unpinRow(e, key) {
  e.stopPropagation();
  if (!confirm('Remove manual override for ' + key + '? Will revert to MQTT label.')) return;
  deleteComment(key, 'manual');
}

function doSaveComment() {
  var key     = document.getElementById('modal-key').value.trim();
  var comment = document.getElementById('modal-comment').value.trim();
  if (!key) {
    document.getElementById('modal-err').textContent = 'Key is required.';
    return;
  }
  var btn = document.getElementById('modal-save-btn');
  btn.disabled = true;
  btn.innerHTML = '<i class="fa-solid fa-spinner fa-spin-custom"></i> Saving...';

  var body = 'key=' + encodeURIComponent(key) + '&comment=' + encodeURIComponent(comment);
  fetch('/api/comment', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: body
  })
  .then(function(r) {
    if (!r.ok) throw new Error('Server error ' + r.status);
    return r.json();
  })
  .then(function() {
    closeModal();
    load();
  })
  .catch(function(err) {
    document.getElementById('modal-err').textContent = 'Error: ' + err.message;
  })
  .finally(function() {
    btn.disabled = false;
    btn.innerHTML = '<i class="fa-solid fa-floppy-disk"></i> Save';
  });
}

function deleteComment(key, src) {
  fetch('/api/comment?key=' + encodeURIComponent(key) + '&src=' + encodeURIComponent(src || 'manual'), { method: 'DELETE' })
    .then(function(r) {
      if (!r.ok) throw new Error('Server error ' + r.status);
      return r.json();
    })
    .then(function() { load(); })
    .catch(function(err) {
      document.getElementById('err').innerHTML =
        '<i class="fa-solid fa-triangle-exclamation"></i> Delete error: ' + err.message;
    });
}

/* ── Load / Rescan ──────────────────────────────────────────────────────── */


