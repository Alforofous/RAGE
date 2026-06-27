#!/bin/bash
# Configure and build RAGE.
#
# Usage:
#   ./scripts/build.sh                  # production build (default): no Tracy, no dev-only UI
#   ./scripts/build.sh --dev            # dev build: Tracy linked + debug UI dev features
#
# Always re-invokes `cmake -B build -DRAGE_DEV_BUILD=...` so flipping --dev between runs
# correctly updates the cache.

set -e

DEV_FLAG=OFF
PROFILE_LABEL=production
for arg in "$@"; do
    case "$arg" in
        --dev) DEV_FLAG=ON; PROFILE_LABEL=dev ;;
        *)
            echo "build.sh: unknown argument '$arg'" >&2
            echo "usage: $0 [--dev]" >&2
            exit 2
            ;;
    esac
done

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

echo "=== Building RAGE ($PROFILE_LABEL) ==="

git -C "$PROJECT_ROOT" submodule update --init

echo "Configuring CMake..."
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DRAGE_DEV_BUILD="$DEV_FLAG"

cmake --build "$BUILD_DIR" --target RAGE

echo "=== Build succeeded ==="
