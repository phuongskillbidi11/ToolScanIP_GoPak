# Sprint Summary — Multi-IP SSH Terminal

Sprint: 2026-05-23-sprint-3-multi-ssh-terminal
Status: [x] Complete

---

## Dates

| Milestone | Date |
|---|---|
| Plan written | 2026-05-23 |
| Implementation started | 2026-05-23 |
| Build verified | 2026-05-23 |
| Deployed to board | 2026-05-23 (make deploy2) |
| Browser tests passed | pending manual verification |

---

## Tasks completed

| Task | Status | Notes |
|---|---|---|
| T1 scanner.html card HTML | [x] | grep confirmed ssh-terminal-ip-sel and ssh-terminal-tabs present |
| T2 ssh-terminal.css tab strip | [x] | grep -c returned 6 matches |
| T3 ssh-terminal.js rewrite | [x] | node --check passed, 3 hoisting guards confirmed |
| T4 Build + deploy | [x] | web/scanner.html 6446 lines, content check returned 18, make deploy2 succeeded |

---

## What was shipped

- SSH Terminal panel now supports multiple simultaneous sessions, one tab per IP
- IP selector is a dropdown populated from `allHosts` (live scanned hosts) with
  a "custom IP…" option that reveals a text input — same pattern as scp-source-sel
- Each tab has independent output history preserved across tab switches
- Duplicate IPs are silently deduplicated (existing tab activated instead)
- × button closes individual tabs; last tab closing clears output and disables input
- SCP source selection pre-fills the IP selector (does not auto-open a session)
- 50 KB history cap prevents unbounded memory growth in long sessions
- No backend changes — route_api_ssh_exec() handles one command at a time, stateless

---

## Deviations from spec

None. All tasks implemented as specified in tasks.md.

---

## Errors encountered

None logged in errors.log.

---

## Known issues / follow-up

- ~~`sshTermPopulateSel()` not wired into `renderHosts()`~~ — **Fixed post-sprint:**
  added `if (typeof sshTermPopulateSel === 'function') sshTermPopulateSel();`
  at end of scan success flow in `hosts.js`. Selector now auto-refreshes on
  every scan while the panel is open.
- Manual browser tests (tests.md Tests A–M) still pending on physical board.
