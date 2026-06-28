#pragma once

#include "engine/content/chunk_generator.hpp"

namespace RAGE::Content {
    /**
     * @brief Chunked sin-based heightmap terrain. Same world-space heightmap as
     *        `TerrainGenerator` but evaluated per-chunk on demand — usable from a
     *        `ProceduralChunkStore` for streaming. Default chunk is 32×32×32 voxels
     *        (4×4×4 bricks).
     */
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
