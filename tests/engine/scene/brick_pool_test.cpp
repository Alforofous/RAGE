#include <gtest/gtest.h>
#include <stdexcept>
#include "engine/scene/brick.hpp"
#include "engine/scene/brick_pool.hpp"

using namespace RAGE;

TEST(Brick, FlatIndexMatchesShaderConvention) {
    EXPECT_EQ(brickVoxelIndex(0, 0, 0), 0u);
    EXPECT_EQ(brickVoxelIndex(1, 0, 0), 1u);
    EXPECT_EQ(brickVoxelIndex(0, 1, 0), 8u);
    EXPECT_EQ(brickVoxelIndex(0, 0, 1), 64u);
    EXPECT_EQ(brickVoxelIndex(7, 7, 7), 511u);
}

TEST(Brick, IsExactly2kbAndZeroInitialised) {
    static_assert(sizeof(Brick) == 2048);
    const Brick b{};
    for (uint32_t v : b.voxels) {
        EXPECT_EQ(v, 0u);
    }
}

TEST(BrickPool, ReservesIndexZeroAsEmptySentinel) {
    const BrickPool pool;
    EXPECT_EQ(pool.capacity(), 1u);    // only the sentinel
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(BrickPool, AllocateReturnsNonZeroHandlesAndMarksDirty) {
    BrickPool pool;
    const BrickHandle a = pool.allocate();
    const BrickHandle b = pool.allocate();
    EXPECT_NE(a, kEmptyBrick);
    EXPECT_NE(b, kEmptyBrick);
    EXPECT_NE(a, b);
    EXPECT_EQ(pool.allocated(), 2u);

    const auto dirty = pool.drainDirty();
    ASSERT_EQ(dirty.size(), 2u);
    EXPECT_EQ(dirty[0], a);
    EXPECT_EQ(dirty[1], b);
}

TEST(BrickPool, ReleaseReusesHandleAndZeroesContents) {
    BrickPool pool;
    const BrickHandle a = pool.allocate();
    pool.writeVoxel(a, 42u, 0xDEADBEEFu);

    pool.release(a);
    EXPECT_EQ(pool.allocated(), 0u);

    const BrickHandle reused = pool.allocate();
    EXPECT_EQ(reused, a);
    EXPECT_EQ(pool.brick(reused).voxels[42], 0u);
}

TEST(BrickPool, ReleaseOfEmptyHandleIsNoOp) {
    BrickPool pool;
    EXPECT_NO_THROW(pool.release(kEmptyBrick));
}

TEST(BrickPool, AccessingEmptyOrOutOfRangeHandleThrows) {
    BrickPool pool;
    EXPECT_THROW(pool.brick(kEmptyBrick), std::out_of_range);
    EXPECT_THROW(pool.brick(BrickHandle{ 999u }), std::out_of_range);
    EXPECT_THROW(pool.release(BrickHandle{ 999u }), std::out_of_range);
}

TEST(BrickPool, MarkDirtyDedupesRepeatedCallsForSameHandle) {
    BrickPool pool;
    const BrickHandle h = pool.allocate();
    (void)pool.drainDirty();

    pool.markDirty(h);
    pool.markDirty(h);
    pool.markDirty(h);
    const auto dirty = pool.drainDirty();
    EXPECT_EQ(dirty.size(), 1u);
    EXPECT_EQ(dirty[0], h);
}

TEST(BrickPool, DrainDirtyEmptiesListAfterCall) {
    BrickPool pool;
    pool.allocate();
    pool.allocate();
    EXPECT_FALSE(pool.drainDirty().empty());
    EXPECT_TRUE(pool.drainDirty().empty());
}

TEST(BrickPool, MarkDirtyIgnoresEmptyHandle) {
    BrickPool pool;
    pool.markDirty(kEmptyBrick);
    EXPECT_TRUE(pool.drainDirty().empty());
}

TEST(BrickPool, GrowsBeyondInitialCapacity) {
    BrickPool pool;
    for (int i = 0; i < 100; ++i) {
        pool.allocate();
    }
    EXPECT_EQ(pool.allocated(), 100u);
    EXPECT_GE(pool.capacity(), 101u);
}

TEST(BrickPool, DedupOffAlwaysAllocatesFresh) {
    BrickPool pool;
    pool.setDedupEnabled(false);
    Brick contents{};
    contents.voxels[0] = 0xDEADBEEFu;

    const BrickHandle a = pool.acquireBrick(contents);
    const BrickHandle b = pool.acquireBrick(contents);
    EXPECT_NE(a, b);
    EXPECT_EQ(pool.allocated(), 2u);
    EXPECT_EQ(pool.logicalBricks(), 2u);
}

TEST(BrickPool, DedupOnReusesIdenticalContent) {
    BrickPool pool;
    Brick contents{};
    contents.voxels[0] = 0xDEADBEEFu;

    const BrickHandle a = pool.acquireBrick(contents);
    const BrickHandle b = pool.acquireBrick(contents);
    EXPECT_EQ(a, b);
    EXPECT_EQ(pool.allocated(), 1u);
    EXPECT_EQ(pool.logicalBricks(), 2u);
    EXPECT_EQ(pool.refCount(a), 2u);
}

TEST(BrickPool, DedupOnAllocatesNewWhenContentDiffers) {
    BrickPool pool;

    Brick a{};
    a.voxels[0] = 0xAAAAAAAAu;
    Brick b{};
    b.voxels[0] = 0xBBBBBBBBu;

    const BrickHandle ha = pool.acquireBrick(a);
    const BrickHandle hb = pool.acquireBrick(b);
    EXPECT_NE(ha, hb);
    EXPECT_EQ(pool.allocated(), 2u);
}

TEST(BrickPool, ReleaseDecrementsRefcountAndFreesAtZero) {
    BrickPool pool;
    Brick contents{};
    contents.voxels[0] = 0x12345678u;

    const BrickHandle h = pool.acquireBrick(contents);
    pool.acquireBrick(contents);
    EXPECT_EQ(pool.refCount(h), 2u);
    EXPECT_EQ(pool.allocated(), 1u);

    pool.release(h);
    EXPECT_EQ(pool.refCount(h), 1u);
    EXPECT_EQ(pool.allocated(), 1u);

    pool.release(h);
    EXPECT_EQ(pool.allocated(), 0u);
}

TEST(BrickPool, PrepareForWriteOnExclusiveBrickDetachesFromDedup) {
    BrickPool pool;
    Brick contents{};
    contents.voxels[0] = 0xCAFEBABEu;

    const BrickHandle a = pool.acquireBrick(contents);
    ASSERT_EQ(pool.refCount(a), 1u);

    const BrickHandle writable = pool.prepareForWrite(a);
    EXPECT_EQ(writable, a);

    const BrickHandle b = pool.acquireBrick(contents);
    EXPECT_NE(a, b);
    EXPECT_EQ(pool.allocated(), 2u);
}

TEST(BrickPool, PrepareForWriteOnSharedBrickClones) {
    BrickPool pool;
    Brick contents{};
    contents.voxels[0] = 0xDEADBEEFu;

    const BrickHandle a = pool.acquireBrick(contents);
    const BrickHandle b = pool.acquireBrick(contents);
    ASSERT_EQ(a, b);
    ASSERT_EQ(pool.refCount(a), 2u);

    const BrickHandle writable = pool.prepareForWrite(a);
    EXPECT_NE(writable, a);
    EXPECT_EQ(pool.refCount(a), 1u);
    EXPECT_EQ(pool.refCount(writable), 1u);
}

TEST(BrickPool, PrepareForWriteOnEmptyHandleIsNoop) {
    BrickPool pool;
    EXPECT_EQ(pool.prepareForWrite(kEmptyBrick), kEmptyBrick);
}

TEST(BrickPool, LogicalBricksMatchesAllocatedWhenDedupOff) {
    BrickPool pool;
    pool.setDedupEnabled(false);
    Brick contents{};
    contents.voxels[0] = 0x1u;

    for (int i = 0; i < 50; ++i) {
        pool.acquireBrick(contents);
    }
    EXPECT_EQ(pool.allocated(), 50u);
    EXPECT_EQ(pool.logicalBricks(), 50u);
}

TEST(BrickPool, LogicalBricksDivergesFromAllocatedWhenDedupOn) {
    BrickPool pool;
    Brick contents{};
    contents.voxels[0] = 0x1u;

    for (int i = 0; i < 50; ++i) {
        pool.acquireBrick(contents);
    }
    EXPECT_EQ(pool.allocated(), 1u);
    EXPECT_EQ(pool.logicalBricks(), 50u);
}
