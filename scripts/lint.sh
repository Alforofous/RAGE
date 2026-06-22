#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
SRC_DIR="$PROJECT_ROOT/src"
TEST_DIR="$PROJECT_ROOT/tests"

echo "=== Running clang-tidy ==="

mapfile -t FILES < <(find "$SRC_DIR" "$TEST_DIR" \( -name "*.cpp" -o -name "*.hpp" \) 2>/dev/null | sort)
TOTAL=${#FILES[@]}

if [[ $TOTAL -eq 0 ]]; then
    echo "No source files found"
    exit 0
fi

CORES=$(nproc 2>/dev/null || echo 4)
JOBS=$((CORES / 2))
[[ $JOBS -lt 1 ]] && JOBS=1
[[ $JOBS -gt 8 ]] && JOBS=8

echo "Linting $TOTAL files with $JOBS parallel jobs..."
echo ""

START=$(date +%s)
ISSUES_FILE=$(mktemp)
DONE_DIR=$(mktemp -d)

trap 'rm -rf "$ISSUES_FILE" "$DONE_DIR"' EXIT

check_one() {
    local file="$1"
    local rel
    rel=$(realpath --relative-to="$PROJECT_ROOT" "$file")

    local file_start
    file_start=$(date +%s)

    local output
    output=$(clang-tidy -p "$BUILD_DIR" --header-filter="(src/|tests/)" "$file" 2>&1 \
        | grep -v "^$" \
        | grep -v "warnings generated" \
        | grep -v "Suppressed" \
        | grep -v "Use -header-filter" \
        || true)

    local duration=$(($(date +%s) - file_start))

    touch "$DONE_DIR/$(basename "$file").$$"
    local done_count
    done_count=$(find "$DONE_DIR" -type f | wc -l)

    printf "  [%2d/%2d] %4ds  %s\n" "$done_count" "$TOTAL" "$duration" "$rel"

    if [[ -n "$output" ]]; then
        {
            echo "--- $rel ---"
            echo "$output"
            echo ""
        } >> "$ISSUES_FILE"
    fi
}

export -f check_one
export BUILD_DIR PROJECT_ROOT TOTAL DONE_DIR ISSUES_FILE

printf '%s\n' "${FILES[@]}" | xargs -P "$JOBS" -I{} bash -c 'check_one "$@"' _ {}

TOTAL_TIME=$(($(date +%s) - START))
echo ""

if [[ -s "$ISSUES_FILE" ]]; then
    echo "--- Issues ---"
    cat "$ISSUES_FILE"
    echo "=== Lint issues found (took ${TOTAL_TIME}s) ==="
    exit 1
fi

echo "=== Lint passed (took ${TOTAL_TIME}s) ==="
