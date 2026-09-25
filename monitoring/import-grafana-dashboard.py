#!/usr/bin/env python3
"""Refresh the dashboard's fleet inventory from Loki and import it into Grafana.

Usage: import-grafana-dashboard.py [--dry-run] [--write-json]
Env: LOKI (default http://100.82.22.47:3100), GRAFANA (default http://100.82.22.47:3000),
     GRAFANA_AUTH (default admin:admin)
"""
import base64
import ipaddress
import json
import os
import sys
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
JSON_PATH = os.path.join(HERE, "grafana-dashboard-luckfox-fleet.json")
LOKI = os.environ.get("LOKI", "http://100.82.22.47:3100")
GRAFANA = os.environ.get("GRAFANA", "http://100.82.22.47:3000")
AUTH = os.environ.get("GRAFANA_AUTH", "admin:admin")
# Loki retention on the monitoring board is 240h; a node silent longer than
# that drops out of the inventory on the next import.
WINDOW_S = 10 * 24 * 3600


def fleet_inventory():
    now_ns = int(time.time()) * 10**9
    params = urllib.parse.urlencode({
        "query": '{job="node-oee"}',
        "start": str(now_ns - WINDOW_S * 10**9),
        "end": str(now_ns),
    })
    with urllib.request.urlopen("%s/loki/api/v1/label/node_ip/values?%s" % (LOKI, params), timeout=30) as r:
        body = json.load(r)
    if body.get("status") != "success":
        raise SystemExit("Error: Loki label query failed: %s" % body)
    ips = set()
    for value in body.get("data") or []:
        try:
            parsed = ipaddress.ip_address(value)
        except ValueError:
            continue
        if parsed.version == 4:
            ips.add(str(parsed))
    return sorted(ips, key=lambda v: tuple(map(int, v.split("."))))


def set_fleet(dashboard, ips):
    for var in dashboard["templating"]["list"]:
        if var["name"] == "fleet":
            var["query"] = ",".join(ips)
            var["options"] = [{"text": ip, "value": ip, "selected": True} for ip in ips]
            var["current"] = {"text": list(ips), "value": list(ips)}
            return
    raise SystemExit("Error: dashboard has no 'fleet' variable")


def main(argv):
    dry_run = "--dry-run" in argv
    write_json = "--write-json" in argv
    with open(JSON_PATH, encoding="utf-8") as f:
        dashboard = json.load(f)

    ips = fleet_inventory()
    if not ips:
        raise SystemExit("Error: Loki returned no node_ip values for {job=\"node-oee\"}")
    print("fleet: %d nodes: %s" % (len(ips), " ".join(ips)))
    set_fleet(dashboard, ips)
    if dry_run:
        return 0

    if write_json:
        with open(JSON_PATH, "w", encoding="utf-8") as f:
            json.dump(dashboard, f, indent=2, ensure_ascii=False)
            f.write("\n")

    payload = json.dumps({"dashboard": dashboard, "overwrite": True}).encode()
    req = urllib.request.Request("%s/api/dashboards/db" % GRAFANA, data=payload, method="POST", headers={
        "Content-Type": "application/json",
        "Authorization": "Basic " + base64.b64encode(AUTH.encode()).decode(),
    })
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            result = json.load(r)
    except urllib.error.HTTPError as e:
        raise SystemExit("Error: Grafana import failed (HTTP %s): %s" % (e.code, e.read().decode()[:500]))
    print("import: status=%s uid=%s url=%s%s" % (result.get("status"), result.get("uid"), GRAFANA, result.get("url")))
    return 0 if result.get("status") == "success" else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
