#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=========================================="
echo "  RAGE — Full check pipeline"
echo "=========================================="
echo ""

run_step() {
    local label="$1"
    shift
    local start
    start=$(date +%s)
    echo ">>> $label"
    "$@"
    local end
    end=$(date +%s)
    echo "<<< $label finished in $((end - start))s"
    echo ""
}

PIPELINE_START=$(date +%s)

run_step "[1/4] Build"      bash "$SCRIPT_DIR/build.sh"
run_step "[2/4] Tests"      bash "$SCRIPT_DIR/test.sh"
run_step "[3/4] Lint"       bash "$SCRIPT_DIR/lint.sh"
run_step "[4/4] Formatting" bash "$SCRIPT_DIR/format.sh"

TOTAL=$(($(date +%s) - PIPELINE_START))
echo "=========================================="
echo "  All checks passed in ${TOTAL}s"
echo "=========================================="
