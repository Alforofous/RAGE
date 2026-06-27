#include <gtest/gtest.h>
#include <vector>
#include "engine/rendering/world_brick_grid.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel_data.hpp"
#include "math/ivec.hpp"

using namespace RAGE;

namespace {
    constexpr uint32_t kRed = 0xFF0000FFu;
}

TEST(WorldBrickGrid, EmptyPlacementsClearsGrid) {
    WorldBrickGrid grid;
    grid.rebuild({});
    EXPECT_EQ(grid.dims(), IVec3(0, 0, 0));
    EXPECT_TRUE(grid.handles().empty());
}

TEST(WorldBrickGrid, SingleEmptyVoxelDataProducesGridWithNoOccupiedCells) {
    BrickPool pool;
    VoxelData v(pool, { 16, 16, 16 });   // 2x2x2 bricks, all empty
    const std::vector<VoxelDataWorldPlacement> placements{ { &v, { 5, 0, 5 } } };
    WorldBrickGrid grid;
    grid.rebuild(placements);
    EXPECT_GT(grid.handles().size(), 0u);
    for (BrickHandle h : grid.handles()) {
        EXPECT_EQ(h, kEmptyBrick);
    }
}

TEST(WorldBrickGrid, MapsOccupiedBrickToCorrectWorldCell) {
    BrickPool pool;
    VoxelData v(pool, { 16, 8, 8 });   // 2x1x1 bricks
    v.setVoxel({ 0, 0, 0 }, kRed);     // populates local brick (0,0,0)
    v.setVoxel({ 8, 0, 0 }, kRed);     // populates local brick (1,0,0)
    const std::vector<VoxelDataWorldPlacement> placements{ { &v, { 10, 0, 0 } } };
    WorldBrickGrid grid;
    grid.rebuild(placements);

    EXPECT_NE(grid.handleAt({ 10, 0, 0 }), kEmptyBrick);
    EXPECT_NE(grid.handleAt({ 11, 0, 0 }), kEmptyBrick);
    EXPECT_EQ(grid.handleAt({ 12, 0, 0 }), kEmptyBrick);
    EXPECT_EQ(grid.handleAt({ -100, 0, 0 }), kEmptyBrick);
}

TEST(WorldBrickGrid, EnclosesUnionOfMultiplePlacementsWithMargin) {
    BrickPool pool;
    VoxelData a(pool, { 8, 8, 8 });
    VoxelData b(pool, { 8, 8, 8 });
    a.setVoxel({ 0, 0, 0 }, kRed);
    b.setVoxel({ 0, 0, 0 }, kRed);
    const std::vector<VoxelDataWorldPlacement> placements{ { &a, { 0, 0, 0 } }, { &b, { 5, 0, 0 } } };
    WorldBrickGrid grid;
    grid.rebuild(placements);

    EXPECT_NE(grid.handleAt({ 0, 0, 0 }), kEmptyBrick);
    EXPECT_NE(grid.handleAt({ 5, 0, 0 }), kEmptyBrick);
    EXPECT_EQ(grid.handleAt({ 0, 0, 0 }), a.handleAt({ 0, 0, 0 }));
    EXPECT_EQ(grid.handleAt({ 5, 0, 0 }), b.handleAt({ 0, 0, 0 }));
    EXPECT_GE(grid.dims().x, 6);     // [0..5] + margins
}

TEST(WorldBrickGrid, RebuildIsIdempotent) {
    BrickPool pool;
    VoxelData v(pool, { 8, 8, 8 });
    v.setVoxel({ 0, 0, 0 }, kRed);
    const std::vector<VoxelDataWorldPlacement> placements{ { &v, { 3, 4, 5 } } };
    WorldBrickGrid grid;
    grid.rebuild(placements);
    const auto firstHandles = std::vector<BrickHandle>(grid.handles().begin(), grid.handles().end());
    grid.rebuild(placements);
    const auto secondHandles = std::vector<BrickHandle>(grid.handles().begin(), grid.handles().end());
    EXPECT_EQ(firstHandles, secondHandles);
    EXPECT_EQ(grid.dims(), grid.dims());
}

TEST(WorldBrickGrid, NullDataPointerInPlacementIsIgnored) {
    WorldBrickGrid grid;
    const std::vector<VoxelDataWorldPlacement> placements{ { nullptr, { 0, 0, 0 } } };
    grid.rebuild(placements);
    EXPECT_EQ(grid.dims(), IVec3(0, 0, 0));
}
