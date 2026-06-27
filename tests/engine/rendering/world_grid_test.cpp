#include <gtest/gtest.h>
#include <vector>
#include "engine/rendering/world_grid.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"

using namespace RAGE;

namespace {
    bool readBit(const std::vector<uint8_t> &bits, IVec3 dims, IVec3 cell) {
        const size_t bitIdx = (static_cast<size_t>(cell.z) * static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y))
                              + (static_cast<size_t>(cell.y) * static_cast<size_t>(dims.x)) + static_cast<size_t>(cell.x);
        return (bits[bitIdx >> 3u] & (1u << (bitIdx & 7u))) != 0;
    }
}

TEST(ComputeWorldGridLayout, EmptyInputProducesEmptyLayout) {
    const WorldGridLayout layout = computeWorldGridLayout(std::span<const CasterAabb>{}, 1.0f);
    EXPECT_EQ(layout.byteSize, 0u);
    EXPECT_EQ(layout.dims.x, 0);
    EXPECT_EQ(layout.dims.y, 0);
    EXPECT_EQ(layout.dims.z, 0);
}

TEST(ComputeWorldGridLayout, ZeroCellSizeProducesEmptyLayout) {
    const CasterAabb a{ { 0, 0, 0 }, { 1, 1, 1 } };
    const std::vector<CasterAabb> aabbs{ a };
    const WorldGridLayout layout = computeWorldGridLayout(aabbs, 0.0f);
    EXPECT_EQ(layout.byteSize, 0u);
}

TEST(ComputeWorldGridLayout, EnclosesSingleAabbWithPadding) {
    const std::vector<CasterAabb> aabbs{ { { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f } } };
    const WorldGridLayout layout = computeWorldGridLayout(aabbs, 1.0f, 1);
    EXPECT_EQ(layout.dims.x, 4);
    EXPECT_EQ(layout.dims.y, 4);
    EXPECT_EQ(layout.dims.z, 4);
    EXPECT_FLOAT_EQ(layout.origin.x, -1.0f);
}

TEST(ComputeWorldGridLayout, ZeroPaddingFitsTightlyToCellGrid) {
    const std::vector<CasterAabb> aabbs{ { { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f } } };
    const WorldGridLayout layout = computeWorldGridLayout(aabbs, 1.0f, 0);
    EXPECT_EQ(layout.dims.x, 2);
    EXPECT_FLOAT_EQ(layout.origin.x, 0.0f);
}

TEST(ComputeWorldGridLayout, UnionEnclosesMultipleAabbs) {
    const std::vector<CasterAabb> aabbs{ { { -3.0f, 0.0f, 0.0f }, { -1.0f, 1.0f, 1.0f } },
                                          { { 5.0f, 0.0f, 0.0f }, { 7.0f, 1.0f, 1.0f } } };
    const WorldGridLayout layout = computeWorldGridLayout(aabbs, 1.0f, 0);
    EXPECT_EQ(layout.dims.x, 10);
    EXPECT_FLOAT_EQ(layout.origin.x, -3.0f);
}

TEST(BuildWorldGrid, EmptyAabbsClearsBuffer) {
    const WorldGridLayout layout = computeWorldGridLayout(std::span<const CasterAabb>{}, 1.0f);
    std::vector<uint8_t> bits(0xFFu, 0xFFu);
    buildWorldGrid({}, layout, bits.data());
    for (uint8_t b : bits) {
        EXPECT_EQ(b, 0xFFu);
    }
}

TEST(BuildWorldGrid, SingleAabbMarksExpectedCells) {
    const std::vector<CasterAabb> aabbs{ { { 0.0f, 0.0f, 0.0f }, { 1.5f, 0.5f, 0.5f } } };
    const WorldGridLayout layout = computeWorldGridLayout(aabbs, 1.0f, 0);
    ASSERT_GE(layout.dims.x, 2);
    std::vector<uint8_t> bits(layout.byteSize, 0u);
    buildWorldGrid(aabbs, layout, bits.data());
    EXPECT_TRUE(readBit(bits, layout.dims, { 0, 0, 0 }));
    EXPECT_TRUE(readBit(bits, layout.dims, { 1, 0, 0 }));
}

TEST(BuildWorldGrid, NonOverlappingCellsRemainUnset) {
    const std::vector<CasterAabb> aabbs{ { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } } };
    const WorldGridLayout layout = computeWorldGridLayout(aabbs, 1.0f, 1);
    std::vector<uint8_t> bits(layout.byteSize, 0u);
    buildWorldGrid(aabbs, layout, bits.data());

    const IVec3 origin{ -1, -1, -1 };
    EXPECT_FALSE(readBit(bits, layout.dims, { 0 - origin.x - 2, 0 - origin.y, 0 - origin.z }));
}

TEST(BuildWorldGrid, OverlappingAabbsUnionCorrectly) {
    const std::vector<CasterAabb> aabbs{ { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } },
                                          { { 2.0f, 0.0f, 0.0f }, { 3.0f, 1.0f, 1.0f } } };
    const WorldGridLayout layout = computeWorldGridLayout(aabbs, 1.0f, 0);
    std::vector<uint8_t> bits(layout.byteSize, 0u);
    buildWorldGrid(aabbs, layout, bits.data());
    EXPECT_TRUE(readBit(bits, layout.dims, { 0, 0, 0 }));
    EXPECT_TRUE(readBit(bits, layout.dims, { 2, 0, 0 }));
}

TEST(BuildWorldGrid, NegativeOriginIsHandled) {
    const std::vector<CasterAabb> aabbs{ { { -2.0f, -2.0f, -2.0f }, { -1.0f, -1.0f, -1.0f } } };
    const WorldGridLayout layout = computeWorldGridLayout(aabbs, 1.0f, 0);
    std::vector<uint8_t> bits(layout.byteSize, 0u);
    buildWorldGrid(aabbs, layout, bits.data());
    EXPECT_TRUE(readBit(bits, layout.dims, { 0, 0, 0 }));
}
