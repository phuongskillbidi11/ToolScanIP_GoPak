# Tasks: Permissions Button (Chmod via Modal)

- [ ] **Task 1: Add backend route `POST /api/chmod`**
  - File: `src/web.c`
  - Add route handler to parse payload fields: `ip`, `mode`, `recursive`, `paths[]`.
  - Register dispatch branch:
    - `else if (strcmp(path, "/api/chmod") == 0) route_api_chmod(fd, req);`
  - **Done when:** Endpoint is reachable and returns JSON responses.

- [ ] **Task 2: Implement backend validation**
  - File: `src/web.c`
  - Validate:
    - `ip` is `LOCAL` or passes `valid_ip`.
    - `mode` matches exactly 4 octal digits.
    - `paths` exists and entries pass `valid_path` + minimum length.
    - Cap paths at 64 and each copied path at 512 chars.
  - **Done when:** Invalid input paths/mode/ip return `{"error":"..."}` and do not execute chmod.

- [ ] **Task 3: Build and execute chmod command**
  - File: `src/web.c`
  - Build single command: `chmod [-R] <mode> '<path1>' '<path2>' ...`.
  - Execute locally for `ip=LOCAL`; execute remote command for non-local IP.
  - **Done when:** Valid request changes file mode and returns `{"ok":true}`.

- [ ] **Task 4: Add permissions modal markup**
  - File: `web/scanner.html`
  - Add modal block with:
    - owner/group/others permission table
    - special bits (setuid/setgid/sticky)
    - octal input
    - recursive checkbox
    - OK/Cancel actions
  - **Done when:** Modal renders and opens/closes correctly.

- [ ] **Task 5: Add frontend chmod logic**
  - File: `web/scanner.html`
  - Implement:
    - `chmodSync()` (checkboxes → octal)
    - `chmodOctalInput()` (octal → checkboxes)
    - `openChmodModal()`, `closeChmodModal()`, `applyChmod()`
  - POST to `/api/chmod` with selected source paths (`scpSelected`) and active source IP.
  - **Done when:** Modal values sync and successful request refreshes source browser.

- [ ] **Task 6: Add and wire Permissions button**
  - File: `web/scanner.html`
  - Insert **Permissions** button in source toolbar near existing source actions.
  - Update button state in source-selection update logic (`scpUpdateSrcBtns()`).
  - **Done when:** Button is disabled with no selected source paths and enabled when selected.

- [ ] **Task 7: Add modal styles**
  - File: `web/scanner.html`
  - Add CSS for chmod table, special-bits row, octal row, and recursive label.
  - **Done when:** Modal layout is readable and consistent with existing UI style.

- [ ] **Task 8: Build and validate behavior**
  - Commands:
    - `make`
    - Optional deploy as needed: `make pi && make deploy` (or `deploy2` / `native3` for target)
  - Execute test cases listed in `.plans/2026-05-22-permissions-button/tests.md`.
  - **Done when:** Build succeeds and all functional checks pass.
