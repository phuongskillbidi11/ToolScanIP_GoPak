---
skill: deploy-pi
last_updated: 2026-05-22
updated_after_sprint: 0
confidence: low
status: draft
note: "Update confidence and status after first sprint using this skill."
---

# Skill: deploy-pi

## Purpose
Upload the cross-compiled ARM binary and `gen-nginx.sh` to the Raspberry Pi,
then launch the full stack: ipscanner web daemon + nginx reverse proxy for all
Luckfox devices.

## When to use
- After any source code change that needs to run on the Pi
- After editing `scripts/gen-nginx.sh`
- After editing `web/scanner.html` (re-embed + redeploy)
- When adding new Luckfox devices to the network

## Files involved

| File | Role |
|------|------|
| `Makefile` | `deploy` target — cross-compiles and scp's to Pi |
| `build/ipscanner-pi` | ARM binary to upload |
| `scripts/gen-nginx.sh` | Uploaded to Pi, run as root to configure everything |
| Pi: `~/ToolScanIP/ipscanner` | Binary on the Pi |
| Pi: `~/ToolScanIP/gen-nginx.sh` | Script on the Pi |

## Step-by-step instructions

### 1. Cross-compile and upload (from WSL/local machine)

```bash
make deploy
# Prompts for SSH password once per scp/ssh call
# Uploads: build/ipscanner-pi → ~/ToolScanIP/ipscanner
#          scripts/gen-nginx.sh → ~/ToolScanIP/gen-nginx.sh
```

> The deploy uses `scp` to `/tmp/ipscanner_new` then `mv` to the final path.
> This avoids "Text file busy" if the old binary is still running.

### 2. SSH into the Pi

```bash
ssh admin@100.66.44.107
cd ~/ToolScanIP
```

### 3. Run the full setup script

```bash
sudo ./gen-nginx.sh
```

This single command:
1. Scans the network (`ipscanner -j /tmp/ipscan_result.json`)
2. Writes `/etc/nginx/conf.d/ipscan-proxy.conf` (one block per Luckfox device)
3. Writes `/var/www/ipscan/index.html` (static landing page)
4. Kills any old ipscanner daemon, starts new one on port **8181** (internal)
5. Writes `/etc/nginx/conf.d/ipscan-scanner.conf` (proxies port 8080 → 8181)
6. Reloads nginx

### 4. Verify

Open in browser:

| URL | What |
|-----|------|
| `http://100.66.44.107/` | Device dashboard (port 80) |
| `http://100.66.44.107:8080/` | Live scanner UI |
| `http://100.66.44.107:9026/` | Luckfox at 192.168.20.26 |

### 5. If a device disappears or a new one appears

Just click **Rescan** on `http://100.66.44.107/` — this calls `/regen` which
rescans, rewrites nginx config, rebuilds the landing page, and reloads nginx.
No terminal required.

## Optional scripts

```bash
# Kill the running daemon manually on Pi
sudo pkill -f ipscanner

# Check daemon log
cat /tmp/ipscanner-web.log

# Test nginx config before reload
nginx -t
```

## Advanced examples / cross-references

- Change Pi address: edit `PI_HOST` in `Makefile`
- Makefile deploy target uses `mv` trick to avoid "Text file busy":
  ```makefile
  scp $(PI_TARGET) $(PI_HOST):/tmp/ipscanner_new
  ssh $(PI_HOST) "mv /tmp/ipscanner_new $(PI_DIR)/ipscanner"
  ```
- See `skills/nginx-proxy/SKILL.md` for nginx config details
- See `skills/build/SKILL.md` for cross-compile setup
