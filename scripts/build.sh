#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

echo "=== Building RAGE ==="

# Init submodules if needed
git -C "$PROJECT_ROOT" submodule update --init

# Configure if needed
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Configuring CMake..."
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR"
fi

# Build
cmake --build "$BUILD_DIR" --target RAGE

echo "=== Build succeeded ==="
