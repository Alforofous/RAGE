#include <gtest/gtest.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include "engine/toolkit/content/chunk_store.hpp"
#include "engine/toolkit/content/file_chunk_store.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"

using namespace RAGE;
using namespace RAGE::Toolkit::Content;

namespace {
    constexpr float kVoxelSize = 0.05f;
    constexpr IVec3 kBrickDims{ 2, 2, 2 };
    constexpr IVec3 kVoxelDims{ 16, 16, 16 };

    class FileChunkStoreTest : public ::testing::Test {
    protected:
        void SetUp() override {
            const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
            dir_ = std::filesystem::temp_directory_path()
                   / (std::string("rage_fcs_") + info->name());
            std::filesystem::remove_all(dir_);
        }

        void TearDown() override { std::filesystem::remove_all(dir_); }

        std::unique_ptr<Voxel3D> makeChunk(uint32_t color) {
            auto v = std::make_unique<Voxel3D>(pool_, kVoxelDims, kVoxelSize);
            if (color != 0u) {
                v->fill([color](IVec3) {
                    return Color::fromRGBA8(color);
                });
            }
            return v;
        }

        std::filesystem::path dir_;
        BrickPool pool_;
    };
}

TEST_F(FileChunkStoreTest, MissingWhenNoFileExists) {
    FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    const ChunkResult r = store.chunkAt(IVec3{ 0, 0, 0 });
    EXPECT_EQ(r.status, ChunkStatus::Missing);
    EXPECT_EQ(r.chunk, nullptr);
}

TEST_F(FileChunkStoreTest, IsWritable) {
    FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    EXPECT_TRUE(store.isWritable());
}

TEST_F(FileChunkStoreTest, PutThenGetRoundTripsVoxels) {
    FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    const auto original = makeChunk(0xFF336699u);
    original->setVoxel(IVec3{ 3, 5, 7 }, Color::fromRGBA8(0xFF0000FFu));
    store.putChunk(IVec3{ 1, 0, -2 }, *original);

    const ChunkResult r = store.chunkAt(IVec3{ 1, 0, -2 });
    ASSERT_EQ(r.status, ChunkStatus::Ready);
    ASSERT_NE(r.chunk, nullptr);
    EXPECT_EQ(r.chunk->dimensions(), kVoxelDims);
    EXPECT_EQ(r.chunk->voxelData()->voxel(IVec3{ 0, 0, 0 }), 0xFF336699u);
    EXPECT_EQ(r.chunk->voxelData()->voxel(IVec3{ 3, 5, 7 }), 0xFF0000FFu);
    EXPECT_EQ(r.chunk->voxelData()->voxel(IVec3{ 15, 15, 15 }), 0xFF336699u);
}

TEST_F(FileChunkStoreTest, AllEmptyChunkDecodesToEmpty) {
    FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    const auto empty = makeChunk(0u);
    store.putChunk(IVec3{ 0, 0, 0 }, *empty);

    EXPECT_TRUE(store.hasChunkFile(IVec3{ 0, 0, 0 }));
    const ChunkResult r = store.chunkAt(IVec3{ 0, 0, 0 });
    EXPECT_EQ(r.status, ChunkStatus::Empty);
    EXPECT_EQ(r.chunk, nullptr);
}

TEST_F(FileChunkStoreTest, OverwriteReplacesPriorContents) {
    FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    store.putChunk(IVec3{ 0, 0, 0 }, *makeChunk(0xFF111111u));
    store.putChunk(IVec3{ 0, 0, 0 }, *makeChunk(0xFF222222u));

    const ChunkResult r = store.chunkAt(IVec3{ 0, 0, 0 });
    ASSERT_EQ(r.status, ChunkStatus::Ready);
    EXPECT_EQ(r.chunk->voxelData()->voxel(IVec3{ 8, 8, 8 }), 0xFF222222u);
}

TEST_F(FileChunkStoreTest, PersistsAcrossStoreInstances) {
    {
        FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
        store.putChunk(IVec3{ -4, 1, 9 }, *makeChunk(0xFFABCDEFu));
    }
    FileChunkStore reopened(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    const ChunkResult r = reopened.chunkAt(IVec3{ -4, 1, 9 });
    ASSERT_EQ(r.status, ChunkStatus::Ready);
    EXPECT_EQ(r.chunk->voxelData()->voxel(IVec3{ 1, 2, 3 }), 0xFFABCDEFu);
}

TEST_F(FileChunkStoreTest, DistinctCoordsUseDistinctFiles) {
    FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    store.putChunk(IVec3{ 1, 0, 0 }, *makeChunk(0xFF111111u));
    store.putChunk(IVec3{ -1, 0, 0 }, *makeChunk(0xFF222222u));

    const ChunkResult a = store.chunkAt(IVec3{ 1, 0, 0 });
    const ChunkResult b = store.chunkAt(IVec3{ -1, 0, 0 });
    ASSERT_EQ(a.status, ChunkStatus::Ready);
    ASSERT_EQ(b.status, ChunkStatus::Ready);
    EXPECT_EQ(a.chunk->voxelData()->voxel(IVec3{ 0, 0, 0 }), 0xFF111111u);
    EXPECT_EQ(b.chunk->voxelData()->voxel(IVec3{ 0, 0, 0 }), 0xFF222222u);
}

TEST_F(FileChunkStoreTest, PutChunkWithWrongDimsThrows) {
    FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    const Voxel3D wrong(pool_, IVec3{ 8, 8, 8 }, kVoxelSize);
    EXPECT_THROW(store.putChunk(IVec3{ 0, 0, 0 }, wrong), std::invalid_argument);
}

TEST_F(FileChunkStoreTest, GarbageFileThrows) {
    FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    {
        std::ofstream out(dir_ / "chunk_0_0_0.rgc", std::ios::binary);
        out << "this is not a chunk";
    }
    EXPECT_THROW((void)store.chunkAt(IVec3{ 0, 0, 0 }), std::runtime_error);
}

TEST_F(FileChunkStoreTest, TruncatedFileThrows) {
    FileChunkStore store(dir_, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    store.putChunk(IVec3{ 0, 0, 0 }, *makeChunk(0xFF445566u));

    const std::filesystem::path path = dir_ / "chunk_0_0_0.rgc";
    const auto fullSize = std::filesystem::file_size(path);
    std::filesystem::resize_file(path, fullSize - 3);
    EXPECT_THROW((void)store.chunkAt(IVec3{ 0, 0, 0 }), std::runtime_error);
}

TEST_F(FileChunkStoreTest, CreatesDirectoryOnConstruction) {
    const std::filesystem::path nested = dir_ / "a" / "b";
    FileChunkStore store(nested, pool_, kBrickDims, kVoxelSize, { .min = 0, .max = 0 });
    EXPECT_TRUE(std::filesystem::is_directory(nested));
}
