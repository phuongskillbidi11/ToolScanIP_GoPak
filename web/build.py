#!/usr/bin/env python3
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
CSS_ORDER = ['base', 'sidebar', 'components', 'table', 'scp', 'sysmonitor', 'ssh-terminal']
JS_ORDER  = ['utils', 'hosts', 'comments', 'sidebar', 'scp', 'ssh-terminal', 'sysmonitor', 'main']

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
