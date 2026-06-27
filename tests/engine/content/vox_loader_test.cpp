#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>
#include "engine/content/vox_loader.hpp"
#include "math/ivec.hpp"

using namespace RAGE;
using namespace RAGE::Content;

namespace {
    constexpr uint32_t kMagicVox = 0x20584F56u;
    constexpr uint32_t kChunkMain = 0x4E49414Du;
    constexpr uint32_t kChunkSize = 0x455A4953u;
    constexpr uint32_t kChunkXyzi = 0x495A5958u;
    constexpr uint32_t kChunkRgba = 0x41424752u;

    void writeU32(std::vector<uint8_t> &buf, uint32_t v) {
        const uint8_t bytes[4] = {
            static_cast<uint8_t>(v & 0xFFu),
            static_cast<uint8_t>((v >> 8u) & 0xFFu),
            static_cast<uint8_t>((v >> 16u) & 0xFFu),
            static_cast<uint8_t>((v >> 24u) & 0xFFu),
        };
        buf.insert(buf.end(), bytes, bytes + 4);
    }

    void writeI32(std::vector<uint8_t> &buf, int32_t v) {
        writeU32(buf, static_cast<uint32_t>(v));
    }

    void writeBytes(std::vector<uint8_t> &buf, const uint8_t *data, size_t n) {
        buf.insert(buf.end(), data, data + n);
    }

    struct Voxel {
        uint8_t x;
        uint8_t y;
        uint8_t z;
        uint8_t colorIdx;
    };

    struct VoxFileBuilder {
        IVec3 dims{ 1, 1, 1 };
        std::vector<Voxel> voxels;
        std::vector<uint32_t> palette; // empty for no RGBA chunk
        uint32_t version = 150;
    };

    std::vector<uint8_t> buildChildren(const VoxFileBuilder &b) {
        std::vector<uint8_t> children;

        // SIZE chunk
        writeU32(children, kChunkSize);
        writeU32(children, 12);
        writeU32(children, 0);
        writeI32(children, b.dims.x);
        writeI32(children, b.dims.y);
        writeI32(children, b.dims.z);

        // XYZI chunk
        writeU32(children, kChunkXyzi);
        writeU32(children, 4u + (static_cast<uint32_t>(b.voxels.size()) * 4u));
        writeU32(children, 0);
        writeU32(children, static_cast<uint32_t>(b.voxels.size()));
        for (const Voxel &v : b.voxels) {
            children.push_back(v.x);
            children.push_back(v.y);
            children.push_back(v.z);
            children.push_back(v.colorIdx);
        }

        // optional RGBA chunk
        if (!b.palette.empty()) {
            writeU32(children, kChunkRgba);
            writeU32(children, 1024);
            writeU32(children, 0);
            std::array<uint32_t, 256> p{};
            for (size_t i = 0; i < b.palette.size() && i < 256; ++i) {
                p[i] = b.palette[i];
            }
            writeBytes(children, reinterpret_cast<const uint8_t *>(p.data()), 1024);
        }

        return children;
    }

    std::vector<uint8_t> buildVox(const VoxFileBuilder &b) {
        std::vector<uint8_t> file;
        writeU32(file, kMagicVox);
        writeU32(file, b.version);

        const std::vector<uint8_t> children = buildChildren(b);

        writeU32(file, kChunkMain);
        writeU32(file, 0);
        writeU32(file, static_cast<uint32_t>(children.size()));
        writeBytes(file, children.data(), children.size());

        return file;
    }

    std::filesystem::path writeTempVox(const std::vector<uint8_t> &bytes, const std::string &name) {
        const std::filesystem::path tmp = std::filesystem::temp_directory_path() / name;
        std::ofstream out(tmp, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return tmp;
    }
}

TEST(VoxLoader, LoadsMinimalSingleVoxelWithoutPalette) {
    VoxFileBuilder b;
    b.dims = { 1, 1, 1 };
    b.voxels = { Voxel{ .x = 0, .y = 0, .z = 0, .colorIdx = 1 } };
    const auto path = writeTempVox(buildVox(b), "rage_vox_test_single.vox");

    const VoxModel m = loadVox(path);
    EXPECT_EQ(m.dims, IVec3(1, 1, 1));
    ASSERT_EQ(m.voxels.size(), 1u);
    EXPECT_EQ(m.voxels[0], 0xFFFFFFFFu);
    std::filesystem::remove(path);
}

TEST(VoxLoader, AppliesRGBAPaletteWithOneBasedRotation) {
    VoxFileBuilder b;
    b.dims = { 2, 1, 1 };
    b.voxels = { Voxel{ .x = 0, .y = 0, .z = 0, .colorIdx = 1 },
                 Voxel{ .x = 1, .y = 0, .z = 0, .colorIdx = 2 } };
    b.palette.resize(256, 0xFF000000u);
    b.palette[0] = 0xFF0000FFu; // red (R=0xFF, A=0xFF)
    b.palette[1] = 0xFF00FF00u; // green
    const auto path = writeTempVox(buildVox(b), "rage_vox_test_palette.vox");

    const VoxModel m = loadVox(path);
    ASSERT_EQ(m.voxels.size(), 2u);
    EXPECT_EQ(m.voxels[0], 0xFF0000FFu);
    EXPECT_EQ(m.voxels[1], 0xFF00FF00u);
    std::filesystem::remove(path);
}

TEST(VoxLoader, LinearIndexingMatchesVoxel3DConvention) {
    VoxFileBuilder b;
    b.dims = { 4, 3, 2 };
    b.voxels = { Voxel{ .x = 1, .y = 2, .z = 1, .colorIdx = 1 } };
    const auto path = writeTempVox(buildVox(b), "rage_vox_test_indexing.vox");

    const VoxModel m = loadVox(path);
    const size_t idx = (1u * 4u * 3u) + (2u * 4u) + 1u;
    ASSERT_LT(idx, m.voxels.size());
    EXPECT_GT((m.voxels[idx] >> 24u) & 0xFFu, 0u);
    for (size_t i = 0; i < m.voxels.size(); ++i) {
        if (i != idx) {
            EXPECT_EQ((m.voxels[i] >> 24u) & 0xFFu, 0u);
        }
    }
    std::filesystem::remove(path);
}

TEST(VoxLoader, EmptyCellsAreTransparent) {
    VoxFileBuilder b;
    b.dims = { 2, 2, 2 };
    b.voxels = { Voxel{ .x = 0, .y = 0, .z = 0, .colorIdx = 1 } };
    const auto path = writeTempVox(buildVox(b), "rage_vox_test_empty.vox");

    const VoxModel m = loadVox(path);
    EXPECT_EQ(m.voxels.size(), 8u);
    int filled = 0;
    for (uint32_t v : m.voxels) {
        if (((v >> 24u) & 0xFFu) > 0u) {
            ++filled;
        }
    }
    EXPECT_EQ(filled, 1);
    std::filesystem::remove(path);
}

TEST(VoxLoader, ThrowsOnMissingFile) {
    EXPECT_THROW(loadVox("/this/path/should/not/exist.vox"), std::runtime_error);
}

TEST(VoxLoader, ThrowsOnBadMagic) {
    std::vector<uint8_t> bytes(20, 0u);
    writeU32(bytes, 0xDEADBEEF);
    const auto path = writeTempVox(bytes, "rage_vox_test_bad_magic.vox");
    EXPECT_THROW(loadVox(path), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(VoxLoader, ThrowsOnUnsupportedVersion) {
    VoxFileBuilder b;
    b.version = 100;
    const auto path = writeTempVox(buildVox(b), "rage_vox_test_old_version.vox");
    EXPECT_THROW(loadVox(path), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(VoxLoader, ThrowsOnMissingSizeChunk) {
    std::vector<uint8_t> file;
    writeU32(file, kMagicVox);
    writeU32(file, 150);
    std::vector<uint8_t> children;
    // include only an XYZI chunk, no SIZE
    writeU32(children, kChunkXyzi);
    writeU32(children, 4);
    writeU32(children, 0);
    writeU32(children, 0);
    writeU32(file, kChunkMain);
    writeU32(file, 0);
    writeU32(file, static_cast<uint32_t>(children.size()));
    writeBytes(file, children.data(), children.size());
    const auto path = writeTempVox(file, "rage_vox_test_no_size.vox");
    EXPECT_THROW(loadVox(path), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(VoxLoader, ThrowsOnNonPositiveDimensions) {
    VoxFileBuilder b;
    b.dims = { 0, 1, 1 };
    const auto path = writeTempVox(buildVox(b), "rage_vox_test_zero_dim.vox");
    EXPECT_THROW(loadVox(path), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(VoxLoader, SkipsOutOfBoundsVoxels) {
    VoxFileBuilder b;
    b.dims = { 2, 2, 2 };
    b.voxels = { Voxel{ .x = 0, .y = 0, .z = 0, .colorIdx = 1 },
                 Voxel{ .x = 5, .y = 5, .z = 5, .colorIdx = 1 } };
    const auto path = writeTempVox(buildVox(b), "rage_vox_test_oob.vox");
    const VoxModel m = loadVox(path);
    int filled = 0;
    for (uint32_t v : m.voxels) {
        if (((v >> 24u) & 0xFFu) > 0u) {
            ++filled;
        }
    }
    EXPECT_EQ(filled, 1);
    std::filesystem::remove(path);
}
