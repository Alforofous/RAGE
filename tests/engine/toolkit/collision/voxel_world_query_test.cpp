#include <gtest/gtest.h>
#include "engine/rendering/world_brick_grid.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel_data.hpp"
#include "engine/toolkit/collision/voxel_world_query.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"

using namespace RAGE;
using namespace RAGE::Toolkit;

namespace {
    constexpr float kVs = 0.1f;
    constexpr uint32_t kStone = 0xFF808080u;

    /// One 16x16x16-voxel chunk: solid floor occupying voxel layers y in [0, 8).
    class VoxelWorldQueryTest : public ::testing::Test {
    protected:
        VoxelWorldQueryTest()
            : grid_(IVec3{ 16, 8, 16 })
            , floor_(pool_, IVec3{ 16, 16, 16 }) {
            for (int32_t z = 0; z < 16; ++z) {
                for (int32_t y = 0; y < 8; ++y) {
                    for (int32_t x = 0; x < 16; ++x) {
                        floor_.setVoxel({ x, y, z }, kStone);
                    }
                }
            }
            grid_.setWindow({ 0, 0, 0 }, { 2, 2, 2 });
            grid_.writeChunk({ 0, 0, 0 }, floor_);
        }

        VoxelWorldQuery query() const { return { grid_, pool_, kVs }; }

        BrickPool pool_;
        WorldBrickGrid grid_;
        VoxelData floor_;
    };
}

TEST_F(VoxelWorldQueryTest, SolidInsideFloorAirAbove) {
    const VoxelWorldQuery q = query();
    EXPECT_TRUE(q.solid({ 5, 0, 5 }));
    EXPECT_TRUE(q.solid({ 5, 7, 5 }));
    EXPECT_FALSE(q.solid({ 5, 8, 5 }));
    EXPECT_FALSE(q.solid({ 5, 15, 5 }));
}

TEST_F(VoxelWorldQueryTest, OutsideWindowIsAir) {
    const VoxelWorldQuery q = query();
    EXPECT_FALSE(q.solid({ -1, 0, 5 }));
    EXPECT_FALSE(q.solid({ 5, 0, 200 }));
    EXPECT_FALSE(q.solid({ 5, -1, 5 }));
}

TEST_F(VoxelWorldQueryTest, FreeFallWithNoObstacleMovesFully) {
    const VoxelWorldQuery q = query();
    const SweepBox box{ .min = Vec3(0.5f, 1.5f, 0.5f), .max = Vec3(0.56f, 1.68f, 0.56f) };
    const SweepResult r = q.sweepAABB(box, Vec3(0.0f, -0.3f, 0.0f));
    EXPECT_FLOAT_EQ(r.moved.y, -0.3f);
    EXPECT_FALSE(r.anyHit());
}

TEST_F(VoxelWorldQueryTest, FallClipsOnFloorTop) {
    const VoxelWorldQuery q = query();
    // Floor top plane is at y = 8 * kVs = 0.8. Falling from 1.0 by -0.5 must stop there.
    const SweepBox box{ .min = Vec3(0.5f, 1.0f, 0.5f), .max = Vec3(0.56f, 1.18f, 0.56f) };
    const SweepResult r = q.sweepAABB(box, Vec3(0.0f, -0.5f, 0.0f));
    EXPECT_TRUE(r.hitY);
    EXPECT_NEAR(r.moved.y, -0.2f, 2e-4f);
    EXPECT_GE(box.min.y + r.moved.y, 0.8f);
}

TEST_F(VoxelWorldQueryTest, RestingOnFloorStaysPut) {
    const VoxelWorldQuery q = query();
    SweepBox box{ .min = Vec3(0.5f, 0.8f, 0.5f), .max = Vec3(0.56f, 0.98f, 0.56f) };
    SweepBox settled = box;
    const SweepResult first = q.sweepAABB(box, Vec3(0.0f, -0.1f, 0.0f));
    settled.min.y += first.moved.y;
    settled.max.y += first.moved.y;
    const SweepResult r = q.sweepAABB(settled, Vec3(0.0f, -0.1f, 0.0f));
    EXPECT_TRUE(r.hitY);
    EXPECT_NEAR(r.moved.y, 0.0f, 2e-4f);
}

TEST_F(VoxelWorldQueryTest, HorizontalMoveSlidesAlongGround) {
    const VoxelWorldQuery q = query();
    // Resting on the floor, moving diagonally down+forward: Y clips, X passes.
    const SweepBox box{ .min = Vec3(0.3f, 0.8001f, 0.3f), .max = Vec3(0.36f, 0.98f, 0.36f) };
    const SweepResult r = q.sweepAABB(box, Vec3(0.25f, -0.05f, 0.0f));
    EXPECT_TRUE(r.hitY);
    EXPECT_FALSE(r.hitX);
    EXPECT_FLOAT_EQ(r.moved.x, 0.25f);
}

TEST_F(VoxelWorldQueryTest, WallClipsHorizontalMotion) {
    // Add a wall: fill x in [80..88) voxels? Use a second chunk column as wall at x >= 8 above floor.
    VoxelData wall(pool_, IVec3{ 16, 16, 16 });
    for (int32_t z = 0; z < 16; ++z) {
        for (int32_t y = 0; y < 16; ++y) {
            for (int32_t x = 8; x < 16; ++x) {
                wall.setVoxel({ x, y, z }, kStone);
            }
        }
    }
    grid_.writeChunk({ 0, 1, 0 }, wall);   // occupies world voxels y in [8,24), x in [8,16)

    const VoxelWorldQuery q = query();
    // Box above the floor at x ~0.5, moving +x toward the wall face at x = 0.8.
    const SweepBox box{ .min = Vec3(0.5f, 0.9f, 0.5f), .max = Vec3(0.62f, 1.1f, 0.62f) };
    const SweepResult r = q.sweepAABB(box, Vec3(0.5f, 0.0f, 0.0f));
    EXPECT_TRUE(r.hitX);
    EXPECT_NEAR(r.moved.x, 0.8f - 0.62f, 2e-4f);
}

TEST_F(VoxelWorldQueryTest, LargeDeltaCannotTunnelThroughThinWall) {
    VoxelData wall(pool_, IVec3{ 16, 16, 16 });
    for (int32_t z = 0; z < 16; ++z) {
        for (int32_t y = 0; y < 16; ++y) {
            wall.setVoxel({ 8, y, z }, kStone);   // one-voxel-thick wall at world x = 8
        }
    }
    grid_.writeChunk({ 0, 1, 0 }, wall);

    const VoxelWorldQuery q = query();
    const SweepBox box{ .min = Vec3(0.1f, 0.9f, 0.5f), .max = Vec3(0.2f, 1.1f, 0.56f) };
    const SweepResult r = q.sweepAABB(box, Vec3(5.0f, 0.0f, 0.0f));
    EXPECT_TRUE(r.hitX);
    EXPECT_NEAR(r.moved.x, 0.8f - 0.2f, 2e-4f);
}
