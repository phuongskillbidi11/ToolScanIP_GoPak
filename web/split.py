#!/usr/bin/env python3
"""
split.py — Extract CSS and JS from web/scanner.html into separate source files.

Run once from the project root:
    python3 web/split.py

Creates:
    web/src/scanner.html          (HTML only, with <link> and <script> tags)
    web/src/assets/css/*.css      (5 CSS files)
    web/src/assets/js/*.js        (6 JS files)
    web/build.py                  (inliner: web/src/ → web/scanner.html)
"""

import os, re

SRC  = 'web/scanner.html'
OUT  = 'web/src'
CSS  = f'{OUT}/assets/css'
JS   = f'{OUT}/assets/js'

os.makedirs(CSS, exist_ok=True)
os.makedirs(JS,  exist_ok=True)

with open(SRC, encoding='utf-8') as f:
    lines = f.readlines()

# ── Locate block boundaries ───────────────────────────────────────────────────
style_open  = next(i for i,l in enumerate(lines) if l.strip() == '<style>')
style_close = next(i for i,l in enumerate(lines) if l.strip() == '</style>')
script_open  = next(i for i,l in enumerate(lines) if l.strip() == '<script>')
script_close = next(i for i,l in enumerate(lines) if l.strip() == '</script>')

css_lines = lines[style_open+1 : style_close]      # content inside <style>
js_lines  = lines[script_open+1 : script_close]     # content inside <script>

# ── CSS split by /* ── … ─── */ section comments ─────────────────────────────
# Assign each CSS line to a bucket by scanning for marker comments.
SECTION_MAP = [
    (r'Body shell|reset|box-sizing|font-family|min-height|font-size', 'base'),
    (r'Sidebar',                'sidebar'),
    (r'Nav|nav-item|nav-text',  'sidebar'),
    (r'scroll area|overflow.*scroll', 'base'),
    (r'Header',                 'components'),
    (r'main.*stat|\.main|stat-card', 'components'),
    (r'card',                   'components'),
    (r'table|col-',             'table'),
    (r'comment.*badge|badge',   'table'),
    (r'action.btn|act-btn',     'table'),
    (r'SSH panel|\.ssh-',       'components'),
    (r'spinner|\.spin',         'components'),
    (r'modal|\.modal',          'components'),
    (r'no-result|\.no-result',  'table'),
    (r'scp-src-btn',            'table'),
    (r'target.*browser|\.tgt-', 'scp'),
    (r'deploy.*badge|\.deploy-','scp'),
    (r'multi.*scp|\.scp-',      'scp'),
    (r'mode.*toggle|\.mode-',   'scp'),
]

buckets_css = {'base': [], 'sidebar': [], 'components': [], 'table': [], 'scp': []}
current = 'base'

for line in css_lines:
    stripped = line.strip()
    if stripped.startswith('/*') and '──' in stripped:
        for pattern, name in SECTION_MAP:
            if re.search(pattern, stripped, re.I):
                current = name
                break
    buckets_css[current].append(line)

for name, content in buckets_css.items():
    path = f'{CSS}/{name}.css'
    with open(path, 'w', encoding='utf-8') as f:
        f.writelines(content)
    print(f'  wrote {path} ({len(content)} lines)')

# ── JS split by /* ── Section ── */ markers in the source ────────────────────
# The source has clear section banners like:
#   /* ── Init ── */
#   /* ── Sidebar ── */
# We use those as primary split boundaries, then fall back to function/var names.

# Section marker → JS file mapping (checked against the comment text)
JS_SECTION_MAP = [
    (r'Init',               'main'),
    (r'Sidebar',            'sidebar'),
]

# For lines NOT starting a section, we track which "functional group" they belong to
# by looking for top-level function/var declarations.
JS_FUNC_MAP = [
    # utils — pure helpers (no deps)
    (r'^function isLuckfox\b',    'utils'),
    (r'^function escHtml\b',      'utils'),
    (r'^function escAttr\b',      'utils'),
    (r'^function scpFmtSize\b',      'utils'),
    (r'^function scpFmtDate\b',      'utils'),
    (r'^function scpNowStr\b',       'utils'),
    (r'^function scpUpdateSelCount\b','scp'),
    (r'^function scpUpdateDeploy\b', 'scp'),
    # hosts — host list
    (r'^var allHosts\b',          'hosts'),
    (r'^var selectedIdx\b',       'hosts'),
    (r'^function renderTable\b',  'hosts'),
    (r'^function filterTable\b',  'hosts'),
    (r'^function selectRow\b',    'hosts'),
    (r'^function copyCmd\b',      'hosts'),
    (r'^function setLoading\b',   'hosts'),
    (r'^function load\b',         'hosts'),
    (r'^function rescan\b',       'hosts'),
    (r'^function hostListOpen\b', 'hosts'),
    (r'^function toggleHostList\b','hosts'),
    # comments
    (r'^function openAddModal\b', 'comments'),
    (r'^function closeModal\b',   'comments'),
    (r'^function doSaveComment\b','comments'),
    (r'^function deleteComment\b','comments'),
    # scp — all SCP vars + functions
    (r'^var scpSource\b',         'scp'),
    (r'^var scpTarget\b',         'scp'),
    (r'^var scpPath\b',           'scp'),
    (r'^var scpSelected\b',       'scp'),
    (r'^var scpDst\b',            'scp'),
    (r'^var scpCurrent',          'scp'),
    (r'^var scpSort',             'scp'),
    (r'^var tgtBrowsers\b',       'scp'),
    (r'^var scpColClass\b',       'scp'),
    (r'^function scpPopulate',    'scp'),
    (r'^function onSource',       'scp'),
    (r'^function scpBrowse\b',    'scp'),
    (r'^function scpRender',      'scp'),
    (r'^function scpSelect',      'scp'),
    (r'^function scpRefresh',     'scp'),
    (r'^function scpSrc',         'scp'),
    (r'^function scpDst',         'scp'),
    (r'^function deployMulti',    'scp'),
    (r'^function openChmod',      'scp'),
    (r'^function closeChmod',     'scp'),
    (r'^function chmod',          'scp'),
    (r'^function sortHdr\b',      'scp'),
    (r'^function tgtBrowse\b',    'scp'),
    (r'^function tgtRender\b',    'scp'),
    (r'^function tgtSelect\b',    'scp'),
    (r'^function addTarget\b',    'scp'),
    (r'^function removeTarget\b', 'scp'),
    (r'^function doRemove\b',     'scp'),
    (r'^function doMkdir\b',      'scp'),
    (r'^function doRename\b',     'scp'),
    (r'^function setMode\b',      'scp'),
]

JS_ORDER = ['utils', 'hosts', 'comments', 'sidebar', 'scp', 'main']
js_buckets = {k: [] for k in JS_ORDER}

cur = 'hosts'   # first lines start with allHosts vars
section_forced = False  # True when a /* ── Section ── */ comment took control

for line in js_lines:
    stripped = line.strip()

    # Check for section-level comment markers (highest priority)
    if stripped.startswith('/*') and '──' in stripped:
        matched_section = False
        for pattern, bucket in JS_SECTION_MAP:
            if re.search(pattern, stripped, re.I):
                cur = bucket
                section_forced = True
                matched_section = True
                break
        if not matched_section:
            section_forced = False  # reset so func markers work again

    # If not in a forced section, use function/var top-level markers
    if not section_forced:
        for pattern, bucket in JS_FUNC_MAP:
            if re.match(pattern, stripped):
                cur = bucket
                break

    js_buckets[cur].append(line)

for name in JS_ORDER:
    content = js_buckets[name]
    path = f'{JS}/{name}.js'
    with open(path, 'w', encoding='utf-8') as f:
        f.writelines(content)
    print(f'  wrote {path} ({len(content)} lines)')

# ── Write web/src/scanner.html (HTML skeleton) ────────────────────────────────
css_tags = '\n'.join(
    f'<link rel="stylesheet" href="assets/css/{n}.css">'
    for n in ['base', 'sidebar', 'components', 'table', 'scp']
)
js_tags = '\n'.join(
    f'<script src="assets/js/{n}.js"></script>'
    for n in JS_ORDER
)

head_lines = lines[:style_open]
between    = lines[style_close+1 : script_open]
tail_lines = lines[script_close+1:]

with open(f'{OUT}/scanner.html', 'w', encoding='utf-8') as f:
    f.writelines(head_lines)
    f.write(css_tags + '\n')
    f.writelines(between)
    f.write(js_tags + '\n')
    f.writelines(tail_lines)

print(f'  wrote {OUT}/scanner.html (HTML skeleton)')

# ── Write web/build.py (inliner) ──────────────────────────────────────────────
build_py = r'''#!/usr/bin/env python3
"""
build.py — Inline CSS and JS back into web/scanner.html for binary embedding.

Run from project root before `make` or `make pi`:
    python3 web/build.py

Reads:  web/src/scanner.html  +  web/src/assets/{css,js}/*
Writes: web/scanner.html
"""

import os, re

SRC_HTML  = 'web/src/scanner.html'
OUT_HTML  = 'web/scanner.html'
CSS_DIR   = 'web/src/assets/css'
JS_DIR    = 'web/src/assets/js'
CSS_ORDER = ['base', 'sidebar', 'components', 'table', 'scp']
JS_ORDER  = ['utils', 'hosts', 'comments', 'sidebar', 'scp', 'main']

def slurp(path):
    with open(path, encoding='utf-8') as f:
        return f.read()

css_block = '\n'.join(slurp(f'{CSS_DIR}/{n}.css') for n in CSS_ORDER)
js_block  = '\n'.join(slurp(f'{JS_DIR}/{n}.js')   for n in JS_ORDER)

with open(SRC_HTML, encoding='utf-8') as f:
    html = f.read()

# Replace <link> tags block with <style>...</style>
# Use lambda to avoid re interpreting backslashes in the replacement string.
css_link_pattern = r'(?:<link rel="stylesheet" href="assets/css/[^"]+\.css">\n)+'
css_repl = f'<style>\n{css_block}</style>\n'
html = re.sub(css_link_pattern, lambda m: css_repl, html)

# Replace <script src> tags block with inline <script>...</script>
js_script_pattern = r'(?:<script src="assets/js/[^"]+\.js"></script>\n)+'
js_repl = f'<script>\n{js_block}\n</script>\n'
html = re.sub(js_script_pattern, lambda m: js_repl, html)

with open(OUT_HTML, 'w', encoding='utf-8') as f:
    f.write(html)

print(f'[build.py] wrote {OUT_HTML} ({len(html.splitlines())} lines)')
'''

with open('web/build.py', 'w', encoding='utf-8') as f:
    f.write(build_py)
os.chmod('web/build.py', 0o755)
print('  wrote web/build.py')

print()
print('Done. Edit in web/src/. Then: python3 web/build.py && make pi')
