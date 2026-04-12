#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

echo "=== Building and running tests ==="

# Build tests
cmake --build "$BUILD_DIR" --target rage_tests

# Run tests
"$BUILD_DIR/tests/rage_tests.exe"

echo "=== All tests passed ==="
