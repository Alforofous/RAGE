#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include "engine/scene/voxel_occupancy_mip.hpp"
#include "math/ivec.hpp"

using namespace RAGE;

namespace {
    constexpr uint32_t kOccupied = 0xFFFFFFFFu;
    constexpr uint32_t kEmpty = 0x00000000u;

    size_t voxelIndex(IVec3 c, IVec3 dim) {
        return (static_cast<size_t>(c.z) * static_cast<size_t>(dim.x) * static_cast<size_t>(dim.y))
               + (static_cast<size_t>(c.y) * static_cast<size_t>(dim.x)) + static_cast<size_t>(c.x);
    }

    bool readMipBit(const std::vector<uint8_t> &mip, const OccupancyMipLayout &layout, uint32_t level, IVec3 cell) {
        const uint8_t *data = mip.data() + layout.levelByteOffsets[level];
        return mipReadBit(data, mipBitIndex(cell, layout.levelDims[level]));
    }
}

TEST(OccupancyMipLayout, ReturnsEmptyForUnitGrid) {
    const OccupancyMipLayout layout = computeOccupancyMipLayout({ 1, 1, 1 }, 4);
    EXPECT_TRUE(layout.levelDims.empty());
    EXPECT_TRUE(layout.levelByteOffsets.empty());
    EXPECT_EQ(layout.totalBytes, 0u);
}

TEST(OccupancyMipLayout, ReturnsEmptyForZeroMaxLevels) {
    const OccupancyMipLayout layout = computeOccupancyMipLayout({ 24, 24, 24 }, 0);
    EXPECT_TRUE(layout.levelDims.empty());
}

TEST(OccupancyMipLayout, ReturnsEmptyForInvalidDims) {
    EXPECT_TRUE(computeOccupancyMipLayout({ 0, 24, 24 }, 4).levelDims.empty());
    EXPECT_TRUE(computeOccupancyMipLayout({ 24, -1, 24 }, 4).levelDims.empty());
}

TEST(OccupancyMipLayout, BuildsExpectedLevelsForCubicGrid) {
    const OccupancyMipLayout layout = computeOccupancyMipLayout({ 24, 24, 24 }, 4);
    ASSERT_EQ(layout.levelDims.size(), 4u);
    EXPECT_EQ(layout.levelDims[0], IVec3(12, 12, 12));
    EXPECT_EQ(layout.levelDims[1], IVec3(6, 6, 6));
    EXPECT_EQ(layout.levelDims[2], IVec3(3, 3, 3));
    EXPECT_EQ(layout.levelDims[3], IVec3(2, 2, 2));
    EXPECT_EQ(layout.levelByteOffsets[0], 0u);
    EXPECT_GT(layout.totalBytes, 0u);
}

TEST(OccupancyMipLayout, StopsHalvingAxisAtOne) {
    const OccupancyMipLayout layout = computeOccupancyMipLayout({ 8, 1, 8 }, 4);
    ASSERT_FALSE(layout.levelDims.empty());
    for (const IVec3 &d : layout.levelDims) {
        EXPECT_EQ(d.y, 1);
    }
}

TEST(OccupancyMipLayout, RespectsMaxLevelsCap) {
    const OccupancyMipLayout layout = computeOccupancyMipLayout({ 64, 64, 64 }, 2);
    EXPECT_EQ(layout.levelDims.size(), 2u);
    EXPECT_EQ(layout.levelDims[0], IVec3(32, 32, 32));
    EXPECT_EQ(layout.levelDims[1], IVec3(16, 16, 16));
}

TEST(BuildOccupancyMip, EmptyVoxelsProducesAllZeroes) {
    const IVec3 dims{ 4, 4, 4 };
    const OccupancyMipLayout layout = computeOccupancyMipLayout(dims, 4);
    const std::vector<uint32_t> voxels(static_cast<size_t>(dims.x * dims.y * dims.z), kEmpty);
    std::vector<uint8_t> mip(layout.totalBytes, 0xFFu);

    buildOccupancyMip(voxels.data(), dims, layout, mip.data());
    for (uint8_t b : mip) {
        EXPECT_EQ(b, 0u);
    }
}

TEST(BuildOccupancyMip, SingleVoxelLightsUpEveryLevel) {
    const IVec3 dims{ 8, 8, 8 };
    const OccupancyMipLayout layout = computeOccupancyMipLayout(dims, 4);
    std::vector<uint32_t> voxels(static_cast<size_t>(dims.x * dims.y * dims.z), kEmpty);
    voxels[voxelIndex({ 3, 5, 7 }, dims)] = kOccupied;
    std::vector<uint8_t> mip(layout.totalBytes, 0u);

    buildOccupancyMip(voxels.data(), dims, layout, mip.data());

    ASSERT_FALSE(layout.levelDims.empty());
    EXPECT_TRUE(readMipBit(mip, layout, 0, { 1, 2, 3 }));
    EXPECT_TRUE(readMipBit(mip, layout, 1, { 0, 1, 1 }));
    if (layout.levelDims.size() > 2) {
        EXPECT_TRUE(readMipBit(mip, layout, 2, { 0, 0, 0 }));
    }
}

TEST(BuildOccupancyMip, OnlyAlphaByteCountsAsOccupied) {
    const IVec3 dims{ 4, 4, 4 };
    const OccupancyMipLayout layout = computeOccupancyMipLayout(dims, 4);
    std::vector<uint32_t> voxels(static_cast<size_t>(dims.x * dims.y * dims.z), 0x00FFFFFFu);
    std::vector<uint8_t> mip(layout.totalBytes, 0u);

    buildOccupancyMip(voxels.data(), dims, layout, mip.data());
    for (uint8_t b : mip) {
        EXPECT_EQ(b, 0u);
    }
}

TEST(BuildOccupancyMip, DenseGridLightsUpAllBits) {
    const IVec3 dims{ 4, 4, 4 };
    const OccupancyMipLayout layout = computeOccupancyMipLayout(dims, 4);
    const std::vector<uint32_t> voxels(static_cast<size_t>(dims.x * dims.y * dims.z), kOccupied);
    std::vector<uint8_t> mip(layout.totalBytes, 0u);

    buildOccupancyMip(voxels.data(), dims, layout, mip.data());

    for (size_t L = 0; L < layout.levelDims.size(); ++L) {
        const IVec3 d = layout.levelDims[L];
        for (int32_t z = 0; z < d.z; ++z) {
            for (int32_t y = 0; y < d.y; ++y) {
                for (int32_t x = 0; x < d.x; ++x) {
                    EXPECT_TRUE(readMipBit(mip, layout, static_cast<uint32_t>(L), { x, y, z }))
                        << "level=" << L << " cell=(" << x << "," << y << "," << z << ")";
                }
            }
        }
    }
}

TEST(BuildOccupancyMip, NonCubicGridProducesCorrectLevelDims) {
    const IVec3 dims{ 8, 1, 8 };
    const OccupancyMipLayout layout = computeOccupancyMipLayout(dims, 4);
    ASSERT_FALSE(layout.levelDims.empty());
    EXPECT_EQ(layout.levelDims[0], IVec3(4, 1, 4));
    EXPECT_EQ(layout.levelDims[1], IVec3(2, 1, 2));
}

TEST(BuildOccupancyMip, OverwritesPreviousMipContent) {
    const IVec3 dims{ 4, 4, 4 };
    const OccupancyMipLayout layout = computeOccupancyMipLayout(dims, 4);
    std::vector<uint8_t> mip(layout.totalBytes, 0xFFu);

    const std::vector<uint32_t> voxels(static_cast<size_t>(dims.x * dims.y * dims.z), kEmpty);
    buildOccupancyMip(voxels.data(), dims, layout, mip.data());
    for (uint8_t b : mip) {
        EXPECT_EQ(b, 0u);
    }
}

TEST(BuildOccupancyMip, BlockBoundaryVoxelsCollapseToSingleBit) {
    const IVec3 dims{ 4, 4, 4 };
    const OccupancyMipLayout layout = computeOccupancyMipLayout(dims, 4);
    std::vector<uint32_t> voxels(static_cast<size_t>(dims.x * dims.y * dims.z), kEmpty);
    // Fill all 8 voxels in the (0,0,0)..(1,1,1) cube
    for (int32_t z = 0; z < 2; ++z) {
        for (int32_t y = 0; y < 2; ++y) {
            for (int32_t x = 0; x < 2; ++x) {
                voxels[voxelIndex({ x, y, z }, dims)] = kOccupied;
            }
        }
    }
    std::vector<uint8_t> mip(layout.totalBytes, 0u);
    buildOccupancyMip(voxels.data(), dims, layout, mip.data());

    EXPECT_TRUE(readMipBit(mip, layout, 0, { 0, 0, 0 }));
    EXPECT_FALSE(readMipBit(mip, layout, 0, { 1, 0, 0 }));
    EXPECT_FALSE(readMipBit(mip, layout, 0, { 0, 1, 0 }));
    EXPECT_FALSE(readMipBit(mip, layout, 0, { 0, 0, 1 }));
    EXPECT_FALSE(readMipBit(mip, layout, 0, { 1, 1, 1 }));
}

TEST(MipBitIndex, PacksLinearlyInXYZOrder) {
    const IVec3 dim{ 4, 3, 2 };
    EXPECT_EQ(mipBitIndex({ 0, 0, 0 }, dim), 0u);
    EXPECT_EQ(mipBitIndex({ 1, 0, 0 }, dim), 1u);
    EXPECT_EQ(mipBitIndex({ 0, 1, 0 }, dim), 4u);
    EXPECT_EQ(mipBitIndex({ 0, 0, 1 }, dim), 12u);
    EXPECT_EQ(mipBitIndex({ 3, 2, 1 }, dim), 23u);
}

TEST(MipReadBit, MatchesManualBitSetting) {
    std::vector<uint8_t> data(16, 0u);
    data[0] = 0b0000'0101u;
    data[3] = 0b1000'0000u;
    EXPECT_TRUE(mipReadBit(data.data(), 0));
    EXPECT_FALSE(mipReadBit(data.data(), 1));
    EXPECT_TRUE(mipReadBit(data.data(), 2));
    EXPECT_TRUE(mipReadBit(data.data(), 31));
}
