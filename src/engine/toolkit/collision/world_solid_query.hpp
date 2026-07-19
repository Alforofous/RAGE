#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel_data.hpp"
#include "math/ivec.hpp"
#include "math/mat.hpp"
#include "math/vec.hpp"

namespace RAGE {
    class Voxel3D;
}

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
     * @brief Static-world solidity: the grid-resident lattice plus registered
     *        free-standing volumes, queried via `solid` / `sweepAABB` / `depenetrate`.
     *        Knows nothing about dynamic bodies or mass — `excluded` lets a caller
     *        (the `CollisionWorld` facade) mask out body-owned volumes.
     *
     * Reads the same world lattice (the windowed volume's `VoxelData`) + shared
     * `BrickPool` the rays traverse.
     * Free-standing volumes are sampled at probed-cell centers transformed into their
     * object space (voxel-resolution approximation). Every query gathers the volumes
     * whose world AABB touches the queried region ONCE (with cached inverse
     * transforms) and threads that scope through its cell tests explicitly.
     *
     * Registered volumes must outlive the query or be unregistered first. Main-thread
     * use; per-frame transform reads go through `Node3D::worldMatrix()`.
     */
    class WorldSolidQuery {
    public:
        WorldSolidQuery(const VoxelData &lattice, const BrickPool &pool, float voxelSize);

        /// Register a free-standing Voxel3D as collidable. No-op if already registered.
        void registerVolume(const Voxel3D &volume);
        void unregisterVolume(const Voxel3D &volume);
        /// Drop every registered volume — scene teardown helper.
        void clearVolumes() { volumes_.clear(); }
        size_t volumeCount() const { return volumes_.size(); }

        /// True when the lattice voxel is solid OR a registered (non-ignored,
        /// non-excluded) volume covers the voxel's center. Outside the streaming
        /// window = air.
        bool solid(IVec3 worldVoxel, const Voxel3D *ignore = nullptr,
                   std::span<const Voxel3D *const> excluded = {}) const;

        /// Move `box` by `delta`, clipping one axis at a time (Y, then X, then Z).
        /// Every crossed voxel plane is tested: no tunneling at any delta.
        SweepResult sweepAABB(const SweepBox &box, Vec3 delta, const Voxel3D *ignore = nullptr,
                              std::span<const Voxel3D *const> excluded = {}) const;

        /// Smallest push (per axis, both directions, up to `maxPush`) clearing `box`
        /// of solid voxels; zero when not overlapping or nothing within reach clears.
        Vec3 depenetrate(const SweepBox &box, float maxPush, const Voxel3D *ignore = nullptr,
                         std::span<const Voxel3D *const> excluded = {}) const;

        float voxelSize() const { return voxelSize_; }

    private:
        /// Volumes relevant to one query, inverse transforms computed once.
        struct VolumeScratch {
            const Voxel3D *volume = nullptr;
            Mat4 invWorld;
        };

        std::span<const VolumeScratch> gatherVolumes_(
            const SweepBox &bounds, const Voxel3D *ignore,
            std::span<const Voxel3D *const> excluded) const;
        bool latticeSolid_(IVec3 worldVoxel) const;
        static bool volumeSolidAt_(const Voxel3D &volume, const Mat4 &invWorld, Vec3 worldPoint);
        bool cellSolid_(IVec3 worldVoxel, std::span<const VolumeScratch> scope) const;
        bool overlapsSolid_(const SweepBox &box, std::span<const VolumeScratch> scope) const;
        float sweepAxis_(SweepBox box, int32_t axis, float delta,
                         std::span<const VolumeScratch> scope, bool &hit) const;

        const VoxelData &lattice_;
        const BrickPool &pool_;
        float voxelSize_ = 0.0f;
        std::vector<const Voxel3D *> volumes_;
        /// Reused gather buffer (allocation-free steady state); handed out as a span.
        mutable std::vector<VolumeScratch> scratch_;
    };
}
