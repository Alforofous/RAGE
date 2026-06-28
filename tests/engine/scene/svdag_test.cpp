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
    std::vector<uint32_t> grid(static_cast<size_t>(kDim) * kDim * kDim, kEmptyBrick);
    grid[0] = 7u;                     // (0,0,0) → brick handle 7
    grid[(kDim * kDim * kDim) - 1] = 9u; // (3,3,3) → brick handle 9
    const Svdag s = buildSvdag(grid.data(), IVec3(kDim, kDim, kDim));

    int32_t skip = -1;
    EXPECT_EQ(descend(s, IVec3(0, 0, 0), skip), 7u);
    EXPECT_EQ(skip, 1);
    EXPECT_EQ(descend(s, IVec3(3, 3, 3), skip), 9u);
    EXPECT_EQ(skip, 1);
}

TEST(Svdag, EmptySubtreeSkipReportsItsActualSize) {
    // 8³ grid with a single brick in one corner: descending into any of the other
    // octants should report an empty subtree spanning the full 4 cells of that octant,
    // and descending into smaller empty cells inside the populated octant should
    // report smaller subtree sizes — the contract the outer DDA relies on for its
    // multi-cell jump.
    constexpr int32_t kDim = 8;
    std::vector<uint32_t> grid(static_cast<size_t>(kDim) * kDim * kDim, kEmptyBrick);
    grid[0] = 1u;
    const Svdag s = buildSvdag(grid.data(), IVec3(kDim, kDim, kDim));

    int32_t skip = 0;
    // Far corner — entirely in the empty top-level octant.
    EXPECT_EQ(descend(s, IVec3(7, 7, 7), skip), 0u);
    EXPECT_EQ(skip, 4);   // half of paddedDim — the top-level empty octant.

    // Inside the populated octant but past the brick — empty at a smaller subtree.
    EXPECT_EQ(descend(s, IVec3(1, 0, 0), skip), 0u);
    EXPECT_LT(skip, 4);
    EXPECT_GT(skip, 0);
}

TEST(Svdag, OutOfBoundsCoordReportsEmptyButSize1) {
    constexpr int32_t kDim = 4;
    std::vector<uint32_t> grid(static_cast<size_t>(kDim) * kDim * kDim, 1u);
    const Svdag s = buildSvdag(grid.data(), IVec3(kDim, kDim, kDim));

    int32_t skip = 0;
    EXPECT_EQ(descend(s, IVec3(-1, 0, 0), skip), 0u);
    EXPECT_EQ(skip, 1);
    EXPECT_EQ(descend(s, IVec3(s.paddedDim, 0, 0), skip), 0u);
    EXPECT_EQ(skip, 1);
}
