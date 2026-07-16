#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include "engine/toolkit/collision/world_solid_query.hpp"
#include "math/vec.hpp"

namespace RAGE {
    class Voxel3D;
}

namespace RAGE::Toolkit {
    /// Registry handle for a dynamic body's collision box. 0 is never issued.
    struct BodyId {
        uint32_t id = 0;
        bool valid() const { return id != 0; }
    };

    /**
     * @brief Dynamic-body side of collision: boxes with mass. Overlapping bodies
     *        separate along the minimum-penetration axis, each taking its
     *        inverse-mass share on its own tick (`m_other / (m_self + m_other)`) —
     *        heavier moves less; pairs converge over a few frames with no global
     *        solver. Knows nothing about voxels; `ownedVolumes()` exposes the visual
     *        volumes bodies own so the solidity side can exclude them.
     */
    class BodyRegistry {
    public:
        /// `mass` must be > 0. `ownedVolume` (may be null) is the body's visual Voxel3D.
        BodyId add(const SweepBox &box, float mass, const Voxel3D *ownedVolume = nullptr);
        void remove(BodyId id);
        void updateBox(BodyId id, const SweepBox &box);
        size_t count() const { return bodies_.size(); }

        /// This body's inverse-mass share of separation from every other body box it
        /// overlaps. Call once per tick per body.
        Vec3 separationFor(BodyId self) const;

        /// Visual volumes owned by any registered body (for solidity exclusion).
        std::span<const Voxel3D *const> ownedVolumes() const { return ownedVolumes_; }

    private:
        struct Body {
            uint32_t id = 0;
            SweepBox box{};
            float mass = 1.0f;
            const Voxel3D *ownedVolume = nullptr;
        };

        void rebuildOwnedVolumes_();

        std::vector<Body> bodies_;
        std::vector<const Voxel3D *> ownedVolumes_;
        uint32_t nextId_ = 1;
    };
}
