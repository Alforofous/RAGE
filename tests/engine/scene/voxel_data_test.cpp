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

// ---- Window contract (api-north-star N7a) ------------------------------------

TEST(VoxelDataWindow, DefaultWindowIsStationaryAtOrigin) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 24, 24, 24 });
    EXPECT_FALSE(d.isWindowed());
    EXPECT_EQ(d.windowOriginBrick(), (IVec3{ 0, 0, 0 }));
    EXPECT_EQ(d.storageBrickDims(), d.brickDims());
}

TEST(VoxelDataWindow, FirstCenterCallConvertsAndReportsWholeWindowMinusOverlap) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 32, 32, 32 });   // 4x4x4 bricks
    const auto entered = d.setWindowCenterBrick(IVec3{ 100, 0, 100 });
    EXPECT_TRUE(d.isWindowed());
    EXPECT_EQ(d.windowOriginBrick(), (IVec3{ 98, -2, 98 }));
    // Teleport far from the default window: everything entered.
    size_t enteredCells = 0;
    for (const auto &r : entered) {
        enteredCells += static_cast<size_t>(r.brickDims.x) * static_cast<size_t>(r.brickDims.y)
                        * static_cast<size_t>(r.brickDims.z);
    }
    EXPECT_EQ(enteredCells, 64u);
}

TEST(VoxelDataWindow, ConversionKeepsExistingVoxelsAddressable) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 32, 32, 32 });
    d.setVoxel(IVec3{ 5, 6, 7 }, 0xAABBCCDDu);
    d.setVoxel(IVec3{ 30, 30, 30 }, 0x11223344u);
    // Center that keeps origin at {0,0,0}: center == brickDims/2.
    const auto entered = d.setWindowCenterBrick(IVec3{ 2, 2, 2 });
    EXPECT_TRUE(d.isWindowed());
    EXPECT_TRUE(entered.empty());
    EXPECT_EQ(d.voxel(IVec3{ 5, 6, 7 }), 0xAABBCCDDu);
    EXPECT_EQ(d.voxel(IVec3{ 30, 30, 30 }), 0x11223344u);
}

TEST(VoxelDataWindow, SlideFreesDepartingBricksAndKeepsSurvivors) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 32, 32, 32 });   // 4^3 bricks, window [0,4)^3
    d.setVoxel(IVec3{ 1, 1, 1 }, 0xFFFF0000u);     // brick {0,0,0} — will depart
    d.setVoxel(IVec3{ 25, 25, 25 }, 0xFF00FF00u);  // brick {3,3,3} — survives
    EXPECT_EQ(pool.allocated(), 2u);

    // Slide +1 brick on every axis: window becomes [1,5)^3.
    const auto entered = d.setWindowCenterBrick(IVec3{ 3, 3, 3 });
    EXPECT_EQ(d.windowOriginBrick(), (IVec3{ 1, 1, 1 }));
    EXPECT_EQ(pool.allocated(), 1u);                     // departing brick freed
    EXPECT_EQ(d.voxel(IVec3{ 1, 1, 1 }), 0u);            // outside window now
    EXPECT_EQ(d.voxel(IVec3{ 25, 25, 25 }), 0xFF00FF00u);  // survivor intact

    // Entered = window minus overlap = 4^3 - 3^3 = 37 cells.
    size_t enteredCells = 0;
    for (const auto &r : entered) {
        enteredCells += static_cast<size_t>(r.brickDims.x) * static_cast<size_t>(r.brickDims.y)
                        * static_cast<size_t>(r.brickDims.z);
    }
    EXPECT_EQ(enteredCells, 37u);
}

TEST(VoxelDataWindow, ReenteredRegionReadsEmptyAndIsWritable) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 32, 32, 32 });
    d.setVoxel(IVec3{ 1, 1, 1 }, 0xDEADBEEFu);
    d.setWindowCenterBrick(IVec3{ 12, 2, 2 });   // far slide: brick {0,0,0} departs
    d.setWindowCenterBrick(IVec3{ 2, 2, 2 });    // slide back to origin
    EXPECT_EQ(d.voxel(IVec3{ 1, 1, 1 }), 0u);    // volume forgot — by contract
    d.setVoxel(IVec3{ 1, 1, 1 }, 0x0000FFFFu);   // and the slot is reusable
    EXPECT_EQ(d.voxel(IVec3{ 1, 1, 1 }), 0x0000FFFFu);
}

TEST(VoxelDataWindow, WritesOutsideWindowAreIgnored) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 32, 32, 32 });
    d.setWindowCenterBrick(IVec3{ 100, 100, 100 });
    d.setVoxel(IVec3{ 0, 0, 0 }, 0xFFFFFFFFu);   // way outside the window
    EXPECT_EQ(pool.allocated(), 0u);
    EXPECT_EQ(d.voxel(IVec3{ 0, 0, 0 }), 0u);
}

TEST(VoxelDataWindow, NegativeCoordinatesWork) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 32, 32, 32 });
    d.setWindowCenterBrick(IVec3{ 0, 0, 0 });    // window [-2,2)^3 bricks
    d.setVoxel(IVec3{ -5, -6, -7 }, 0x12345678u);
    EXPECT_EQ(d.voxel(IVec3{ -5, -6, -7 }), 0x12345678u);
    EXPECT_EQ(d.handleAt(IVec3{ -1, -1, -1 }), d.handleAt(IVec3{ -1, -1, -1 }));
}

TEST(VoxelDataWindow, NonPow2DimsRoundStorageUp) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 24, 24, 24 });      // 3^3 bricks
    d.setWindowCenterBrick(IVec3{ 50, 0, 0 });
    EXPECT_EQ(d.storageBrickDims(), (IVec3{ 4, 4, 4 }));
    EXPECT_EQ(d.brickDims(), (IVec3{ 3, 3, 3 }));  // window extent unchanged
}

TEST(VoxelDataWindow, DestructorReleasesWindowedBricks) {
    BrickPool pool;
    {
        VoxelData d(pool, IVec3{ 32, 32, 32 });
        d.setWindowCenterBrick(IVec3{ 40, 40, 40 });
        d.setVoxel(IVec3{ 40 * 8, 40 * 8, 40 * 8 }, 0xFFFFFFFFu);
        EXPECT_EQ(pool.allocated(), 1u);
    }
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(VoxelDataWindow, RepeatedSameCenterIsNoOp) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 32, 32, 32 });
    d.setWindowCenterBrick(IVec3{ 10, 10, 10 });
    d.setVoxel(IVec3{ 80, 80, 80 }, 0xAA0000FFu);
    const auto entered = d.setWindowCenterBrick(IVec3{ 10, 10, 10 });
    EXPECT_TRUE(entered.empty());
    EXPECT_EQ(d.voxel(IVec3{ 80, 80, 80 }), 0xAA0000FFu);
}

TEST(VoxelDataWindow, FillFromPackedThrowsOnceWindowed) {
    BrickPool pool;
    VoxelData d(pool, IVec3{ 16, 16, 16 });
    d.setWindowCenterBrick(IVec3{ 5, 5, 5 });
    std::vector<uint32_t> src(16 * 16 * 16, 0u);
    EXPECT_THROW(d.fillFromPackedRGBA8(src.data(), IVec3{ 16, 16, 16 }), std::logic_error);
}
