function isLuckfox(comment) {
  return comment && comment.indexOf('Line') !== -1 && comment.indexOf('[GM') !== -1;
}

function escHtml(s) {
  return String(s)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

/* ── Table rendering ────────────────────────────────────────────────────── */

function escAttr(s) {
  return String(s)
    .replace(/\\/g,'\\\\').replace(/'/g,"\\'")
    .replace(/"/g,'&quot;');
}

/* ── Search ─────────────────────────────────────────────────────────────── */

function scpFmtSize(b) {
  if (b >= 1048576) return (b/1048576).toFixed(1) + ' MB';
  if (b >= 1024)    return (b/1024).toFixed(1) + ' KB';
  return b + ' B';
}

/* Convert date string to display format.
   GNU (LOCAL): "YYYY-MM-DD HH:MM:SS" → "DD/MM/YYYY H:MM:SS AM/PM"
   BusyBox (remote): "May 22 14:30" — shown as-is (no seconds available) */
function scpFmtDate(s) {
  if (!s) return '';
  var m = s.match(/^(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})$/);
  if (!m) return s;  /* BusyBox format — show as-is */
  var h = parseInt(m[4], 10), min = m[5], sec = m[6];
  var ampm = h >= 12 ? 'PM' : 'AM';
  var h12  = h % 12 || 12;
  return m[3] + '/' + m[2] + '/' + m[1] + ' ' + h12 + ':' + min + ':' + sec + ' ' + ampm;
}

function scpNowStr() {
  var d = new Date();
  return d.getFullYear() + '-' +
    String(d.getMonth()+1).padStart(2,'0') + '-' +
    String(d.getDate()).padStart(2,'0') + ' ' +
    String(d.getHours()).padStart(2,'0') + ':' +
    String(d.getMinutes()).padStart(2,'0') + ':' +
    String(d.getSeconds()).padStart(2,'0');
}

/* ── Host List toggle ───────────────────────────────────────────────────── */
var hostListOpen = true;

