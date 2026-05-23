---
skill: deploy
last_updated: 2026-05-22
updated_after_sprint: 0
confidence: high
status: active
note: "Update if board IPs, passwords, or interfaces change."
---

# Skill: deploy

## Purpose

Cross-compile and deploy ipscanner to embedded Linux targets via SSH/SCP.
Load this skill for any task involving Makefile deploy targets, sshpass, or board-specific config.

## When to use

- Deploying to any board (Pi 1, Pi 2, Orange Pi)
- Adding a new deploy target to the Makefile
- Changing deployment scripts (`scripts/gen-nginx.sh`, etc.)
- Troubleshooting SSH connectivity or GLIBC mismatches

## Targets

| Name | Tailscale IP | Interface | Arch | Auth |
|------|-------------|-----------|------|------|
| Pi 1 (isoft) | `admin@100.66.44.107` | eth0 | armhf | SSH key |
| Pi 2 (intercom) | `intercom@100.70.31.100` | wlan0 | armhf | sshpass `admin@123` |
| Orange Pi (isoftembedded) | `root@100.101.117.23` | lan0 | aarch64 | SSH key |

## Makefile targets

```bash
make deploy    # cross-compile armhf + scp to Pi 1
make deploy2   # cross-compile armhf + scp to Pi 2 (sshpass)
make build2    # make pi + make deploy2 in one step
make deploy3   # cross-compile aarch64 + scp to Orange Pi
make native3   # scp sources to OrangePi, compile on-board (avoids GLIBC mismatch)
```

## Files deployed

Each `make deploy*` copies:
- Binary (`ipscanner`) → `~/ToolScanIP/ipscanner`
- `scripts/gen-nginx.sh` → `~/ToolScanIP/scripts/gen-nginx.sh`
- `scripts/fetch-machine-names.sh` → `~/ToolScanIP/scripts/`
- `scripts/mqtt-sync-comments.sh` → `~/ToolScanIP/scripts/`

## Post-deploy steps (run on board)

```bash
# Full restart (nginx + daemon):
cd ~/ToolScanIP && sudo ./scripts/gen-nginx.sh [eth0|wlan0|lan0]

# Quick test without nginx:
sudo ./ipscanner -i eth0 -w 8080
```

**Note:** `sudo gen-nginx.sh` must be run manually via SSH after each deploy —
sshpass + sudo + non-interactive does not work cleanly.

## Constraints

- Tailscale must be active on both sides for 100.x.x.x addresses
- Binary architecture must match board (armhf ≠ aarch64 — wrong binary = `Exec format error`)
- GLIBC: Pi boards have 2.31; the Ubuntu toolchain targets 2.34 for aarch64 → link pthread statically
- `make native3` compiles on-board using the board's own gcc — use when GLIBC mismatch is suspected

## Step-by-step: adding a new board

1. Add host vars to `Makefile` (see `PI3_HOST`, `PI3_DIR`, `PI3_CC` pattern)
2. Add cross-compile target if needed (see `aarch64` target pattern)
3. Add `deploy4` target (copy from `deploy3`, update host vars)
4. Add `build4: pi deploy4` shortcut
5. Update `skills/deploy/SKILL.md` with new board row
6. Update `CLAUDE.md` `## Current state` cross-compile table

## Tests and verification

```bash
# Verify binary is correct arch
file ipscanner-pi         # should say: ARM, EABI5
file ipscanner-aarch64    # should say: ARM aarch64

# Verify deploy reached the board
ssh admin@100.66.44.107 "file ~/ToolScanIP/ipscanner"

# Verify web UI is up after gen-nginx.sh
curl http://100.66.44.107:8080/api/scan | python3 -m json.tool
```

**Pass:** `file` shows correct architecture; curl returns valid JSON with `hosts` array
**Fail:** `Exec format error` on board → wrong arch binary; `Connection refused` → daemon not started
