#pragma once

#include "engine/toolkit/content/chunk_generator.hpp"

namespace RAGE::Toolkit::Content {
    /** @brief Sin-based heightmap terrain, evaluated per-chunk on demand. */
    class TerrainChunkGenerator : public ChunkGenerator {
    public:
        explicit TerrainChunkGenerator(IVec3 chunkBrickDims = IVec3{ 4, 4, 4 })
            : chunkBrickDims_(chunkBrickDims) {}

        IVec3 chunkBrickDims() const override { return chunkBrickDims_; }

        std::unique_ptr<Voxel3D> generateChunk(IVec3 chunkCoord, BrickPool &pool,
                                                float voxelSize) override;

    private:
        IVec3 chunkBrickDims_{ 4, 4, 4 };
    };
}
