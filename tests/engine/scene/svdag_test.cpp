#include <gtest/gtest.h>
#include <vector>
#include "engine/scene/svdag.hpp"
#include "math/ivec.hpp"

using namespace RAGE;

TEST(Svdag, EmptyInputProducesEmptyTree) {
    const Svdag s = buildSvdag(nullptr, IVec3(0, 0, 0));
    EXPECT_TRUE(s.nodes.empty());
    EXPECT_EQ(s.levels, 0);
}

TEST(Svdag, AllEmptyGridCollapsesToSingleSentinelNode) {
    const std::vector<uint32_t> grid(static_cast<size_t>(4) * 4 * 4, kEmptyBrick);
    const Svdag s = buildSvdag(grid.data(), IVec3(4, 4, 4));
    EXPECT_EQ(s.nodes.size(), 1u);    // only the sentinel
    EXPECT_EQ(s.rootIndex, kEmptySvdagNode);
}

TEST(Svdag, UniformBrickGridDedupsAggressively) {
    constexpr uint32_t kHandle = 7u;
    const std::vector<uint32_t> grid(static_cast<size_t>(4) * 4 * 4, kHandle);
    const Svdag s = buildSvdag(grid.data(), IVec3(4, 4, 4));
    // 4³ grid → leaf-parent (2³), then root. Two distinct non-empty nodes total: the
    // leaf-parent referencing kHandle eight times, and the root referencing that node
    // eight times. Plus sentinel = 3 nodes total.
    EXPECT_EQ(s.nodes.size(), 3u);
    EXPECT_EQ(s.levels, 2);
}

TEST(Svdag, SingleBrickInOtherwiseEmptyGridDedupsAcrossLevels) {
    std::vector<uint32_t> grid(static_cast<size_t>(8) * 8 * 8, kEmptyBrick);
    grid[0] = 1u;
    const Svdag s = buildSvdag(grid.data(), IVec3(8, 8, 8));
    // 8³ grid = 3 levels. At every level the unique node has children
    // {N, 0, 0, 0, 0, 0, 0, 0} where N is the same value (brick-handle 1 at level 0;
    // dedup'd node-index 1 at every higher level). The hash matches across levels and
    // the SVDAG collapses to a single non-sentinel node — the shader distinguishes
    // brick-handle vs node-index interpretations by tracking its descent depth.
    EXPECT_EQ(s.nodes.size(), 2u);
    EXPECT_EQ(s.levels, 3);
}

TEST(Svdag, NonCubicDimsArePaddedWithEmpty) {
    std::vector<uint32_t> grid(static_cast<size_t>(8) * 4 * 8, 1u);
    const Svdag s = buildSvdag(grid.data(), IVec3(8, 4, 8));
    EXPECT_EQ(s.paddedDim, 8);
    // Padded layers (y=4..7) are empty; uniform handle = 1 fills y=0..3.
    // Three layers of dedup: leaf-parents at lvl 0, one tier above, root.
    // Expect the unique node count to be small (a few, plus sentinel).
    EXPECT_LT(s.nodes.size(), 16u);
}

TEST(Svdag, RootIndexZeroWhenEntireGridEmpty) {
    const std::vector<uint32_t> grid(static_cast<size_t>(2) * 2 * 2, kEmptyBrick);
    const Svdag s = buildSvdag(grid.data(), IVec3(2, 2, 2));
    EXPECT_EQ(s.rootIndex, kEmptySvdagNode);
}

TEST(Svdag, NonEmptyRootWhenGridHasContent) {
    std::vector<uint32_t> grid(static_cast<size_t>(2) * 2 * 2, kEmptyBrick);
    grid[3] = 42u;
    const Svdag s = buildSvdag(grid.data(), IVec3(2, 2, 2));
    EXPECT_NE(s.rootIndex, kEmptySvdagNode);
}

TEST(Svdag, MeasuresMeaningfulCompressionOnLargelyEmptyGrid) {
    // 16³ brick grid with content only in one octant — the empty octants should fully dedup.
    constexpr int32_t kDim = 16;
    std::vector<uint32_t> grid(static_cast<size_t>(kDim) * kDim * kDim, kEmptyBrick);
    for (int32_t z = 0; z < kDim / 2; ++z) {
        for (int32_t y = 0; y < kDim / 2; ++y) {
            for (int32_t x = 0; x < kDim / 2; ++x) {
                grid[(static_cast<size_t>(z) * kDim * kDim) + (static_cast<size_t>(y) * kDim) + x] =
                    static_cast<uint32_t>(x + (y * 7u) + (z * 13u) + 1u);
            }
        }
    }
    const Svdag s = buildSvdag(grid.data(), IVec3(kDim, kDim, kDim));

    const size_t flatGridBytes = grid.size() * sizeof(uint32_t);
    const size_t svdagBytesUsed = svdagBytes(s);
    EXPECT_LT(svdagBytesUsed, flatGridBytes);
}
