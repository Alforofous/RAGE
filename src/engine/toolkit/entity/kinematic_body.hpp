#pragma once

#include "engine/scene/node3d.hpp"
#include "engine/toolkit/collision/voxel_world_query.hpp"
#include "math/vec.hpp"

namespace RAGE::Toolkit {
    /// All tuning injected by the app (capacity-injection rule). Units: meters, seconds.
    struct KinematicBodyConfig {
        /// Collision box full extents; the driven node's position is the box's
        /// bottom-center ("feet").
        Vec3 size{ 0.6f, 1.8f, 0.6f };
        float gravity = 22.0f;
        float terminalSpeed = 50.0f;
        float jumpSpeed = 7.0f;
        /// Max ledge height auto-climbed while walking (0 disables step-up).
        float stepUpHeight = 0.12f;
    };

    /// Per-tick movement intent, produced by the app's input layer.
    struct MoveInput {
        /// Desired horizontal velocity in world units/s (.y ignored).
        Vec3 walk;
        bool jump = false;
    };

    /**
     * @brief Gravity/jump/step-up kinematics over `VoxelWorldQuery::sweepAABB`,
     *        driving any `Node3D`'s position — nothing here is "a player"; attach a
     *        camera child and it is one, attach a Voxel3D child and it's a mob.
     *        Deterministic and render-free: `update(world, input, dt)` is the entire
     *        surface. The node's position is the collision box's bottom-center.
     */
    class KinematicBody {
    public:
        KinematicBody(Node3D &node, KinematicBodyConfig config);

        void update(const VoxelWorldQuery &world, const MoveInput &input, float dt);

        bool grounded() const { return grounded_; }
        Vec3 velocity() const { return velocity_; }
        const KinematicBodyConfig &config() const { return config_; }

    private:
        SweepBox boxAt_(Vec3 feet) const;

        Node3D &node_;
        KinematicBodyConfig config_;
        Vec3 velocity_{ 0.0f, 0.0f, 0.0f };
        bool grounded_ = false;
    };
}
