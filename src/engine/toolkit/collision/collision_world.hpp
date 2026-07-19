#pragma once

#include <cstdint>
#include "engine/toolkit/collision/body_registry.hpp"
#include "engine/toolkit/collision/world_solid_query.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"

namespace RAGE::Toolkit {
    class KinematicBody;

    /**
     * @brief The one collision authority (design sheet Q2-A): a thin facade over the
     *        static-world side (`WorldSolidQuery` — lattice + registered volumes) and
     *        the dynamic side (`BodyRegistry` — boxes with mass). Volumes owned by a
     *        body are excluded from solidity, so body-vs-body resolves through boxes
     *        + inverse-mass shares while the lattice and body-less volumes (e.g.
     *        scripted spinners) act as infinite mass.
     *
     * Consumers that only need static solidity (projectiles, AI probes) can take a
     * `WorldSolidQuery` directly; bodies and gameplay take the facade.
     */
    class CollisionWorld {
    public:
        CollisionWorld(const VoxelData &lattice, const BrickPool &pool, float voxelSize)
            : query_(lattice, pool, voxelSize) {}

        // --- registration (api-north-star N9): volumes and bodies alike ------------
        /// Register a volume as solid world geometry (scripted movers, the world).
        void add(const Voxel3D &volume) { query_.registerVolume(volume); }
        /// Bind a standalone KinematicBody to this world; the body stays registered
        /// for its lifetime.
        void add(KinematicBody &body);
        void remove(const Voxel3D &volume) { query_.unregisterVolume(volume); }
        void clearVolumes() { query_.clearVolumes(); }
        size_t volumeCount() const { return query_.volumeCount(); }

        // --- solidity queries (body-owned volumes excluded) ------------------------
        bool solid(IVec3 worldVoxel, const Voxel3D *ignore = nullptr) const {
            return query_.solid(worldVoxel, ignore, bodies_.ownedVolumes());
        }
        SweepResult sweepAABB(const SweepBox &box, Vec3 delta,
                              const Voxel3D *ignore = nullptr) const {
            return query_.sweepAABB(box, delta, ignore, bodies_.ownedVolumes());
        }
        Vec3 depenetrate(const SweepBox &box, float maxPush,
                         const Voxel3D *ignore = nullptr) const {
            return query_.depenetrate(box, maxPush, ignore, bodies_.ownedVolumes());
        }

        // --- dynamic bodies ---------------------------------------------------------
        BodyId addBody(const SweepBox &box, float mass, const Voxel3D *ownedVolume = nullptr) {
            return bodies_.add(box, mass, ownedVolume);
        }
        void removeBody(BodyId id) { bodies_.remove(id); }
        void updateBodyBox(BodyId id, const SweepBox &box) { bodies_.updateBox(id, box); }
        size_t bodyCount() const { return bodies_.count(); }
        Vec3 separationFor(BodyId self) const { return bodies_.separationFor(self); }

        float voxelSize() const { return query_.voxelSize(); }
        const WorldSolidQuery &solidQuery() const { return query_; }

    private:
        WorldSolidQuery query_;
        BodyRegistry bodies_;
    };
}
