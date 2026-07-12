#include <gtest/gtest.h>
#include "engine/rendering/world_brick_grid.hpp"
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
            : grid_(IVec3{ 16, 8, 16 })
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
            grid_.setWindow({ 0, 0, 0 }, { 2, 2, 2 });
            grid_.writeChunk({ 0, 0, 0 }, terrain_);
        }

        VoxelWorldQuery query() const { return { grid_, pool_, kVs }; }

        static KinematicBodyConfig smallBody() {
            return KinematicBodyConfig{ .size = Vec3(0.1f, 0.3f, 0.1f),
                                        .gravity = 10.0f,
                                        .terminalSpeed = 50.0f,
                                        .jumpSpeed = 2.0f,
                                        .stepUpHeight = 0.25f };
        }

        BrickPool pool_;
        WorldBrickGrid grid_;
        VoxelData terrain_;
    };

    void settle(KinematicBody &body, const VoxelWorldQuery &q, int32_t steps = 60) {
        for (int32_t i = 0; i < steps; ++i) {
            body.update(q, MoveInput{}, 1.0f / 60.0f);
        }
    }
}

TEST_F(KinematicBodyTest, FallsAndLandsOnFloor) {
    Node3D entity;
    entity.setPosition(Vec3(0.4f, 1.6f, 0.4f));
    KinematicBody body(entity, smallBody());
    const VoxelWorldQuery q = query();

    EXPECT_FALSE(body.grounded());
    settle(body, q);
    EXPECT_TRUE(body.grounded());
    EXPECT_NEAR(entity.position().y, 0.8f, 2e-3f);
    EXPECT_FLOAT_EQ(body.velocity().y, 0.0f);
}

TEST_F(KinematicBodyTest, JumpRisesThenLandsAgain) {
    Node3D entity;
    entity.setPosition(Vec3(0.4f, 0.9f, 0.4f));
    KinematicBody body(entity, smallBody());
    const VoxelWorldQuery q = query();
    settle(body, q);
    ASSERT_TRUE(body.grounded());

    body.update(q, MoveInput{ .walk = Vec3(0.0f, 0.0f, 0.0f), .jump = true }, 1.0f / 60.0f);
    EXPECT_FALSE(body.grounded());
    EXPECT_GT(entity.position().y, 0.8f);

    settle(body, q, 120);
    EXPECT_TRUE(body.grounded());
    EXPECT_NEAR(entity.position().y, 0.8f, 2e-3f);
}

TEST_F(KinematicBodyTest, WalksAndStepsUpLowLedge) {
    Node3D entity;
    entity.setPosition(Vec3(0.4f, 0.9f, 0.4f));
    KinematicBody body(entity, smallBody());
    const VoxelWorldQuery q = query();
    settle(body, q);

    for (int32_t i = 0; i < 90; ++i) {
        body.update(q, MoveInput{ .walk = Vec3(0.5f, 0.0f, 0.0f) }, 1.0f / 60.0f);
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
    KinematicBody body(entity, cfg);
    const VoxelWorldQuery q = query();
    settle(body, q);

    for (int32_t i = 0; i < 120; ++i) {
        body.update(q, MoveInput{ .walk = Vec3(0.5f, 0.0f, 0.0f) }, 1.0f / 60.0f);
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
    grid_.writeChunk({ 0, 1, 0 }, ceiling);

    Node3D entity;
    entity.setPosition(Vec3(0.4f, 0.9f, 0.4f));
    KinematicBodyConfig cfg = smallBody();
    cfg.jumpSpeed = 5.0f;
    KinematicBody body(entity, cfg);
    const VoxelWorldQuery q = query();
    settle(body, q);

    body.update(q, MoveInput{ .walk = Vec3(0.0f, 0.0f, 0.0f), .jump = true }, 1.0f / 60.0f);
    float peak = entity.position().y;
    for (int32_t i = 0; i < 30; ++i) {
        body.update(q, MoveInput{}, 1.0f / 60.0f);
        peak = std::max(peak, entity.position().y);
    }
    EXPECT_LE(peak + smallBody().size.y, 1.2f + 1e-2f);
    settle(body, q, 60);
    EXPECT_TRUE(body.grounded());
}
