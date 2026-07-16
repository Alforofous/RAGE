#pragma once

#include <cstdint>
#include <vector>
#include "engine/rendering/world_brick_grid.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"
#include "math/ivec.hpp"
#include "math/mat.hpp"
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

    /// Registry handle for a dynamic body's collision box. 0 is never issued.
    struct BodyId {
        uint32_t id = 0;
        bool valid() const { return id != 0; }
    };

    /**
     * @brief The one collision authority (design sheet Q2-A, graduated from the T2 free
     *        functions): the grid-resident world lattice, a registry of free-standing
     *        (full-TRS) volumes, and a registry of dynamic body boxes with mass —
     *        queried through a single `solid` / `sweepAABB` / separation surface.
     *        Reads the same `WorldBrickGrid` + shared `BrickPool` the rays traverse.
     *
     * Mass model (positional, not dynamic): the lattice and body-less volumes (e.g.
     * scripted spinners) are infinite mass — they push bodies, never yield. Overlapping
     * body boxes separate along the minimum-penetration axis, each body taking its
     * inverse-mass share on its own tick (`m_other / (m_self + m_other)`), so heavier
     * bodies move less. Volumes owned by a body are excluded from `solid`/`sweepAABB`
     * — body-vs-body resolves through boxes + mass only, letting a walking body
     * bulldoze lighter ones instead of being wall-blocked by their voxels.
     *
     * Free-standing volumes are sampled at probed-cell centers transformed into their
     * object space: rotated shapes collide at voxel resolution (~one world voxel of
     * slop); a swept-OBB narrow phase can replace the sampling later.
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

        /**
         * @brief Register a dynamic body box. `mass` must be > 0. `ownedVolume` (may be
         *        null) is this body's visual Voxel3D; body-owned volumes stop blocking
         *        `solid`/`sweepAABB` so body-vs-body goes through boxes instead.
         */
        BodyId addBody(const SweepBox &box, float mass, const Voxel3D *ownedVolume = nullptr);
        void removeBody(BodyId id);
        void updateBodyBox(BodyId id, const SweepBox &box);
        size_t bodyCount() const { return bodies_.size(); }

        /**
         * @brief Smallest push (checked per axis, both directions, up to `maxPush`)
         *        that clears `box` of solid voxels. Zero when not overlapping or when
         *        nothing within `maxPush` clears it. Infinite-mass response: the caller
         *        takes the full push (a spinner rotating into a body shoves it out).
         */
        Vec3 depenetrate(const SweepBox &box, float maxPush,
                         const Voxel3D *ignore = nullptr) const;

        /**
         * @brief This body's inverse-mass share of separation from every other
         *        registered body box it overlaps, along each overlap's
         *        minimum-penetration axis. Call once per tick per body; both bodies
         *        converge as each takes its own share.
         */
        Vec3 separationFor(BodyId self) const;

        float voxelSize() const { return voxelSize_; }

    private:
        /// Per-query cache: volumes whose world AABB touches the queried region, with
        /// their inverse transforms computed once (not per probed cell).
        struct VolumeScratch {
            const Voxel3D *volume = nullptr;
            Mat4 invWorld;
        };

        struct Body {
            uint32_t id = 0;
            SweepBox box{};
            float mass = 1.0f;
            const Voxel3D *ownedVolume = nullptr;
        };

        bool latticeSolid_(IVec3 worldVoxel) const;
        bool volumeSolidAt_(const Voxel3D &volume, const Mat4 &invWorld, Vec3 worldPoint) const;
        bool volumeIsBodyOwned_(const Voxel3D *volume) const;
        void gatherVolumes_(const SweepBox &bounds, const Voxel3D *ignore) const;
        bool scratchSolid_(IVec3 worldVoxel) const;
        bool overlapsSolid_(const SweepBox &box, const Voxel3D *ignore) const;
        float sweepAxis_(SweepBox box, int32_t axis, float delta, bool &hit) const;

        const WorldBrickGrid &grid_;
        const BrickPool &pool_;
        float voxelSize_ = 0.0f;
        std::vector<const Voxel3D *> volumes_;
        std::vector<Body> bodies_;
        mutable std::vector<VolumeScratch> volumeScratch_;
        uint32_t nextBodyId_ = 1;
    };
}
