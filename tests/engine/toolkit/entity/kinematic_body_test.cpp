#include <gtest/gtest.h>
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/voxel_data.hpp"
#include "engine/toolkit/entity/kinematic_body.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"

using namespace RAGE;
using namespace RAGE::Toolkit;

namespace {
    constexpr float kVs = 0.1f;
    constexpr uint32_t kStone = 0xFF808080u;

    /// Floor at world voxel layers y in [0,8) (top plane y = 0.8), plus a 2-voxel step
    /// (height 0.2) whose top is at y = 1.0, occupying x >= 0.8.
    class KinematicBodyTest : public ::testing::Test {
    protected:
        KinematicBodyTest()
            : world_(pool_, IVec3{ 16, 32, 16 })   // 2x4x2-brick lattice, window at origin
            , terrain_(pool_, IVec3{ 16, 16, 16 }) {
            for (int32_t z = 0; z < 16; ++z) {
                for (int32_t x = 0; x < 16; ++x) {
                    for (int32_t y = 0; y < 8; ++y) {
                        terrain_.setVoxel({ x, y, z }, kStone);
                    }
                    if (x >= 8) {
                        terrain_.setVoxel({ x, 8, z }, kStone);
                        terrain_.setVoxel({ x, 9, z }, kStone);
                    }
                }
            }
            world_.adoptBricksFrom(terrain_, IVec3{ 0, 0, 0 });
        }


        static KinematicBodyConfig smallBody() {
            return KinematicBodyConfig{ .size = Vec3(0.1f, 0.3f, 0.1f),
                                        .gravity = 10.0f,
                                        .terminalSpeed = 50.0f,
                                        .jumpSpeed = 2.0f,
                                        .stepUpHeight = 0.25f };
        }

        BrickPool pool_;
        VoxelData world_;
        VoxelData terrain_;
    };

    void settle(KinematicBody &body, int32_t steps = 60) {
        for (int32_t i = 0; i < steps; ++i) {
            body.update(MoveInput{}, 1.0f / 60.0f);
        }
    }
}

TEST_F(KinematicBodyTest, FallsAndLandsOnFloor) {
    Node3D entity;
    entity.setPosition(Vec3(0.4f, 1.6f, 0.4f));
    CollisionWorld world(world_, pool_, kVs);
    KinematicBody body(entity, world, smallBody());

    EXPECT_FALSE(body.grounded());
    settle(body);
    EXPECT_TRUE(body.grounded());
    EXPECT_NEAR(entity.position().y, 0.8f, 2e-3f);
    EXPECT_FLOAT_EQ(body.velocity().y, 0.0f);
}

TEST_F(KinematicBodyTest, JumpRisesThenLandsAgain) {
    Node3D entity;
    entity.setPosition(Vec3(0.4f, 0.9f, 0.4f));
    CollisionWorld world(world_, pool_, kVs);
    KinematicBody body(entity, world, smallBody());
    settle(body);
    ASSERT_TRUE(body.grounded());

    body.update(MoveInput{ .walk = Vec3(0.0f, 0.0f, 0.0f), .jump = true }, 1.0f / 60.0f);
    EXPECT_FALSE(body.grounded());
    EXPECT_GT(entity.position().y, 0.8f);

    settle(body, 120);
    EXPECT_TRUE(body.grounded());
    EXPECT_NEAR(entity.position().y, 0.8f, 2e-3f);
}

TEST_F(KinematicBodyTest, WalksAndStepsUpLowLedge) {
    Node3D entity;
    entity.setPosition(Vec3(0.4f, 0.9f, 0.4f));
    CollisionWorld world(world_, pool_, kVs);
    KinematicBody body(entity, world, smallBody());
    settle(body);

    for (int32_t i = 0; i < 90; ++i) {
        body.update(MoveInput{ .walk = Vec3(0.5f, 0.0f, 0.0f) }, 1.0f / 60.0f);
    }
    EXPECT_GT(entity.position().x, 0.9f);
    EXPECT_NEAR(entity.position().y, 1.0f, 2e-2f);
    EXPECT_TRUE(body.grounded());
}

TEST_F(KinematicBodyTest, TallLedgeBlocksWhenStepUpDisabled) {
    Node3D entity;
    entity.setPosition(Vec3(0.4f, 0.9f, 0.4f));
    KinematicBodyConfig cfg = smallBody();
    cfg.stepUpHeight = 0.0f;
    CollisionWorld world(world_, pool_, kVs);
    KinematicBody body(entity, world, cfg);
    settle(body);

    for (int32_t i = 0; i < 120; ++i) {
        body.update(MoveInput{ .walk = Vec3(0.5f, 0.0f, 0.0f) }, 1.0f / 60.0f);
    }
    EXPECT_LT(entity.position().x, 0.8f);
    EXPECT_NEAR(entity.position().y, 0.8f, 2e-3f);
}

TEST_F(KinematicBodyTest, CeilingCancelsAscent) {
    // Ceiling: solid layer right above a 0.4-tall gap over the floor.
    VoxelData ceiling(pool_, IVec3{ 16, 16, 16 });
    for (int32_t z = 0; z < 16; ++z) {
        for (int32_t x = 0; x < 8; ++x) {
            ceiling.setVoxel({ x, 4, z }, kStone);   // world y = 1.2..1.3
        }
    }
    world_.adoptBricksFrom(ceiling, IVec3{ 0, 1, 0 });

    Node3D entity;
    entity.setPosition(Vec3(0.4f, 0.9f, 0.4f));
    KinematicBodyConfig cfg = smallBody();
    cfg.jumpSpeed = 5.0f;
    CollisionWorld world(world_, pool_, kVs);
    KinematicBody body(entity, world, cfg);
    settle(body);

    body.update(MoveInput{ .walk = Vec3(0.0f, 0.0f, 0.0f), .jump = true }, 1.0f / 60.0f);
    float peak = entity.position().y;
    for (int32_t i = 0; i < 30; ++i) {
        body.update(MoveInput{}, 1.0f / 60.0f);
        peak = std::max(peak, entity.position().y);
    }
    EXPECT_LE(peak + smallBody().size.y, 1.2f + 1e-2f);
    settle(body, 60);
    EXPECT_TRUE(body.grounded());
}

TEST_F(KinematicBodyTest, HeavierBodyMovesLessInMutualOverlap) {
    CollisionWorld world(world_, pool_, kVs);

    Node3D light;
    Node3D heavy;
    // Overlapping boxes on the floor, offset slightly in x so separation is lateral.
    light.setPosition(Vec3(0.38f, 0.8001f, 0.4f));
    heavy.setPosition(Vec3(0.44f, 0.8001f, 0.4f));
    KinematicBodyConfig lightCfg = smallBody();
    lightCfg.mass = 10.0f;
    KinematicBodyConfig heavyCfg = smallBody();
    heavyCfg.mass = 1000.0f;
    KinematicBody lightBody(light, world, lightCfg);
    KinematicBody heavyBody(heavy, world, heavyCfg);
    EXPECT_EQ(world.bodyCount(), 2u);

    const float lightStart = light.position().x;
    const float heavyStart = heavy.position().x;
    for (int32_t i = 0; i < 30; ++i) {
        lightBody.update(MoveInput{}, 1.0f / 60.0f);
        heavyBody.update(MoveInput{}, 1.0f / 60.0f);
    }
    const float lightMoved = std::abs(light.position().x - lightStart);
    const float heavyMoved = std::abs(heavy.position().x - heavyStart);
    EXPECT_GT(lightMoved, heavyMoved * 5.0f);
    EXPECT_GT(lightMoved + heavyMoved, 0.03f);
}

TEST_F(KinematicBodyTest, DepenetrationPushesBodyOutOfSolid) {
    CollisionWorld world(world_, pool_, kVs);
    Node3D entity;
    // Feet 0.05 below the floor top (0.8): overlapping the floor by half a voxel.
    entity.setPosition(Vec3(0.4f, 0.75f, 0.4f));
    KinematicBody body(entity, world, smallBody());

    body.update(MoveInput{}, 1.0f / 60.0f);
    EXPECT_GE(entity.position().y, 0.8f - 1e-3f);
    EXPECT_TRUE(body.grounded());
}
