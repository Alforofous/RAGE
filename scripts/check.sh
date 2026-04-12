#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=========================================="
echo "  RAGE — Full check pipeline"
echo "=========================================="
echo ""

echo "[1/4] Build"
bash "$SCRIPT_DIR/build.sh"
echo ""

echo "[2/4] Tests"
bash "$SCRIPT_DIR/test.sh"
echo ""

echo "[3/4] Lint"
bash "$SCRIPT_DIR/lint.sh"
echo ""

echo "[4/4] Formatting"
bash "$SCRIPT_DIR/format.sh"
echo ""

echo "=========================================="
echo "  All checks passed"
echo "=========================================="
