#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel_data.hpp"
#include "math/ivec.hpp"

using namespace RAGE;

namespace {
    constexpr uint32_t kRed = 0xFF0000FFu;
    constexpr uint32_t kBlue = 0xFFFF0000u;
}

TEST(VoxelData, BrickDimsRoundUpFromVoxelDims) {
    BrickPool pool;
    const VoxelData v(pool, { 9, 16, 1 });
    EXPECT_EQ(v.brickDims(), IVec3(2, 2, 1));
}

TEST(VoxelData, ThrowsOnNonPositiveDims) {
    BrickPool pool;
    EXPECT_THROW(VoxelData(pool, { 0, 1, 1 }), std::invalid_argument);
    EXPECT_THROW(VoxelData(pool, { 1, -1, 1 }), std::invalid_argument);
}

TEST(VoxelData, NewVoxelDataAllocatesNoBricks) {
    BrickPool pool;
    const VoxelData v(pool, { 32, 32, 32 });
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(VoxelData, SetVoxelOfZeroIntoEmptyRegionIsNoOp) {
    BrickPool pool;
    VoxelData v(pool, { 16, 16, 16 });
    v.setVoxel({ 3, 4, 5 }, 0u);
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(VoxelData, SetVoxelAllocatesOneBrickPerRegion) {
    BrickPool pool;
    VoxelData v(pool, { 32, 32, 32 });
    v.setVoxel({ 0, 0, 0 }, kRed);
    EXPECT_EQ(pool.allocated(), 1u);
    v.setVoxel({ 3, 4, 2 }, kRed);     // same brick
    EXPECT_EQ(pool.allocated(), 1u);
    v.setVoxel({ 8, 0, 0 }, kRed);     // next brick on x
    EXPECT_EQ(pool.allocated(), 2u);
}

TEST(VoxelData, RoundTripsViaVoxelGetter) {
    BrickPool pool;
    VoxelData v(pool, { 16, 16, 16 });
    v.setVoxel({ 1, 2, 3 }, kRed);
    v.setVoxel({ 10, 11, 12 }, kBlue);
    EXPECT_EQ(v.voxel({ 1, 2, 3 }), kRed);
    EXPECT_EQ(v.voxel({ 10, 11, 12 }), kBlue);
    EXPECT_EQ(v.voxel({ 5, 5, 5 }), 0u);
}

TEST(VoxelData, OutOfBoundsReadsReturnZero) {
    BrickPool pool;
    const VoxelData v(pool, { 4, 4, 4 });
    EXPECT_EQ(v.voxel({ -1, 0, 0 }), 0u);
    EXPECT_EQ(v.voxel({ 0, 4, 0 }), 0u);
    EXPECT_EQ(v.voxel({ 100, 0, 0 }), 0u);
}

TEST(VoxelData, OutOfBoundsWritesAreIgnored) {
    BrickPool pool;
    VoxelData v(pool, { 4, 4, 4 });
    v.setVoxel({ -1, 0, 0 }, kRed);
    v.setVoxel({ 0, 5, 0 }, kRed);
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(VoxelData, DestructorReleasesAllAllocatedBricks) {
    BrickPool pool;
    {
        VoxelData v(pool, { 32, 32, 32 });
        v.setVoxel({ 0, 0, 0 }, kRed);
        v.setVoxel({ 8, 0, 0 }, kRed);
        EXPECT_EQ(pool.allocated(), 2u);
    }
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(VoxelData, FillFromPackedRGBA8AllocatesOnlyNonEmptyBricks) {
    BrickPool pool;
    VoxelData v(pool, { 16, 8, 8 });    // 2x1x1 = 2 bricks
    std::vector<uint32_t> src(16u * 8u * 8u, 0u);
    src[0] = kRed;                       // brick 0
    // brick 1 stays empty
    v.fillFromPackedRGBA8(src.data(), { 16, 8, 8 });
    EXPECT_EQ(pool.allocated(), 1u);
    EXPECT_EQ(v.voxel({ 0, 0, 0 }), kRed);
    EXPECT_EQ(v.handleAt({ 1, 0, 0 }), kEmptyBrick);
}

TEST(VoxelData, FillFromPackedRGBA8RoundTripsAllVoxels) {
    BrickPool pool;
    VoxelData v(pool, { 8, 8, 8 });
    std::vector<uint32_t> src(8u * 8u * 8u, 0u);
    src[0] = kRed;                              // (0,0,0)
    src[1u + (2u * 8u) + (3u * 64u)] = kBlue;   // (1,2,3)
    v.fillFromPackedRGBA8(src.data(), { 8, 8, 8 });
    EXPECT_EQ(v.voxel({ 0, 0, 0 }), kRed);
    EXPECT_EQ(v.voxel({ 1, 2, 3 }), kBlue);
}

TEST(VoxelData, FillFromPackedRGBA8MismatchedDimsThrows) {
    BrickPool pool;
    VoxelData v(pool, { 8, 8, 8 });
    const std::vector<uint32_t> src(8u * 8u * 8u, 0u);
    EXPECT_THROW(v.fillFromPackedRGBA8(src.data(), { 16, 8, 8 }), std::invalid_argument);
}

TEST(VoxelData, BricksMarkedDirtyAfterMutation) {
    BrickPool pool;
    VoxelData v(pool, { 8, 8, 8 });
    (void)pool.drainDirty();
    v.setVoxel({ 0, 0, 0 }, kRed);
    EXPECT_EQ(pool.drainDirty().size(), 1u);
}

// RAII cycle: many build/teardown rounds force per-cycle leaks to accumulate visibly.
TEST(VoxelData, DestroyingVoxelDatasReleasesAllBricksBackToPool) {
    BrickPool pool;

    constexpr int32_t kCycles = 10;
    constexpr int32_t kVoxelDimsPerSide = 16;     // 2³ bricks per VoxelData
    constexpr int32_t kVoxelDataCount = 5;

    for (int32_t cycle = 0; cycle < kCycles; ++cycle) {
        SCOPED_TRACE("cycle " + std::to_string(cycle));

        std::vector<std::unique_ptr<VoxelData>> scene;
        const size_t voxelsPerData = static_cast<size_t>(kVoxelDimsPerSide) * kVoxelDimsPerSide
                                     * kVoxelDimsPerSide;
        for (int32_t i = 0; i < kVoxelDataCount; ++i) {
            auto vd = std::make_unique<VoxelData>(pool,
                IVec3{ kVoxelDimsPerSide, kVoxelDimsPerSide, kVoxelDimsPerSide });
            std::vector<uint32_t> src(voxelsPerData,
                                       (i >= kVoxelDataCount - 2) ? kRed : (kRed + static_cast<uint32_t>(i)));
            vd->fillFromPackedRGBA8(src.data(),
                IVec3{ kVoxelDimsPerSide, kVoxelDimsPerSide, kVoxelDimsPerSide });
            scene.push_back(std::move(vd));
        }
        ASSERT_GT(pool.allocated(), 0u) << "scene must have populated bricks before teardown";

        scene.clear();

        EXPECT_EQ(pool.allocated(), 0u) << "every brick must be back on the free list";
        EXPECT_EQ(pool.logicalBricks(), 0u) << "refcount sum must be zero with no live handle holders";
        EXPECT_EQ(pool.drainDirty().size(), 0u) << "no dirty bricks should outlive the scene";
    }
}

TEST(VoxelData, DestroyingSharedDedupedBrickDoesNotDoubleReleaseOrLeak) {
    BrickPool pool;
    {
        VoxelData a(pool, { 8, 8, 8 });
        VoxelData b(pool, { 8, 8, 8 });
        const std::vector<uint32_t> src(8 * 8 * 8, kRed);
        a.fillFromPackedRGBA8(src.data(), { 8, 8, 8 });
        b.fillFromPackedRGBA8(src.data(), { 8, 8, 8 });
        EXPECT_EQ(pool.allocated(), 1u);    // dedup'd to one slot
        EXPECT_EQ(pool.logicalBricks(), 2u);
    }
    EXPECT_EQ(pool.allocated(), 0u);
    EXPECT_EQ(pool.logicalBricks(), 0u);
}
