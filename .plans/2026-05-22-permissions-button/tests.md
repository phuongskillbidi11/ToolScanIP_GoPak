# Tests: Permissions Button (Chmod via Modal)

## Build
- Run: `make`

## API-Level Checks

1. **Valid chmod request**
   - Command:
     ```bash
     curl -s -X POST http://<host>:<port>/api/chmod \
       -H 'Content-Type: application/json' \
       -d '{"ip":"192.168.20.111","paths":["/root/test.sh"],"mode":"0755","recursive":false}'
     ```
   - Expected: `{"ok":true}`

2. **Invalid mode rejected**
   - Command: same request with `"mode":"755"` or `"mode":"08aa"`
   - Expected: JSON error response.

3. **Invalid ip rejected**
   - Command: same request with `"ip":"bad-ip"`
   - Expected: JSON error response.

4. **Invalid or empty paths rejected**
   - Command: same request with `"paths":[]` or malformed path values
   - Expected: JSON error response.

## UI Functional Checks

1. **Permissions button state**
   - Steps:
     - Open Multi SCP source browser.
     - With no selection, inspect Permissions button.
     - Select one or more source entries.
   - Expected:
     - Disabled with no selection.
     - Enabled when `scpSelected.size > 0`.

2. **Modal open/close**
   - Steps:
     - Click Permissions button.
     - Close via Cancel and overlay click.
   - Expected:
     - Modal opens and closes correctly.
     - Default octal value is `0644` when opened.

3. **Checkboxes → octal sync**
   - Steps:
     - In modal, toggle Owner Execute on default 0644.
   - Expected:
     - Octal updates to `0744`.

4. **Octal → checkboxes sync**
   - Steps:
     - Type `0755` in octal field.
   - Expected:
     - Owner: RWX checked.
     - Group: R/X checked.
     - Others: R/X checked.

5. **Apply chmod success flow**
   - Steps:
     - Select source file(s), open modal, set mode, click OK.
   - Expected:
     - POST `/api/chmod` succeeds.
     - Modal closes.
     - Source browser refreshes.

6. **Remote result verification**
   - Command:
     ```bash
     ssh root@<ip> "ls -la /root/<file>"
     ```
   - Expected:
     - Permissions match requested mode (e.g., `-rwxr-xr-x` for `0755`).

## Regression Checks
- Multi SCP upload/deploy flow still works.
- Existing source actions (Refresh/Mkdir/Delete/Rename) still work.
- No JS console errors in normal source browsing and selection flow.
