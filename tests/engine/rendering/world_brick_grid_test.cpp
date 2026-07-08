#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>
#include "engine/rendering/world_brick_grid.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel_data.hpp"
#include "math/ivec.hpp"

using namespace RAGE;

namespace {
    constexpr uint32_t kRed = 0xFF0000FFu;
    constexpr IVec3 kGridDims{ 32, 8, 32 };
}

TEST(WorldBrickGrid, NonPowerOfTwoDimsThrow) {
    EXPECT_THROW(WorldBrickGrid(IVec3{ 30, 8, 32 }), std::invalid_argument);
    EXPECT_THROW(WorldBrickGrid(IVec3{ 32, 0, 32 }), std::invalid_argument);
    EXPECT_THROW(WorldBrickGrid(IVec3{ 32, 8, -32 }), std::invalid_argument);
}

TEST(WorldBrickGrid, StartsZeroedWithEmptyWindow) {
    const WorldBrickGrid grid(kGridDims);
    EXPECT_EQ(grid.fixedDims(), kGridDims);
    EXPECT_EQ(grid.windowExtent(), IVec3(0, 0, 0));
    EXPECT_EQ(grid.handles().size(),
              static_cast<size_t>(kGridDims.x) * kGridDims.y * kGridDims.z);
    EXPECT_EQ(grid.handleAt({ 0, 0, 0 }), kEmptyBrick);
}

TEST(WorldBrickGrid, WindowExtentExceedingFixedDimsThrows) {
    WorldBrickGrid grid(kGridDims);
    EXPECT_THROW(grid.setWindow({ 0, 0, 0 }, { 33, 8, 32 }), std::invalid_argument);
    EXPECT_THROW(grid.setWindow({ 0, 0, 0 }, { 32, 8, -1 }), std::invalid_argument);
}

TEST(WorldBrickGrid, EmptyPlacementsClearsWindow) {
    WorldBrickGrid grid(kGridDims);
    grid.rebuild({});
    EXPECT_EQ(grid.windowExtent(), IVec3(0, 0, 0));
}

TEST(WorldBrickGrid, MapsOccupiedBrickToCorrectWorldCell) {
    BrickPool pool;
    VoxelData v(pool, { 16, 8, 8 });   // 2x1x1 bricks
    v.setVoxel({ 0, 0, 0 }, kRed);     // populates local brick (0,0,0)
    v.setVoxel({ 8, 0, 0 }, kRed);     // populates local brick (1,0,0)
    const std::vector<VoxelDataWorldPlacement> placements{ { &v, { 10, 0, 0 } } };
    WorldBrickGrid grid(kGridDims);
    grid.rebuild(placements);

    EXPECT_NE(grid.handleAt({ 10, 0, 0 }), kEmptyBrick);
    EXPECT_NE(grid.handleAt({ 11, 0, 0 }), kEmptyBrick);
    EXPECT_EQ(grid.handleAt({ 12, 0, 0 }), kEmptyBrick);
    EXPECT_EQ(grid.handleAt({ -100, 0, 0 }), kEmptyBrick);
}

TEST(WorldBrickGrid, EnclosesUnionOfMultiplePlacements) {
    BrickPool pool;
    VoxelData a(pool, { 8, 8, 8 });
    VoxelData b(pool, { 8, 8, 8 });
    a.setVoxel({ 0, 0, 0 }, kRed);
    b.setVoxel({ 0, 0, 0 }, kRed);
    const std::vector<VoxelDataWorldPlacement> placements{ { &a, { 0, 0, 0 } }, { &b, { 5, 0, 0 } } };
    WorldBrickGrid grid(kGridDims);
    grid.rebuild(placements);

    EXPECT_EQ(grid.handleAt({ 0, 0, 0 }), a.handleAt({ 0, 0, 0 }));
    EXPECT_EQ(grid.handleAt({ 5, 0, 0 }), b.handleAt({ 0, 0, 0 }));
    EXPECT_GE(grid.windowExtent().x, 6);
}

TEST(WorldBrickGrid, RebuildIsIdempotent) {
    BrickPool pool;
    VoxelData v(pool, { 8, 8, 8 });
    v.setVoxel({ 0, 0, 0 }, kRed);
    const std::vector<VoxelDataWorldPlacement> placements{ { &v, { 3, 4, 5 } } };
    WorldBrickGrid grid(kGridDims);
    grid.rebuild(placements);
    const auto firstHandles = std::vector<BrickHandle>(grid.handles().begin(), grid.handles().end());
    grid.rebuild(placements);
    const auto secondHandles = std::vector<BrickHandle>(grid.handles().begin(), grid.handles().end());
    EXPECT_EQ(firstHandles, secondHandles);
}

TEST(WorldBrickGrid, NullDataPointerInPlacementIsIgnored) {
    WorldBrickGrid grid(kGridDims);
    const std::vector<VoxelDataWorldPlacement> placements{ { nullptr, { 0, 0, 0 } } };
    grid.rebuild(placements);
    EXPECT_EQ(grid.windowExtent(), IVec3(0, 0, 0));
}

TEST(WorldBrickGrid, WriteChunkAtNegativeCoordsWrapsIntoStorage) {
    BrickPool pool;
    VoxelData v(pool, { 8, 8, 8 });
    v.setVoxel({ 0, 0, 0 }, kRed);

    WorldBrickGrid grid(kGridDims);
    grid.setWindow({ -5, 0, -5 }, { 10, 1, 10 });
    grid.writeChunk({ -5, 0, -5 }, v);

    EXPECT_EQ(grid.handleAt({ -5, 0, -5 }), v.handleAt({ 0, 0, 0 }));
    EXPECT_EQ(grid.handleAt({ -6, 0, -5 }), kEmptyBrick);
}

TEST(WorldBrickGrid, CellsOutsideWindowReadEmptyEvenIfSlotIsWritten) {
    BrickPool pool;
    VoxelData v(pool, { 8, 8, 8 });
    v.setVoxel({ 0, 0, 0 }, kRed);

    WorldBrickGrid grid(kGridDims);
    grid.setWindow({ 0, 0, 0 }, { 4, 1, 4 });
    grid.writeChunk({ 0, 0, 0 }, v);

    // Same wrapped slot, one full storage period away — outside the window.
    EXPECT_EQ(grid.handleAt({ kGridDims.x, 0, 0 }), kEmptyBrick);
    EXPECT_EQ(grid.handleAt({ 0, 0, 0 }), v.handleAt({ 0, 0, 0 }));
}

TEST(WorldBrickGrid, SlidingWindowReusesSlotsAfterClear) {
    BrickPool pool;
    VoxelData a(pool, { 8, 8, 8 });
    VoxelData b(pool, { 8, 8, 8 });
    a.setVoxel({ 0, 0, 0 }, kRed);
    b.setVoxel({ 0, 0, 0 }, kRed);

    WorldBrickGrid grid(kGridDims);
    grid.setWindow({ 0, 0, 0 }, { 4, 1, 4 });
    grid.writeChunk({ 0, 0, 0 }, a);
    ASSERT_EQ(grid.handleAt({ 0, 0, 0 }), a.handleAt({ 0, 0, 0 }));

    // The streamer's contract: evict clears before the window moves on.
    grid.clearChunk({ 0, 0, 0 }, a.brickDims());
    grid.setWindow({ kGridDims.x, 0, 0 }, { 4, 1, 4 });
    // World coord one full period away maps to the same slot — now legitimately b's.
    grid.writeChunk({ kGridDims.x, 0, 0 }, b);

    EXPECT_EQ(grid.handleAt({ kGridDims.x, 0, 0 }), b.handleAt({ 0, 0, 0 }));
    EXPECT_EQ(grid.handleAt({ 0, 0, 0 }), kEmptyBrick);
}

TEST(WorldBrickGrid, WriteChunkClearsHolesInsideChunkBox) {
    BrickPool pool;
    VoxelData full(pool, { 8, 8, 8 });
    VoxelData empty(pool, { 8, 8, 8 });
    full.setVoxel({ 0, 0, 0 }, kRed);

    WorldBrickGrid grid(kGridDims);
    grid.setWindow({ 0, 0, 0 }, { 4, 1, 4 });
    grid.writeChunk({ 1, 0, 1 }, full);
    ASSERT_NE(grid.handleAt({ 1, 0, 1 }), kEmptyBrick);

    grid.writeChunk({ 1, 0, 1 }, empty);
    EXPECT_EQ(grid.handleAt({ 1, 0, 1 }), kEmptyBrick);
}
