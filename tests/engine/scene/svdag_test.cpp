#include <gtest/gtest.h>
#include <span>
#include <vector>
#include "engine/scene/svdag.hpp"
#include "math/ivec.hpp"

using namespace RAGE;

TEST(Svdag, EmptyInputProducesEmptyTree) {
    const Svdag s = buildSvdag({}, IVec3(0, 0, 0));
    EXPECT_TRUE(s.nodes.empty());
    EXPECT_EQ(s.levels, 0);
}

TEST(Svdag, AllEmptyGridCollapsesToSingleSentinelNode) {
    const std::vector<BrickHandle> grid(static_cast<size_t>(4) * 4 * 4, kEmptyBrick);
    const Svdag s = buildSvdag(grid, IVec3(4, 4, 4));
    EXPECT_EQ(s.nodes.size(), 1u);
    EXPECT_EQ(s.rootIndex, kEmptySvdagNode);
}

TEST(Svdag, UniformBrickGridDedupsAggressively) {
    constexpr BrickHandle kHandle{ 7u };
    const std::vector<BrickHandle> grid(static_cast<size_t>(4) * 4 * 4, kHandle);
    const Svdag s = buildSvdag(grid, IVec3(4, 4, 4));
    EXPECT_EQ(s.nodes.size(), 3u);
    EXPECT_EQ(s.levels, 2);
}

TEST(Svdag, SingleBrickInOtherwiseEmptyGridDedupsAcrossLevels) {
    std::vector<BrickHandle> grid(static_cast<size_t>(8) * 8 * 8, kEmptyBrick);
    grid[0] = BrickHandle{ 1u };
    const Svdag s = buildSvdag(grid, IVec3(8, 8, 8));
    EXPECT_EQ(s.nodes.size(), 2u);
    EXPECT_EQ(s.levels, 3);
}

TEST(Svdag, NonCubicDimsArePaddedWithEmpty) {
    std::vector<BrickHandle> grid(static_cast<size_t>(8) * 4 * 8, BrickHandle{ 1u });
    const Svdag s = buildSvdag(grid, IVec3(8, 4, 8));
    EXPECT_EQ(s.paddedDim, 8);
    EXPECT_LT(s.nodes.size(), 16u);
}

TEST(Svdag, RootIndexZeroWhenEntireGridEmpty) {
    const std::vector<BrickHandle> grid(static_cast<size_t>(2) * 2 * 2, kEmptyBrick);
    const Svdag s = buildSvdag(grid, IVec3(2, 2, 2));
    EXPECT_EQ(s.rootIndex, kEmptySvdagNode);
}

TEST(Svdag, NonEmptyRootWhenGridHasContent) {
    std::vector<BrickHandle> grid(static_cast<size_t>(2) * 2 * 2, kEmptyBrick);
    grid[3] = BrickHandle{ 42u };
    const Svdag s = buildSvdag(grid, IVec3(2, 2, 2));
    EXPECT_NE(s.rootIndex, kEmptySvdagNode);
}

TEST(Svdag, MeasuresMeaningfulCompressionOnLargelyEmptyGrid) {
    constexpr int32_t kDim = 16;
    std::vector<BrickHandle> grid(static_cast<size_t>(kDim) * kDim * kDim, kEmptyBrick);
    for (int32_t z = 0; z < kDim / 2; ++z) {
        for (int32_t y = 0; y < kDim / 2; ++y) {
            for (int32_t x = 0; x < kDim / 2; ++x) {
                grid[(static_cast<size_t>(z) * kDim * kDim) + (static_cast<size_t>(y) * kDim) + x] =
                    BrickHandle{ static_cast<uint32_t>(x + (y * 7u) + (z * 13u) + 1u) };
            }
        }
    }
    const Svdag s = buildSvdag(grid, IVec3(kDim, kDim, kDim));

    const size_t flatGridBytes = grid.size() * sizeof(BrickHandle);
    const size_t svdagBytesUsed = svdagBytes(s);
    EXPECT_LT(svdagBytesUsed, flatGridBytes);
}

namespace {
    // Mirror of the shader's integer-coord descent (svdagBrickAt in voxel_raycast.comp).
    // Keeping a host-side reference implementation lets us pin the shader's traversal
    // contract via tests rather than rendered-pixel sanity checks. If the shader and
    // this function ever diverge, one of them is wrong.
    uint32_t descend(const Svdag &s, IVec3 brickCoord, int32_t &outEmptySubtreeSize) {
        outEmptySubtreeSize = 1;
        if (s.paddedDim <= 0 || s.nodes.empty()) {
            return 0u;
        }
        if (brickCoord.x < 0 || brickCoord.x >= s.paddedDim || brickCoord.y < 0
            || brickCoord.y >= s.paddedDim || brickCoord.z < 0 || brickCoord.z >= s.paddedDim) {
            return 0u;
        }
        uint32_t current = s.rootIndex;
        int32_t dim = s.paddedDim;
        IVec3 c = brickCoord;
        for (int32_t level = s.levels; level > 0; --level) {
            if (current == 0u) {
                outEmptySubtreeSize = dim;
                return 0u;
            }
            const int32_t half = dim / 2;
            uint32_t octant = 0u;
            if (c.x >= half) { octant |= 1u; c.x -= half; }
            if (c.y >= half) { octant |= 2u; c.y -= half; }
            if (c.z >= half) { octant |= 4u; c.z -= half; }
            current = s.nodes[current].children[octant];
            dim = half;
        }
        return current;
    }
}

TEST(Svdag, DescentReturnsTheRecordedBrickHandleAtLeafLevel) {
    constexpr int32_t kDim = 4;
    std::vector<BrickHandle> grid(static_cast<size_t>(kDim) * kDim * kDim, kEmptyBrick);
    grid[0] = BrickHandle{ 7u };
    grid[(kDim * kDim * kDim) - 1] = BrickHandle{ 9u };
    const Svdag s = buildSvdag(grid, IVec3(kDim, kDim, kDim));

    int32_t skip = -1;
    EXPECT_EQ(descend(s, IVec3(0, 0, 0), skip), 7u);
    EXPECT_EQ(skip, 1);
    EXPECT_EQ(descend(s, IVec3(3, 3, 3), skip), 9u);
    EXPECT_EQ(skip, 1);
}

TEST(Svdag, EmptySubtreeSkipReportsItsActualSize) {
    constexpr int32_t kDim = 8;
    std::vector<BrickHandle> grid(static_cast<size_t>(kDim) * kDim * kDim, kEmptyBrick);
    grid[0] = BrickHandle{ 1u };
    const Svdag s = buildSvdag(grid, IVec3(kDim, kDim, kDim));

    int32_t skip = 0;
    EXPECT_EQ(descend(s, IVec3(7, 7, 7), skip), 0u);
    EXPECT_EQ(skip, 4);

    EXPECT_EQ(descend(s, IVec3(1, 0, 0), skip), 0u);
    EXPECT_LT(skip, 4);
    EXPECT_GT(skip, 0);
}

TEST(Svdag, OutOfBoundsCoordReportsEmptyButSize1) {
    constexpr int32_t kDim = 4;
    std::vector<BrickHandle> grid(static_cast<size_t>(kDim) * kDim * kDim, BrickHandle{ 1u });
    const Svdag s = buildSvdag(grid, IVec3(kDim, kDim, kDim));

    int32_t skip = 0;
    EXPECT_EQ(descend(s, IVec3(-1, 0, 0), skip), 0u);
    EXPECT_EQ(skip, 1);
    EXPECT_EQ(descend(s, IVec3(s.paddedDim, 0, 0), skip), 0u);
    EXPECT_EQ(skip, 1);
}
