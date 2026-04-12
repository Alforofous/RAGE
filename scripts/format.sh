#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$PROJECT_ROOT/src"
CONFIG="$PROJECT_ROOT/uncrustify.cfg"

echo "=== Checking formatting ==="

if ! command -v uncrustify &>/dev/null; then
    echo "uncrustify not found. Install it or add to PATH."
    exit 1
fi

FILES=$(find "$SRC_DIR" -name "*.cpp" -o -name "*.hpp" 2>/dev/null)

if [[ -z "$FILES" ]]; then
    echo "No source files found in src/"
    exit 0
fi

FAILED=0
for file in $FILES; do
    # Check mode: compare uncrustified output to original
    FORMATTED=$(uncrustify -c "$CONFIG" -f "$file" 2>/dev/null)
    ORIGINAL=$(cat "$file")
    if [[ "$FORMATTED" != "$ORIGINAL" ]]; then
        echo "Formatting issue: $file"
        FAILED=1
    fi
done

if [[ $FAILED -eq 0 ]]; then
    echo "=== Formatting check passed ==="
else
    echo "=== Formatting issues found. Run: uncrustify -c uncrustify.cfg --replace --no-backup src/**/*.{cpp,hpp} ==="
    exit 1
fi
