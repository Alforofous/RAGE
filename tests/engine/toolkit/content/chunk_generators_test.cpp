#include <gtest/gtest.h>
#include <memory>
#include "engine/toolkit/content/chunk_generators.hpp"
#include "engine/scene/brick.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"
#include "engine/scene/voxel_data.hpp"

using namespace RAGE;
using namespace RAGE::Toolkit::Content;

namespace {
    constexpr float kVoxelSize = 0.05f;

    /// Walk every voxel in the chunk and return a deterministic content hash.
    uint64_t hashChunkContent(const Voxel3D &chunk) {
        uint64_t h = 1469598103934665603ull;
        const IVec3 dims = chunk.dimensions();
        for (int32_t z = 0; z < dims.z; ++z) {
            for (int32_t y = 0; y < dims.y; ++y) {
                for (int32_t x = 0; x < dims.x; ++x) {
                    const Color c = chunk.voxel(IVec3{ x, y, z });
                    const uint32_t packed = c.toRGBA8();
                    for (int i = 0; i < 4; ++i) {
                        h ^= static_cast<uint64_t>((packed >> (i * 8)) & 0xFFu);
                        h *= 1099511628211ull;
                    }
                }
            }
        }
        return h;
    }
}

TEST(TerrainChunkGenerator, ChunkBrickDimsDefaultsToFourCubed) {
    const TerrainChunkGenerator gen;
    EXPECT_EQ(gen.chunkBrickDims(), IVec3(4, 4, 4));
}

TEST(TerrainChunkGenerator, ChunkBrickDimsRespectsConstructorArgument) {
    const TerrainChunkGenerator gen(IVec3{ 2, 8, 2 });
    EXPECT_EQ(gen.chunkBrickDims(), IVec3(2, 8, 2));
}

TEST(TerrainChunkGenerator, GeneratesVoxel3DWithCorrectDimensions) {
    BrickPool pool;
    TerrainChunkGenerator gen(IVec3{ 4, 4, 4 });
    const auto chunk = gen.generateChunk(IVec3{ 0, 0, 0 }, pool, kVoxelSize);
    ASSERT_NE(chunk, nullptr);
    EXPECT_EQ(chunk->dimensions(), IVec3(32, 32, 32));  // 4 bricks × 8 voxels
}

TEST(TerrainChunkGenerator, PositionsChunkAtItsWorldCoord) {
    BrickPool pool;
    TerrainChunkGenerator gen(IVec3{ 4, 4, 4 });
    const auto chunk = gen.generateChunk(IVec3{ 2, 0, -3 }, pool, kVoxelSize);
    // Chunk (cx, cy, cz) sits at world (cx, cy, cz) * voxelExtent * voxelSize.
    // voxelExtent = 32, voxelSize = 0.05 → 1.6 per chunk axis.
    EXPECT_FLOAT_EQ(chunk->position().x, 2.0f * 32.0f * 0.05f);
    EXPECT_FLOAT_EQ(chunk->position().y, 0.0f);
    EXPECT_FLOAT_EQ(chunk->position().z, -3.0f * 32.0f * 0.05f);
}

TEST(TerrainChunkGenerator, SameCoordYieldsBitIdenticalContent) {
    BrickPool pool;
    TerrainChunkGenerator gen;
    const auto a = gen.generateChunk(IVec3{ 1, 0, 2 }, pool, kVoxelSize);
    const auto b = gen.generateChunk(IVec3{ 1, 0, 2 }, pool, kVoxelSize);
    EXPECT_EQ(hashChunkContent(*a), hashChunkContent(*b));
}

TEST(TerrainChunkGenerator, DifferentCoordsYieldDifferentContent) {
    BrickPool pool;
    TerrainChunkGenerator gen;
    const auto a = gen.generateChunk(IVec3{ 0, 0, 0 }, pool, kVoxelSize);
    const auto b = gen.generateChunk(IVec3{ 1, 0, 0 }, pool, kVoxelSize);
    EXPECT_NE(hashChunkContent(*a), hashChunkContent(*b));
}

TEST(TerrainChunkGenerator, ChunkAboveTerrainTopIsEmpty) {
    BrickPool pool;
    TerrainChunkGenerator gen;
    // Heightmap baseline = 28, max swing = ±7. Chunk at cy=10 covers world voxel
    // Y in [320, 352) — way above any terrain.
    const auto chunk = gen.generateChunk(IVec3{ 0, 10, 0 }, pool, kVoxelSize);
    const IVec3 dims = chunk->dimensions();
    for (int32_t z = 0; z < dims.z; ++z) {
        for (int32_t y = 0; y < dims.y; ++y) {
            for (int32_t x = 0; x < dims.x; ++x) {
                const Color c = chunk->voxel(IVec3{ x, y, z });
                EXPECT_EQ(c.toRGBA8(), 0u);
            }
        }
    }
}

TEST(TerrainChunkGenerator, ChunkAtTerrainLevelHasContent) {
    BrickPool pool;
    TerrainChunkGenerator gen;
    // Heightmap baseline ~28; chunk at cy=0 covers Y in [0, 32) which contains the surface.
    const auto chunk = gen.generateChunk(IVec3{ 0, 0, 0 }, pool, kVoxelSize);
    const IVec3 dims = chunk->dimensions();
    bool anyFilled = false;
    for (int32_t z = 0; z < dims.z && !anyFilled; ++z) {
        for (int32_t y = 0; y < dims.y && !anyFilled; ++y) {
            for (int32_t x = 0; x < dims.x; ++x) {
                if (chunk->voxel(IVec3{ x, y, z }).toRGBA8() != 0u) {
                    anyFilled = true;
                    break;
                }
            }
        }
    }
    EXPECT_TRUE(anyFilled);
}
