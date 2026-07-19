#include "collision_world.hpp"

#include "engine/toolkit/entity/kinematic_body.hpp"

namespace RAGE::Toolkit {
    void CollisionWorld::add(KinematicBody &body) { body.bindWorld_(*this); }
}
