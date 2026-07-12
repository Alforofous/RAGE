#pragma once

#include <cstdint>
#include "engine/rendering/world_brick_grid.hpp"
#include "engine/scene/brick_pool.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"

namespace RAGE::Toolkit {
    /// Axis-aligned box in world units. `min`/`max` are corner positions.
    struct SweepBox {
        Vec3 min;
        Vec3 max;
    };

    /// Outcome of a swept move: the delta actually applied after clipping against
    /// solid voxels, plus per-axis contact flags (true = clipped on that axis).
    struct SweepResult {
        Vec3 moved;
        bool hitX = false;
        bool hitY = false;
        bool hitZ = false;

        bool hitAxis(int32_t axis) const {
            if (axis == 0) {
                return hitX;
            }
            return axis == 1 ? hitY : hitZ;
        }
        bool anyHit() const { return hitX || hitY || hitZ; }
    };

    /**
     * @brief CPU-side solidity + swept-AABB queries against the voxel world — the same
     *        `WorldBrickGrid` + shared `BrickPool` the rays traverse, read on the CPU.
     *        No GPU work, fully deterministic.
     *
     * Free-function-stage collision (design sheet Q2-B): one world, queried directly.
     * Graduates to a `CollisionWorld` registry the moment a second dynamic body exists —
     * see `.claude/plans/toolkit-roadmap.md`.
     *
     * Coordinates: world voxel coords are `floor(worldPos / voxelSize)`. Cells outside
     * the grid's current window read as air (matching the renderer). Main-thread use;
     * the pool's internal mutex makes concurrent worker allocation safe to read past.
     */
    class VoxelWorldQuery {
    public:
        VoxelWorldQuery(const WorldBrickGrid &grid, const BrickPool &pool, float voxelSize);

        /// True when the voxel at `worldVoxel` has a non-zero alpha byte.
        bool solid(IVec3 worldVoxel) const;

        /**
         * @brief Move `box` by `delta`, clipping against solid voxels one axis at a
         *        time (Y, then X, then Z — vertical resolution first so horizontal
         *        motion slides along the ground). Any delta magnitude is supported;
         *        every voxel plane crossed is tested.
         */
        SweepResult sweepAABB(const SweepBox &box, Vec3 delta) const;

        float voxelSize() const { return voxelSize_; }

    private:
        float sweepAxis_(SweepBox box, int32_t axis, float delta, bool &hit) const;

        const WorldBrickGrid &grid_;
        const BrickPool &pool_;
        float voxelSize_ = 0.0f;
    };
}
