#!/bin/bash
# load_skill.sh — print a skill's SKILL.md to stdout
# Usage: ./scripts/load_skill.sh <skill-name>
#        ./scripts/load_skill.sh list

SKILLS_DIR="$(dirname "$0")/../skills"

if [ "$1" = "list" ] || [ -z "$1" ]; then
    echo "Available skills:"
    for d in "$SKILLS_DIR"/*/; do
        name="$(basename "$d")"
        desc=$(python3 -c "
import json
m = json.load(open('$SKILLS_DIR/manifest.json'))
s = next((x for x in m['skills'] if x['name']=='$name'), None)
print(s['description'] if s else '')
" 2>/dev/null)
        printf "  %-16s %s\n" "$name" "$desc"
    done
    echo ""
    echo "Usage: $0 <skill-name>"
    exit 0
fi

SKILL_FILE="$SKILLS_DIR/$1/SKILL.md"

if [ ! -f "$SKILL_FILE" ]; then
    echo "Error: skill '$1' not found."
    echo "Run '$0 list' to see available skills."
    exit 1
fi

cat "$SKILL_FILE"
