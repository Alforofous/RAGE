#pragma once

#include "engine/scene/node3d.hpp"
#include "engine/toolkit/collision/collision_world.hpp"
#include "math/vec.hpp"

namespace RAGE::Toolkit {
    /// All tuning injected by the app (capacity-injection rule). Units: meters, seconds, kg.
    struct KinematicBodyConfig {
        /// Collision box full extents; the driven node's position is the box's
        /// bottom-center ("feet").
        Vec3 size{ 0.6f, 1.8f, 0.6f };
        float gravity = 22.0f;
        float terminalSpeed = 50.0f;
        float jumpSpeed = 7.0f;
        /// Max ledge height auto-climbed while walking (0 disables step-up).
        float stepUpHeight = 0.12f;
        /// Inverse-mass share of body-vs-body separation; heavier moves less.
        float mass = 80.0f;
        /// Max per-tick push-out when solid geometry moves into the body (e.g. a
        /// scripted volume rotating through it). 0 disables depenetration.
        float maxDepenetration = 0.4f;
    };

    /// Per-tick movement intent, produced by the app's input layer.
    struct MoveInput {
        /// Desired horizontal velocity in world units/s (.y ignored).
        Vec3 walk;
        bool jump = false;
    };

    /**
     * @brief Gravity/jump/step-up kinematics plus positional collision response over
     *        `CollisionWorld`, driving any `Node3D`'s position — nothing here is
     *        "a player"; attach a camera child and it is one, attach a Voxel3D child
     *        and it's a mob. Deterministic and render-free.
     *
     * Registers itself with the world for its lifetime (non-copyable, non-movable).
     * Tick order: depenetrate from solid geometry (infinite mass — full push), take
     * this body's mass share of separation from overlapping bodies, then integrate
     * gravity and sweep. Upward pushes count as ground contact so bodies can stand
     * on each other. `selfVolume` is this body's visual Voxel3D (if any); body-owned
     * volumes don't block sweeps — body-vs-body resolves through boxes + mass.
     */
    class KinematicBody {
    public:
        KinematicBody(Node3D &node, CollisionWorld &world, KinematicBodyConfig config,
                      const Voxel3D *selfVolume = nullptr);
        ~KinematicBody();

        KinematicBody(const KinematicBody &) = delete;
        KinematicBody &operator=(const KinematicBody &) = delete;
        KinematicBody(KinematicBody &&) = delete;
        KinematicBody &operator=(KinematicBody &&) = delete;

        void update(const MoveInput &input, float dt);

        bool grounded() const { return grounded_; }
        Vec3 velocity() const { return velocity_; }
        const KinematicBodyConfig &config() const { return config_; }

    private:
        SweepBox boxAt_(Vec3 feet) const;

        Node3D &node_;
        CollisionWorld &world_;
        KinematicBodyConfig config_;
        const Voxel3D *selfVolume_ = nullptr;
        BodyId bodyId_{};
        Vec3 velocity_{ 0.0f, 0.0f, 0.0f };
        bool grounded_ = false;
    };
}
