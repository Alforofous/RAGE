#pragma once

#include <memory>
#include "math/ivec.hpp"

namespace RAGE {
    class BrickPool;
    class Voxel3D;
}

namespace RAGE::Content {
    /** @brief Pure function from chunk coord to `Voxel3D`. Same coord → identical content. */
    class ChunkGenerator {
    public:
        virtual ~ChunkGenerator() = default;

        ChunkGenerator() = default;
        ChunkGenerator(const ChunkGenerator &) = delete;
        ChunkGenerator &operator=(const ChunkGenerator &) = delete;
        ChunkGenerator(ChunkGenerator &&) = delete;
        ChunkGenerator &operator=(ChunkGenerator &&) = delete;

        virtual IVec3 chunkBrickDims() const = 0;
        virtual std::unique_ptr<Voxel3D> generateChunk(IVec3 chunkCoord, BrickPool &pool,
                                                       float voxelSize) = 0;
    };
}
