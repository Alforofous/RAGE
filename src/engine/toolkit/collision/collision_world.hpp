#pragma once

#include <cstdint>
#include <vector>
#include "engine/rendering/world_brick_grid.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"
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
     * @brief The one collision authority (design sheet Q2-A, graduated from the T2 free
     *        functions): the grid-resident world lattice plus a registry of
     *        free-standing (full-TRS) volumes, queried through a single `solid` /
     *        `sweepAABB` surface. Reads the same `WorldBrickGrid` + shared `BrickPool`
     *        the rays traverse — what you see is what you collide with.
     *
     * Free-standing volumes are sampled at probed-cell centers transformed into their
     * object space: rotated shapes collide at voxel resolution (~one world voxel of
     * slop), which is exact enough for characters and props; a swept-OBB narrow phase
     * can replace the sampling later without changing this surface.
     *
     * Registered volumes must outlive the world or be unregistered first. Main-thread
     * use; per-frame transform reads go through `Node3D::worldMatrix()`.
     */
    class CollisionWorld {
    public:
        CollisionWorld(const WorldBrickGrid &grid, const BrickPool &pool, float voxelSize);

        /// Register a free-standing Voxel3D as collidable. No-op if already registered.
        void registerVolume(const Voxel3D &volume);
        void unregisterVolume(const Voxel3D &volume);
        /// Drop every registered volume — scene teardown helper.
        void clearVolumes() { volumes_.clear(); }
        size_t volumeCount() const { return volumes_.size(); }

        /**
         * @brief True when the world-lattice voxel is solid OR any registered volume
         *        (except `ignore`) covers the voxel's center. Lattice cells outside
         *        the streaming window read as air.
         */
        bool solid(IVec3 worldVoxel, const Voxel3D *ignore = nullptr) const;

        /**
         * @brief Move `box` by `delta`, clipping one axis at a time (Y, then X, then Z —
         *        vertical resolution first so horizontal motion slides along ground).
         *        Every crossed voxel plane is tested: no tunneling at any delta.
         *        `ignore` excludes a body's own volume from blocking itself.
         */
        SweepResult sweepAABB(const SweepBox &box, Vec3 delta,
                              const Voxel3D *ignore = nullptr) const;

        float voxelSize() const { return voxelSize_; }

    private:
        bool latticeSolid_(IVec3 worldVoxel) const;
        bool volumeSolidAt_(const Voxel3D &volume, const Mat4 &invWorld, Vec3 worldPoint) const;
        float sweepAxis_(SweepBox box, int32_t axis, float delta, const Voxel3D *ignore,
                         bool &hit) const;

        const WorldBrickGrid &grid_;
        const BrickPool &pool_;
        float voxelSize_ = 0.0f;
        std::vector<const Voxel3D *> volumes_;
    };
}
