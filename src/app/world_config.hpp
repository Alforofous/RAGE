#pragma once

#include <cstdint>
#include <filesystem>
#include "engine/rendering/renderer.hpp"
#include "engine/toolkit/content/chunk_store.hpp"
#include "math/ivec.hpp"

namespace RAGE::App {
    inline int32_t nextPow2(int32_t v) {
        int32_t p = 1;
        while (p < v) {
            p *= 2;
        }
        return p;
    }

    /**
     * @brief Top-level world sizing — the single place capacities originate. Everything
     *        below (brick pool size, grid SSBO capacity, world volume dims) is DERIVED
     *        here and injected into engine components via constructors; engine code owns
     *        no capacity constants and validates what it is given at the seams.
     */
    struct WorldPipelineConfig {
        int32_t streamRadius = 30;
        Toolkit::Content::ChunkStore::YRange yRange{ .min = -1, .max = 2 };
        IVec3 chunkBrickDims{ 4, 4, 4 };

        /// Bricks per axis the loaded window can span (XZ diameter × chunk, Y from yRange).
        IVec3 windowBrickExtent() const {
            const int32_t diameter = (2 * streamRadius) + 1;
            const int32_t yChunks = yRange.max - yRange.min + 1;
            return IVec3{ diameter * chunkBrickDims.x, yChunks * chunkBrickDims.y,
                          diameter * chunkBrickDims.z };
        }

        /// The world volume's voxel dims: the window extent in voxels.
        IVec3 worldVoxelDims() const {
            const IVec3 w = windowBrickExtent();
            return IVec3{ w.x * 8, w.y * 8, w.z * 8 };
        }

        /// Grid capacity: window extent rounded to the next power of two per axis —
        /// must match the world volume's internal storage rounding.
        IVec3 gridDims() const {
            const IVec3 w = windowBrickExtent();
            return IVec3{ nextPow2(w.x), nextPow2(w.y), nextPow2(w.z) };
        }

        /// Statistical, unlike the geometric bounds above: sized from measured occupancy
        /// of the streamed terrain (~110K unique bricks at radius 30) plus headroom.
        /// BrickPool throws loudly on exhaustion.
        size_t maxBricks() const {
            const IVec3 w = windowBrickExtent();
            const size_t windowCells = static_cast<size_t>(w.x) * static_cast<size_t>(w.y)
                                       * static_cast<size_t>(w.z);
            return (windowCells * 3) / 20;
        }

        Renderer::WorldLimits rendererLimits(bool brickDedup) const {
            return Renderer::WorldLimits{
                .brickPool = { .maxBricks = maxBricks(), .enableDedup = brickDedup },
                .worldGridDims = gridDims(),
            };
        }
    };

    inline std::filesystem::path executableDir(const char *argv0) {
        std::error_code ec;
        const std::filesystem::path exePath = std::filesystem::weakly_canonical(argv0, ec);
        if (ec || exePath.empty()) {
            return std::filesystem::current_path();
        }
        return exePath.parent_path();
    }
}
