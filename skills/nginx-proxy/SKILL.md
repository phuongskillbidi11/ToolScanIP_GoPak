# Skill: nginx-proxy

## Purpose
Generate and manage nginx reverse-proxy configs that expose Luckfox device web UIs
(port 8000 on each device) as remotely accessible URLs through the Raspberry Pi's
public/Tailscale IP.

## When to use
- First-time nginx setup on the Pi
- After adding or removing Luckfox devices from the network
- After a router reset changes device IPs (re-run to refresh)
- Clicking **Rescan** on the dashboard is equivalent (no terminal needed)

## Files involved

| File | Role |
|------|------|
| `scripts/gen-nginx.sh` | Main script — scan + write configs + start daemon |
| `/etc/nginx/conf.d/ipscan-proxy.conf` | Generated: one server block per Luckfox device |
| `/etc/nginx/conf.d/ipscan-landing.conf` | Generated: port 80 dashboard server block |
| `/etc/nginx/conf.d/ipscan-scanner.conf` | Generated: port 8080 → ipscanner:8181 proxy |
| `/var/www/ipscan/index.html` | Generated: clickable device landing page |
| `/tmp/ipscan_result.json` | Temporary scan output used by gen-nginx.sh |
| `/tmp/ipscanner-web.log` | ipscanner daemon stdout log |

## Step-by-step instructions

### 1. Install nginx (once on Pi)

```bash
sudo apt install nginx
```

### 2. Run the full setup

```bash
cd ~/ToolScanIP
sudo ./gen-nginx.sh
```

### 3. What gen-nginx.sh does (in order)

```
[1/6] Scan network → /tmp/ipscan_result.json
[2/6] Write /etc/nginx/conf.d/ipscan-proxy.conf
[3/6] Write /var/www/ipscan/index.html  (landing page)
[4/6] Write /etc/nginx/conf.d/ipscan-landing.conf  (port 80)
[5/6] Start ipscanner daemon on port 8181 (background)
      Write /etc/nginx/conf.d/ipscan-scanner.conf  (port 8080 → 8181)
[6/6] nginx -t && nginx -s reload
```

### 4. Port mapping formula

For each Luckfox device at `192.168.20.X`:

```
Public port = 9000 + X
→ 192.168.20.12  becomes  http://100.66.44.107:9012/
→ 192.168.20.26  becomes  http://100.66.44.107:9026/
→ 192.168.20.34  becomes  http://100.66.44.107:9034/
```

The nginx server block for each device:

```nginx
server {
    listen 9026;
    server_name _;
    location / {
        proxy_pass         http://192.168.20.26:8000;
        proxy_http_version 1.1;
        proxy_set_header   Upgrade    $http_upgrade;
        proxy_set_header   Connection "upgrade";
        proxy_set_header   Host       $host;
        proxy_read_timeout 3600s;
    }
}
```

WebSocket upgrade headers are included because Luckfox UIs use `ws://` for
live data monitoring.

### 5. Resulting URLs (Pi Tailscale IP: `100.66.44.107`)

| URL | What |
|-----|------|
| `http://100.66.44.107/` | Static device dashboard |
| `http://100.66.44.107:8080/` | Live scanner UI (ipscanner web) |
| `http://100.66.44.107:9010/` | Luckfox at 192.168.20.10 |
| `http://100.66.44.107:9012/` | Luckfox at 192.168.20.12 |
| ... | |

### 6. Rescan without terminal

Click **Rescan** on `http://100.66.44.107/`. This calls `GET /regen` which:

1. Rescans the network
2. Rewrites `/etc/nginx/conf.d/ipscan-proxy.conf`
3. Rewrites `/var/www/ipscan/index.html`
4. Runs `nginx -s reload`
5. Browser auto-reloads with fresh device list

### 7. Which devices get proxied?

Only hosts whose comment contains **both** `"Line"` and `"[GM"`:

```python
# In gen-nginx.sh
devices = [h for h in hosts
           if 'Line' in h.get('comment','') and '[GM' in h.get('comment','')]
```

Label devices correctly with `./ipscanner -C "MAC=Line X [GMY]"` to include them.

## Optional scripts (if any)

```bash
# Check nginx config is valid
nginx -t

# Reload nginx after manual config changes
systemctl reload nginx

# View generated proxy config
cat /etc/nginx/conf.d/ipscan-proxy.conf

# Check ipscanner daemon status
pgrep -a ipscanner
cat /tmp/ipscanner-web.log

# Manually kill and restart daemon
sudo pkill -f ipscanner
sudo nohup ~/ToolScanIP/ipscanner -i eth0 -w 8181 > /tmp/ipscanner-web.log 2>&1 &
```

## Advanced examples / cross-references

- The `/regen` route in `src/web.c` replicates gen-nginx.sh steps 2-4 in C,
  then calls `system("nginx -t && nginx -s reload")`
- Device identification rule (Line + [GM) is defined in both `gen-nginx.sh`
  (Python) and `src/web.c` (C) — keep them in sync if you change the rule
- See `skills/comments/SKILL.md` for how to label devices correctly
- See `skills/web-ui/SKILL.md` for the `/regen` route implementation
- See `skills/deploy-pi/SKILL.md` for the full deploy workflow
