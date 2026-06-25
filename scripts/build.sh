#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

echo "=== Building RAGE ==="

# Init submodules if needed
git -C "$PROJECT_ROOT" submodule update --init

# Configure if the cache is missing or stale. CMake is idempotent when the cache is valid;
# it short-circuits cleanly. We check for CMakeCache.txt rather than the directory itself
# because a previous failed configure can leave behind an empty or partial build/ dir.
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "Configuring CMake..."
    cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR"
fi

# Build
cmake --build "$BUILD_DIR" --target RAGE

echo "=== Build succeeded ==="
