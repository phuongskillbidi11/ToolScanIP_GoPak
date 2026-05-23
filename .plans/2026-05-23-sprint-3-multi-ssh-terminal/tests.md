# Multi-IP SSH Terminal — tests.md

All tests are manual (browser). Run after T4 (build + deploy + process restart).

---

## Pre-flight: binary content checks

```bash
grep -c "sshTermAddSession"    web/scanner.html   # ≥ 1
grep -c "ssh-terminal-tabs"    web/scanner.html   # ≥ 1
grep -c "ssh-terminal-ip-sel"  web/scanner.html   # ≥ 1
grep -c "ssh-terminal-tab {"   web/scanner.html   # ≥ 1  (CSS rule inlined)
```

---

## Test A — IP selector shows scanned hosts

1. Run a scan (click Rescan or wait for auto-scan).
2. Open SSH Terminal panel.
3. The `<select>` must contain one option per discovered host (IP + comment/hostname).
4. Last option must be `— custom IP… —`.
5. "Add" without selecting → error message "Select a device or enter a custom IP."

---

## Test B — Custom IP option

1. Select `— custom IP… —` from the dropdown.
2. A text input appears next to the dropdown.
3. Type an invalid string (e.g. `notanip`) → click Add → error "Invalid IP address: notanip".
4. Type a valid IP → click Add → tab opens, no error.
5. Re-selecting a non-custom option hides the custom input.

---

## Test C — Add session / tab appears

1. Select a scanned host from the dropdown.
2. Click "Add".
3. A tab appears in the tab strip with the IP as its label.
4. Output area is empty (no commands yet).
5. Command input becomes enabled.
6. Run button becomes enabled.

---

## Test D — Duplicate IP not double-tabbed

1. Open a session for IP X (tab appears).
2. Select the same IP X again from the dropdown.
3. Click "Add".
4. No second tab is created — existing tab is activated.

---

## Test E — Multiple tabs, independent output

1. Open session for Host A.
2. Run `whoami` on Host A → output appears.
3. Click "Add" for Host B — second tab appears. Host A tab stays.
4. Run `df -h` on Host B → output appears.
5. Click Host A tab → output shows `$ whoami` / `root`, NOT `df -h` output.
6. Click Host B tab → output shows `$ df -h` output, not Host A history.

---

## Test F — Close tab

1. Open sessions for Host A and Host B (two tabs).
2. Make Host A active. Click × on Host A tab.
3. Host A tab disappears. Host B becomes active.
4. Output shows Host B's history.
5. Close Host B tab → tab strip is empty, output clears, command input disabled.

---

## Test G — Close non-active tab

1. Open sessions for Host A and Host B. Activate Host B.
2. Click × on Host A tab (not active).
3. Host B remains active and its output is unchanged.

---

## Test H — Command sent to active tab's IP only

1. Open sessions for Host A and Host B.
2. Activate Host A. Run `hostname`.
3. Activate Host B. Check output — Host B output must not contain Host A's response.
4. Run `hostname` on Host B — must return Host B's hostname.

---

## Test I — History preserved on tab switch

1. Run 5 commands on Host A.
2. Switch to Host B, run 2 commands.
3. Switch back to Host A — all 5 commands and their output must still be visible.

---

## Test J — SCP source sync pre-fills selector

1. In SCP panel, select a source host (e.g. 192.168.1.5).
2. Open SSH Terminal panel.
3. The IP selector must have 192.168.1.5 pre-selected.
4. No tab is opened automatically (Add must be clicked explicitly).
5. Change SCP source → selector value updates.
6. If SSH Terminal already has an active session, the selector update does NOT
   close any existing tabs.

---

## Test K — Scan update refreshes selector

1. Open SSH Terminal panel.
2. Click Rescan.
3. Selector options update to reflect new scan results (new hosts appear, gone
   hosts disappear). Existing sessions are unaffected.

---

## Test L — 50 KB history cap

1. Run commands that produce large output until session history exceeds 50 KB.
2. Old lines at the top are trimmed — no browser freeze, no JS error in console.

---

## Test M — System Monitor regression (Chart.js not broken)

1. Open System Monitor panel.
2. Charts render and update every 3 s.
3. No console errors about Chart.js or script order.

---

## Failure triage

| Symptom | Likely cause | Check |
|---|---|---|
| Selector empty / no hosts | `allHosts` not yet populated when panel opens | `sshTermSetPinned` calls `sshTermPopulateSel()` — verify T3 |
| Custom input never appears | `sshTermSelChange()` not called on `onchange` | Check T1: `onchange="sshTermSelChange()"` on `<select>` |
| Tab strip not visible | `.ssh-terminal-tabs:empty { display:none }` — tabs div is empty | Check T3 `sshTermRenderTabs()` |
| Active tab not highlighted | Missing `active` class | Check `sshTermActivate()` and `sshTermRenderTabs()` |
| Output not updating after command | `sshTermRenderOutput()` not called in `.then()` | Check T3 run flow |
| `sshTermSessions is not defined` in console | hoisting guard missing on a sidebar-called function | Check T3: all 3 guards present |
| Build output unchanged | `python3 web/build.py` not re-run after T3 | Run build again |
