# SSH Terminal Card — tests.md

All tests are manual (browser + curl). No automated test harness exists in this project.
Run after T9 (build + deploy) is complete.

---

## Pre-flight: binary content checks

```bash
# ssh-terminal JS is present in the built HTML
grep -c "sshTermRun" web/scanner.html
# expected: ≥ 1

# ssh-terminal CSS is present
grep -c "ssh-terminal-output" web/scanner.html
# expected: ≥ 1

# route registered in C binary (symbol present)
nm ipscanner | grep route_api_ssh_exec
# expected: one line with T symbol
```

---

## API-layer tests (curl)

Run these against the deployed board (replace IP with board's Tailscale address).

### Test A — valid command, known host

```bash
curl -s -X POST http://BOARD:8080/api/ssh-exec \
     -H 'Content-Type: application/json' \
     -d '{"ip":"TARGET_IP","command":"whoami"}'
```

Expected:
```json
{"stdout":"root\n","stderr":"","exit_code":0,"elapsed_ms":NNN}
```

### Test B — empty command rejected

```bash
curl -s -X POST http://BOARD:8080/api/ssh-exec \
     -H 'Content-Type: application/json' \
     -d '{"ip":"TARGET_IP","command":""}'
```

Expected: `{"error":"empty command"}`

### Test C — invalid IP rejected

```bash
curl -s -X POST http://BOARD:8080/api/ssh-exec \
     -H 'Content-Type: application/json' \
     -d '{"ip":"not-an-ip","command":"ls"}'
```

Expected: `{"error":"invalid ip"}`

### Test D — deny-char blocked (semicolon)

```bash
curl -s -X POST http://BOARD:8080/api/ssh-exec \
     -H 'Content-Type: application/json' \
     -d '{"ip":"TARGET_IP","command":"ls ; rm -rf /"}'
```

Expected: `{"error":"unsafe character in command"}`

### Test E — single-quote stripped (not rejected)

```bash
curl -s -X POST http://BOARD:8080/api/ssh-exec \
     -H 'Content-Type: application/json' \
     -d "{\"ip\":\"TARGET_IP\",\"command\":\"echo 'hello'\"}"
```

Expected: stdout contains `hello` (single-quotes stripped, `echo hello` runs fine), exit_code 0.
Must NOT return an error response.

### Test F — sshpass not found path

Temporarily rename sshpass on board, confirm response:
```json
{"stdout":"","stderr":"sshpass not found","exit_code":-1,"elapsed_ms":0}
```
Restore sshpass after.

---

## Browser UI tests

Open the deployed dashboard in a browser.

### Test G — SSH Terminal nav item is active (not greyed)

1. Load dashboard.
2. Sidebar: "SSH Terminal" nav item must be clickable (no disabled styling).
3. Click it → SSH Terminal card appears.
4. Click it again → card hides.
5. At least one other panel remains visible (pin system prevents all-hidden).

### Test H — Connect / Disconnect flow

1. Open SSH Terminal panel.
2. Leave IP blank → click Connect → error message "Enter an IP address." appears.
3. Enter a valid host IP → click Connect.
4. IP field becomes disabled; "Run" button becomes enabled; status shows "Connected to …".
5. Click "Disconnect" → IP field re-enabled; Run button disabled; status shows "Disconnected."

### Test I — Command execution and output

1. Connect to a known host.
2. Type `ls /root` → click Run (or press Enter).
3. Output area shows `$ ls /root` prompt line followed by directory listing.
4. Status bar shows `exit 0  (NNNms)`.
5. Type a second command → output appends below first output.

### Test J — Bad command shows stderr

1. Connect to a known host.
2. Type `ls /nonexistent_path_xyz` → Run.
3. `[stderr]` line appears in output.
4. Status shows non-zero exit code.

### Test K — SCP → SSH IP sync

1. SCP panel: select a source host from the dropdown.
2. Open SSH Terminal panel.
3. IP input field must be pre-filled with the selected source IP.
4. Change SCP source → SSH Terminal IP updates (if not yet connected).
5. Connect SSH → change SCP source → SSH Terminal IP does NOT change
   (syncSSHTabsFromSCP returns early when connected).

### Test L — Pin state persists across panel toggles

1. Pin SSH Terminal (thumbtack icon).
2. Navigate to another panel.
3. SSH Terminal card remains visible.
4. Unpin → card hides.

### Test M — sysmonitor still works (regression)

1. Click System Monitor nav item → card appears.
2. Data refreshes every 3 s (badge shows "Live").
3. Charts render (Chart.js CDN still loads correctly — ssh-terminal.js before CDN
   must not break the CDN load order).

---

## Failure triage

| Symptom | Likely cause | Check |
|---|---|---|
| SSH Terminal card never appears | sidebar.js missing `'ssh-terminal'` registration | T5 |
| `sshTermRun is not defined` in console | ssh-terminal.js not in JS_ORDER or not in Group 1 | T1, T4b |
| All JS broken after build | build.py JS_ORDER has a typo | `python3 web/build.py` + check output |
| `route_api_ssh_exec` returns 404 | dispatch line not added | T8 |
| Single-quote command causes SSH to hang | strip loop not added or placed after snprintf | T7 |
| syncSSHTabsFromSCP not defined at page load | scp.js calls it before ssh-terminal.js loads | check script tag order in scanner.html |
| Chart.js charts broken | ssh-terminal.js script tag placed after CDN instead of before | T4b |
