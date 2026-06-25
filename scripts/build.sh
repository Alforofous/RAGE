#!/bin/bash
# Configure and build RAGE.
#
# Usage:
#   ./scripts/build.sh             # lean build, no profiling
#   ./scripts/build.sh --profile   # build with Tracy profiling enabled
#
# Always re-invokes `cmake -B build -DRAGE_ENABLE_PROFILING=...` so toggling --profile
# between runs correctly updates the cache.

set -e

PROFILE_FLAG=OFF
for arg in "$@"; do
    case "$arg" in
        --profile) PROFILE_FLAG=ON ;;
        *)
            echo "build.sh: unknown argument '$arg'" >&2
            echo "usage: $0 [--profile]" >&2
            exit 2
            ;;
    esac
done

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

echo "=== Building RAGE (profiling=$PROFILE_FLAG) ==="

git -C "$PROJECT_ROOT" submodule update --init

echo "Configuring CMake..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DRAGE_ENABLE_PROFILING="$PROFILE_FLAG"

cmake --build "$BUILD_DIR" --target RAGE

echo "=== Build succeeded ==="
