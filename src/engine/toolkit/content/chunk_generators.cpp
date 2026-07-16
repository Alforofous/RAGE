#include "chunk_generators.hpp"

#include <algorithm>
#include <cmath>
#include <vector>
#include "engine/scene/brick.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"

namespace RAGE::Toolkit::Content {
    namespace {
        constexpr uint32_t kStone = 0xFF707070u;
        constexpr uint32_t kDirt  = 0xFF354E6Eu;
        constexpr uint32_t kGrass = 0xFF3D8A4Au;

        int32_t heightAt(int32_t worldVoxelX, int32_t worldVoxelZ) {
            const float fx = static_cast<float>(worldVoxelX);
            const float fz = static_cast<float>(worldVoxelZ);
            const float h = (std::sin(fx * 0.10f) * 4.0f) + (std::sin(fz * 0.08f) * 3.0f) + 28.0f;
            return std::max(static_cast<int32_t>(h), 0);
        }
    }

    std::unique_ptr<Voxel3D> TerrainChunkGenerator::generateChunk(IVec3 chunkCoord, BrickPool &pool,
                                                                    float voxelSize) {
        const IVec3 voxelDims{ chunkBrickDims_.x * Brick::kDim, chunkBrickDims_.y * Brick::kDim,
                                chunkBrickDims_.z * Brick::kDim };
        const IVec3 voxelOrigin{ chunkCoord.x * voxelDims.x, chunkCoord.y * voxelDims.y,
                                  chunkCoord.z * voxelDims.z };

        std::vector<uint32_t> src(static_cast<size_t>(voxelDims.x) * voxelDims.y * voxelDims.z, 0u);

        for (int32_t lz = 0; lz < voxelDims.z; ++lz) {
            for (int32_t lx = 0; lx < voxelDims.x; ++lx) {
                const int32_t height = heightAt(voxelOrigin.x + lx, voxelOrigin.z + lz);
                for (int32_t ly = 0; ly < voxelDims.y; ++ly) {
                    const int32_t worldY = voxelOrigin.y + ly;
                    if (worldY > height) {
                        break;
                    }
                    const int32_t depth = height - worldY;
                    uint32_t color = kStone;
                    if (depth == 0) {
                        color = kGrass;
                    } else if (depth <= 3) {
                        color = kDirt;
                    }
                    const size_t idx = (static_cast<size_t>(lz) * voxelDims.y * voxelDims.x)
                                        + (static_cast<size_t>(ly) * voxelDims.x)
                                        + static_cast<size_t>(lx);
                    src[idx] = color;
                }
            }
        }

        auto chunk = std::make_unique<Voxel3D>(pool, voxelDims, voxelSize);
        chunk->fillFromPackedRGBA8(src.data(), voxelDims);
        const Vec3 worldPos{ static_cast<float>(voxelOrigin.x) * voxelSize,
                             static_cast<float>(voxelOrigin.y) * voxelSize,
                             static_cast<float>(voxelOrigin.z) * voxelSize };
        chunk->setPosition(worldPos);
        return chunk;
    }
}
