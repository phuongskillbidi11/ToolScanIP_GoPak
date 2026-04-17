#!/bin/bash
# snapshot.sh — create or manage conversation snapshots
#
# Usage:
#   ./scripts/snapshot.sh create <name> [description]
#   ./scripts/snapshot.sh list
#   ./scripts/snapshot.sh show  <name>
#   ./scripts/snapshot.sh fork  <name> <new-name> [description]

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SNAP_DIR="$ROOT/skills/snapshots"
TEMPLATE="$SNAP_DIR/_TEMPLATE_.md"
MANIFEST="$ROOT/skills/manifest.json"

# ── helpers ────────────────────────────────────────────────────────────────

die()  { echo "Error: $*" >&2; exit 1; }
info() { echo "  $*"; }

timestamp() { date "+%Y-%m-%d %H:%M %Z"; }

# Add or update a snapshot entry in manifest.json
manifest_upsert() {
    local name="$1"
    local desc="$2"
    local path="skills/snapshots/$name/SKILL.md"

    # Use python3 to safely edit the JSON
    python3 - "$MANIFEST" "$name" "$desc" "$path" <<'PYEOF'
import sys, json

manifest_file, name, desc, path = sys.argv[1:]
with open(manifest_file) as f:
    m = json.load(f)

snapshots = m.setdefault("snapshots", [])
entry = next((s for s in snapshots if s["name"] == name), None)
if entry:
    entry["description"] = desc
    entry["path"]        = path
else:
    snapshots.append({"name": name, "type": "snapshot",
                      "description": desc, "path": path})

with open(manifest_file, "w") as f:
    json.dump(m, f, indent=2)
    f.write("\n")

print(f"  manifest updated: {name}")
PYEOF
}

# ── subcommands ────────────────────────────────────────────────────────────

cmd_create() {
    local name="${1:-}"
    local desc="${2:-Snapshot created on $(timestamp)}"

    [ -z "$name" ] && die "Usage: $0 create <name> [description]"

    local dest="$SNAP_DIR/$name"
    [ -d "$dest" ] && die "Snapshot '$name' already exists. Use 'fork' to branch it."
    mkdir -p "$dest"

    local ts
    ts="$(timestamp)"
    sed \
        -e "s/{{NAME}}/$name/g" \
        -e "s/{{TIMESTAMP}}/$ts/g" \
        -e "s/{{STATUS}}/active/g" \
        -e "s|{{REQUIREMENTS}}|<!-- Fill in: paste the user's requirements -->|g" \
        -e "s|{{DECISIONS}}|<!-- Fill in: technical decisions -->|g" \
        -e "s|{{FILES}}|<!-- Fill in: files modified -->|g" \
        -e "s|{{CODE}}|<!-- Fill in: key code snippets -->|g" \
        -e "s|{{QUESTIONS}}|<!-- Fill in: open questions -->|g" \
        -e "s|{{NEXT_STEPS}}|<!-- Fill in: next steps -->|g" \
        -e "s|{{CONSTRAINTS}}|<!-- Fill in: hard constraints -->|g" \
        "$TEMPLATE" > "$dest/SKILL.md"

    manifest_upsert "$name" "$desc"

    info "Snapshot created: $dest/SKILL.md"
    info "Edit the file to fill in context, or ask Claude to populate it."
    info ""
    info "To load:  ./scripts/load_skill.sh snapshot $name"
}

cmd_list() {
    echo ""
    echo "Snapshots:"
    local found=0
    for d in "$SNAP_DIR"/*/; do
        local name
        name="$(basename "$d")"
        [ "$name" = "_TEMPLATE_" ] && continue
        found=1
        # Read status line from the SKILL.md
        local status
        status=$(grep -m1 '\*\*Status:\*\*' "$d/SKILL.md" 2>/dev/null \
                 | sed 's/.*Status:\*\* *//;s/ .*//;s/-->.*//' | tr -d ' ')
        local ts
        ts=$(grep -m1 '\*\*Created:\*\*' "$d/SKILL.md" 2>/dev/null \
             | sed 's/.*Created:\*\* *//')
        printf "  %-32s  %-12s  %s\n" "$name" "${status:-?}" "$ts"
    done
    [ "$found" -eq 0 ] && echo "  (none yet)"
    echo ""
    echo "Usage:"
    echo "  $0 create <name> [description]"
    echo "  $0 show   <name>"
    echo "  $0 fork   <name> <new-name> [description]"
}

cmd_show() {
    local name="${1:-}"
    [ -z "$name" ] && die "Usage: $0 show <name>"
    local f="$SNAP_DIR/$name/SKILL.md"
    [ -f "$f" ] || die "Snapshot '$name' not found."
    cat "$f"
}

cmd_fork() {
    local src="${1:-}"
    local dst="${2:-}"
    local desc="${3:-Forked from $src on $(timestamp)}"

    [ -z "$src" ] || [ -z "$dst" ] && die "Usage: $0 fork <source> <new-name> [description]"

    local src_file="$SNAP_DIR/$src/SKILL.md"
    [ -f "$src_file" ] || die "Source snapshot '$src' not found."

    local dst_dir="$SNAP_DIR/$dst"
    [ -d "$dst_dir" ] && die "Snapshot '$dst' already exists."
    mkdir -p "$dst_dir"

    local ts
    ts="$(timestamp)"

    # Copy source, update header fields
    sed \
        -e "s/# Snapshot: .*/# Snapshot: $dst/" \
        -e "s/\*\*Name:\*\*.*//" \
        -e "s/\*\*Created:\*\* .*/\*\*Created:\*\* $ts/" \
        -e "s/\*\*Status:\*\* .*/\*\*Status:\*\* active  <!-- forked from $src -->/" \
        -e "s/\"$src\"/\"$dst\"/g" \
        "$src_file" > "$dst_dir/SKILL.md"

    # Mark source as forked
    sed -i "s/\*\*Status:\*\* active/\*\*Status:\*\* forked  <!-- into $dst -->/" \
        "$src_file" 2>/dev/null || true

    manifest_upsert "$dst" "$desc"

    info "Forked '$src' → '$dst'"
    info "File: $dst_dir/SKILL.md"
    info "Edit to apply the fork changes, or ask Claude to apply them."
}

# ── dispatch ───────────────────────────────────────────────────────────────

case "${1:-list}" in
    create) shift; cmd_create "$@" ;;
    list)           cmd_list        ;;
    show)   shift; cmd_show   "$@" ;;
    fork)   shift; cmd_fork   "$@" ;;
    *)      die "Unknown command '$1'. Use: create | list | show | fork" ;;
esac
