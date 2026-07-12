#include "kinematic_body.hpp"

#include <algorithm>
#include <cmath>

namespace RAGE::Toolkit {
    namespace {
        SweepBox translated(const SweepBox &b, Vec3 d) {
            return SweepBox{ .min = b.min + d, .max = b.max + d };
        }
    }

    KinematicBody::KinematicBody(Node3D &node, KinematicBodyConfig config,
                                 const Voxel3D *selfVolume)
        : node_(node)
        , config_(config)
        , selfVolume_(selfVolume) {}

    SweepBox KinematicBody::boxAt_(Vec3 feet) const {
        const Vec3 half{ config_.size.x * 0.5f, 0.0f, config_.size.z * 0.5f };
        return SweepBox{
            .min = Vec3(feet.x - half.x, feet.y, feet.z - half.z),
            .max = Vec3(feet.x + half.x, feet.y + config_.size.y, feet.z + half.z),
        };
    }

    void KinematicBody::update(const CollisionWorld &world, const MoveInput &input, float dt) {
        velocity_.x = input.walk.x;
        velocity_.z = input.walk.z;
        if (grounded_ && input.jump) {
            velocity_.y = config_.jumpSpeed;
        }
        velocity_.y = std::max(velocity_.y - (config_.gravity * dt), -config_.terminalSpeed);

        const Vec3 delta = velocity_ * dt;
        const SweepBox box = boxAt_(node_.position());
        SweepResult r = world.sweepAABB(box, delta, selfVolume_);

        // Step-up: a grounded horizontal hit retries the blocked remainder from one
        // ledge higher (up → forward → back down), and wins only if it gets farther.
        const bool blockedHorizontally = r.hitX || r.hitZ;
        if (blockedHorizontally && grounded_ && config_.stepUpHeight > 0.0f) {
            const Vec3 remainder{ delta.x - r.moved.x, 0.0f, delta.z - r.moved.z };
            const SweepBox atClip = translated(box, r.moved);
            const SweepResult up =
                world.sweepAABB(atClip, Vec3(0.0f, config_.stepUpHeight, 0.0f), selfVolume_);
            const SweepResult fwd =
                world.sweepAABB(translated(atClip, up.moved), remainder, selfVolume_);
            const float gained = std::sqrt((fwd.moved.x * fwd.moved.x) + (fwd.moved.z * fwd.moved.z));
            if (gained > 1e-4f) {
                const SweepBox atFwd = translated(atClip, up.moved + fwd.moved);
                const SweepResult down =
                    world.sweepAABB(atFwd, Vec3(0.0f, -up.moved.y, 0.0f), selfVolume_);
                r.moved = r.moved + up.moved + fwd.moved + down.moved;
                r.hitX = fwd.hitX;
                r.hitZ = fwd.hitZ;
                r.hitY = r.hitY || down.hitY;
            }
        }

        node_.setPosition(node_.position() + r.moved);

        if (r.hitY) {
            grounded_ = velocity_.y < 0.0f;
            velocity_.y = 0.0f;
        } else {
            grounded_ = false;
        }
        if (r.hitX) {
            velocity_.x = 0.0f;
        }
        if (r.hitZ) {
            velocity_.z = 0.0f;
        }
    }
}
