#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
SRC_DIR="$PROJECT_ROOT/src"
TEST_DIR="$PROJECT_ROOT/tests"

echo "=== Running clang-tidy ==="

# Check both .cpp and .hpp files in src/ and tests/
FILES=$(find "$SRC_DIR" "$TEST_DIR" -name "*.cpp" -o -name "*.hpp" 2>/dev/null)

if [[ -z "$FILES" ]]; then
    echo "No source files found"
    exit 0
fi

FAILED=0
for file in $FILES; do
    REL=$(realpath --relative-to="$PROJECT_ROOT" "$file")
    echo "Checking $REL..."
    OUTPUT=$(clang-tidy -p "$BUILD_DIR" --header-filter="(src/|tests/)" "$file" 2>&1 | grep -v "^$" | grep -v "warnings generated" | grep -v "Suppressed" | grep -v "Use -header-filter" || true)
    if [[ -n "$OUTPUT" ]]; then
        echo "$OUTPUT"
        FAILED=1
    fi
done

if [[ $FAILED -eq 0 ]]; then
    echo "=== Lint passed ==="
else
    echo "=== Lint issues found ==="
    exit 1
fi
