# Multi-IP SSH Terminal — DECISION_LOG.md

Sprint: 2026-05-23-sprint-3-multi-ssh-terminal

---

## D1 — Output layout: tabs (one per IP) over split panels or broadcast

**Decision:** Tabs — one tab per connected device, single output div with
history swapped on activation.

**Rejected:**
- Split panels (all outputs visible at once) — complex DOM management, poor
  readability on narrow screens
- Broadcast mode (one command → all devices) — different use case; tabs let
  the user work with each device independently

**User input:** User explicitly selected "Tabs" when presented with mockups
of all three options.

**Implementation:** History is stored as a plain string in each session object
(`{ ip, history, inFlight }`). On tab activation, `sshTermRenderOutput()` sets
`out.textContent = session.history`. No per-tab DOM elements — simpler, no
memory leak from orphan DOM nodes.

---

## D2 — IP selector: allHosts global over /api/scan fetch

**Decision:** Populate `ssh-terminal-ip-sel` from the `allHosts` global
(declared in `hosts.js:1`, updated at `hosts.js:157`).

**Rejected:** Fresh `fetch('/api/scan')` call inside `sshTermPopulateSel()`.

**Reasoning:** `allHosts` is always current (updated on every scan). A fresh
fetch would add latency, an extra request, and a loading state. The SCP panel
uses `allHosts` for the same purpose (`scpPopulateSources()` at `scp.js:24`).
Consistency with SCP is intentional.

**Refresh trigger:** `sshTermPopulateSel()` is called from `sshTermSetPinned(true)`,
so the selector is fresh each time the panel is opened. It should also be called
from `renderHosts()` in hosts.js when a new scan result arrives, following the
same pattern SCP uses.

---

## D3 — State model: sessions array over per-tab DOM state

**Decision:** `var sshTermSessions = []` — JS array of session objects.
History lives in `session.history` string, not in DOM elements.

**Rejected:** Storing history in hidden `<div>` elements per tab.

**Reasoning:** DOM-stored state creates memory leaks when tabs are closed
(detached nodes). JS object state is trivially garbage-collected on splice.
The single output div with `textContent` swap is simpler to reason about and
has no layout quirks.

---

## D4 — syncSSHTabsFromSCP: pre-fill selector, do not auto-open session

**Decision:** `syncSSHTabsFromSCP()` sets `ssh-terminal-ip-sel` value only.
The user must click "Add" explicitly to open a session.

**Rejected:** Auto-opening a session tab when SCP source changes.

**Reasoning:** Auto-opening would be surprising and potentially unwanted — the
user may be changing SCP source for file operations, not because they want to
SSH there. Pre-filling the selector is a convenience; clicking Add is intent.

---

## D5 — Hoisting guard: var not let/const, guard in every externally-called function

**Decision:** `var sshTermSessions` (not `let`/`const`). Every function callable
from `sidebar.js` before `ssh-terminal.js` runs to completion must have
`if (!sshTermSessions) return` as the first statement.

**Why:** `var` declarations are hoisted but not initialized. When `sidebar.js`
calls `applyPinState()` → `sshTermSetPinned()` during page init (sidebar.js
runs before ssh-terminal.js in JS_ORDER), `sshTermSessions` exists as a name
but its value is `undefined`. The guard catches this. `let`/`const` would throw
a ReferenceError instead of returning `undefined`, making the guard impossible.

**Precedent:** Same pattern established in Sprint 2 for `sysmonitor.js`
(`if (!sysMonitorState) return`) and the original Sprint 2 `ssh-terminal.js`.

**Functions requiring the guard in Sprint 3:**
- `sshTermRun()` — called from HTML `onclick`
- `syncSSHTabsFromSCP()` — called from scp.js
- `sshTermSetPinned()` — called from sidebar.js

---

## D6 — No backend changes in this sprint

**Decision:** `route_api_ssh_exec()` in `src/web.c` is unchanged.

**Reasoning:** The route is already stateless — it takes `{ip, command}` and
returns `{stdout, stderr, exit_code, elapsed_ms}`. Multi-session is entirely
a front-end concern: the JS maintains session state and fires one POST per
command regardless of how many tabs are open. No server-side session tracking
is needed.
