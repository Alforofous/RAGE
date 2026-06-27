#pragma once

#include <cstddef>
#include <span>
#include <vector>
#include "engine/scene/brick.hpp"
#include "math/ivec.hpp"

namespace RAGE {
    class VoxelData;

    /**
     * Renderer-owned sparse top-level brick grid. Each cell stores either
     * `kEmptyBrick` (empty) or a `BrickHandle` pointing into the shared `BrickPool`.
     * The shader does its outer DDA over this grid: when a cell is non-empty, it
     * follows the handle into the pool and does the inner 8³ DDA.
     *
     * `rebuild(...)` is called every frame from the scene placements — cheap because
     * it's just handle copies, no voxel data movement.
     *
     * Storage v1: dense `std::vector<BrickHandle>`, sized to enclose all placements'
     * world-brick AABBs with one cell of margin. Fine sub-100 m worlds (a 128³ grid
     * = ~2 MB). Kilometre-scale will need a sparse/hashed variant — tracked in the
     * brickmap component plan, not this milestone.
     */
    struct VoxelDataWorldPlacement {
        const VoxelData *data = nullptr;
        IVec3 worldBrickOrigin{ 0, 0, 0 };   // brick-coord where `data`'s (0,0,0) lands
    };

    class WorldBrickGrid {
    public:
        WorldBrickGrid();

        /**
         * Rebuild the grid from the current frame's `VoxelData` placements. Computes
         * world brick origin + dims from the union of placement AABBs (with one
         * cell of margin) and writes each placement's brick handles into the right
         * cells. Existing grid contents are discarded.
         *
         * Empty input clears the grid (`dims = (0,0,0)`).
         */
        void rebuild(std::span<const VoxelDataWorldPlacement> placements);

        IVec3 dims() const { return dims_; }
        IVec3 worldBrickOrigin() const { return worldBrickOrigin_; }

        BrickHandle handleAt(IVec3 worldCell) const;
        std::span<const BrickHandle> handles() const { return handles_; }

    private:
        size_t flatIndex(IVec3 worldCell) const;

        IVec3 worldBrickOrigin_{ 0, 0, 0 };
        IVec3 dims_{ 0, 0, 0 };
        std::vector<BrickHandle> handles_;
    };
}
