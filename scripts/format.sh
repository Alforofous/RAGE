#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DIR="$PROJECT_ROOT/src"
TEST_DIR="$PROJECT_ROOT/tests"

echo "=== Checking formatting ==="

if ! command -v clang-format &>/dev/null; then
    echo "clang-format not found. Install it or add to PATH."
    exit 1
fi

FILES=$(find "$SRC_DIR" "$TEST_DIR" -name "*.cpp" -o -name "*.hpp" 2>/dev/null)

if [[ -z "$FILES" ]]; then
    echo "No source files found"
    exit 0
fi

FAILED=0
for file in $FILES; do
    if ! clang-format --dry-run --Werror "$file" 2>/dev/null; then
        echo "Formatting issue: $file"
        FAILED=1
    fi
done

if [[ $FAILED -eq 0 ]]; then
    echo "=== Formatting check passed ==="
else
    echo "=== Formatting issues found ==="
    exit 1
fi
