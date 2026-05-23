---
skill: multi-scp
last_updated: 2026-05-22
updated_after_sprint: 0
confidence: low
status: draft
note: "Update confidence and status after first sprint using this skill."
---

# Skill: multi-scp

## What it does
Adds a "Multi SCP — File Transfer" panel to the Live Scanner UI (`:8080`).
Users can browse files on any scanned host via SSH, select files/folders,
choose one or more Luckfox target devices, and push them all in parallel —
directly from the browser. Used for firmware updates and config distribution.

## Architecture

```
Browser  →  POST /api/multi-scp  →  web.c route_api_multi_scp()
                                      ├─ pthread per target
                                      │   └─ sshpass scp root@SOURCE:PATH root@TARGET:DIR
                                      └─ pthread_join all  →  JSON results

Browser  →  GET /api/browse?ip=&path=  →  web.c route_api_browse()
                                             └─ sshpass ssh root@IP "ls -la PATH"
                                                  →  parsed JSON entries
```

## API endpoints

### GET /api/browse
Query params: `ip=<source_ip>`, `path=<absolute_path>`

Response:
```json
{
  "path": "/root",
  "entries": [
    {"name": "firmware.bin", "type": "f", "size": 1234567},
    {"name": "config",       "type": "d", "size": 0},
    {"name": "link",         "type": "l", "size": 10}
  ]
}
```

Error response: `{"error": "SSH connection failed"}`

- Uses `sshpass -p luckfox ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5`
- Parses `ls -la` output: skips 8 tokens to extract name, reads token 4 for size
- Skips `.` and `..` entries
- Strips symlink ` -> target` suffix from names

### POST /api/multi-scp
Content-Type: `application/json`

Request body:
```json
{
  "source_ip":    "192.168.3.60",
  "source_paths": ["/root/firmware.bin", "/root/config.json"],
  "target_ips":   ["192.168.3.96", "192.168.3.138", "192.168.3.140"],
  "target_dir":   "/root/"
}
```

Response:
```json
{
  "results": [
    {"target": "192.168.3.96",  "status": "ok",   "output": ""},
    {"target": "192.168.3.138", "status": "fail",  "output": "Permission denied"},
    {"target": "192.168.3.140", "status": "ok",   "output": ""}
  ]
}
```

- One `pthread` per target, all launched simultaneously
- Command per thread: `sshpass -p luckfox scp -o StrictHostKeyChecking=no -o ConnectTimeout=30 -r "root@SOURCE:FILE1" "root@SOURCE:FILE2" "root@TARGET:DIR"`
- If exit_code != 0 and output contains "Host key verification failed":
  - Runs `ssh-keygen -R <target_ip>` once
  - Retries the scp command
- Max parallel targets: 16 (`MAX_SCP_TARGETS`)
- Max source files: 8 (`MAX_SCP_FILES`)

## Security validation

Both `valid_ip()` and `valid_path()` run before any shell execution:

```c
valid_ip(ip)   // must match d.d.d.d with each octet 0-255
valid_path(p)  // must start with /; rejects ; | & ` $ ' \n \r
```

## Web UI (scanner.html)

The "Multi SCP" card is **collapsed by default** and expands on click.

| Section | What it does |
|---------|-------------|
| Source Device | Dropdown of all scanned hosts + custom IP option |
| File Browser | Calls `/api/browse`, shows dirs/files with checkboxes; click folder to navigate, click file to toggle selection |
| Breadcrumb nav | Clickable path components to navigate up |
| Target Devices | Checkboxes for all Luckfox hosts (comment contains "Line" and "[GM") |
| Destination | Text field, default `/root/` |
| Deploy button | Enabled when ≥1 file and ≥1 target selected; calls `/api/multi-scp` |
| Results | Per-target OK/FAIL badge + scp output text |

Key JS globals:
- `scpSourceIP` — currently selected source IP
- `scpPath` — current directory being browsed
- `scpSelected` — `Set` of absolute paths selected for transfer

Key JS functions:
- `toggleSCPPanel()` — expand/collapse; triggers `scpPopulateSources()` + `scpPopulateTargets()`
- `scpBrowse(path)` — fetch `/api/browse` and render file list
- `deployMultiSCP()` — POST `/api/multi-scp` and render results

## nginx proxy (gen-nginx.sh)

Both new routes are proxied in the landing page nginx block at port 80:

```nginx
location /api/browse    { proxy_pass http://127.0.0.1:8181/api/browse;    proxy_read_timeout 15s; }
location /api/multi-scp { proxy_pass http://127.0.0.1:8181/api/multi-scp; proxy_read_timeout 90s; client_max_body_size 1m; }
```

## Build & deploy

```bash
# After any change to web/scanner.html:
xxd -i web/scanner.html | sed 's/web_scanner_html/scanner_html/g' > src/scanner_html.h
make pi && make deploy2   # intercom Pi (wlan0)
make pi && make deploy    # isoft Pi (eth0)
```

Then on the Pi:
```bash
sudo ./scripts/gen-nginx.sh wlan0   # restarts daemon + reloads nginx
```

## Credentials

All SSH/SCP operations use: `root` / `luckfox` (same as terminal auto-login).
The Pi running ipscanner must be able to reach both SOURCE and TARGET IPs.

## Limitations

- Source paths may not contain: `;  |  &  \`  $  '  \n  \r`
- Source must be on the same subnet as the Pi (direct IP routing)
- No progress streaming — browser waits for all transfers to complete (max ~30s)
- No file upload from browser; source must be a reachable host on the network
