# Spec: Permissions Button (Chmod via Modal)

## Objective
Add a **Permissions** action in Multi SCP source controls that opens a chmod modal and applies permissions to selected source paths through a new backend endpoint.

## Scope
- **In scope:**
  - Add `POST /api/chmod` in `src/web.c`.
  - Validate input: `ip`, `mode`, and `paths[]`.
  - Execute `chmod` locally for `ip=LOCAL`, and remotely for device IPs.
  - Add a WinSCP-style permissions modal in `web/scanner.html`.
  - Add checkbox↔octal synchronization JS.
  - Add a **Permissions** button in source toolbar.
  - Enable the button when at least one source path is selected.
- **Out of scope:**
  - Any change to scan pipeline, host table behavior, or deploy scripts.
  - Any new auth, privilege escalation, or role model.
  - Changes to unrelated API routes.

## User Flow / Data Flow
1. User selects one or more source entries in Multi SCP.
2. **Permissions** button becomes enabled.
3. Clicking button opens modal initialized to `0644`.
4. User modifies checkboxes and/or octal field; values stay synchronized.
5. On OK, frontend POSTs `{ ip, paths, mode, recursive }` to `/api/chmod`.
6. Backend validates payload, builds one `chmod` command, executes local/remote, and returns JSON result.
7. On success, modal closes and source browser refreshes.

## Technical Constraints
- Use existing plain HTML/CSS/JS style in `web/scanner.html`.
- Reuse existing backend patterns for request parsing and JSON responses in `src/web.c`.
- `mode` must be exactly 4 octal digits (`[0-7]{4}`).
- `paths` max count: 64; each path max length: 512.
- Preserve existing endpoint response style:
  - success: `{"ok":true}`
  - failure: `{"error":"..."}`

## Design Decisions
- **Chosen approach:** Add a dedicated `/api/chmod` endpoint and keep chmod UI logic fully in scanner frontend.
- **Alternatives considered:** Per-file chmod calls (rejected; less efficient and noisier UX).
- **Assumptions:** Existing helpers (`valid_ip`, `valid_path`, SSH execution helper, JSON utility pattern) are available and reusable in `src/web.c`.

## Success Criteria
- [ ] Source toolbar shows **Permissions** button and toggles enabled state with source selection.
- [ ] Modal opens, defaults to `0644`, and checkbox/octal synchronization works both directions.
- [ ] Backend rejects invalid payloads with clear JSON error.
- [ ] Valid requests run chmod on LOCAL or remote host and return `{"ok":true}`.
- [ ] UI closes modal and refreshes source browser after success.
