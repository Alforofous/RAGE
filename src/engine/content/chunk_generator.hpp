#pragma once

#include <memory>
#include "math/ivec.hpp"

namespace RAGE {
    class BrickPool;
    class Voxel3D;
}

namespace RAGE::Content {
    /**
     * @brief Produces one `Voxel3D` per integer chunk coord. Must be a pure function of
     *        `chunkCoord` — same coord → bit-identical content — so a chunk can be
     *        evicted and regenerated transparently. Use with a `ChunkStore` for
     *        streaming.
     */
    class ChunkGenerator {
    public:
        virtual ~ChunkGenerator() = default;

        ChunkGenerator() = default;
        ChunkGenerator(const ChunkGenerator &) = delete;
        ChunkGenerator &operator=(const ChunkGenerator &) = delete;
        ChunkGenerator(ChunkGenerator &&) = delete;
        ChunkGenerator &operator=(ChunkGenerator &&) = delete;

        /// Chunk extent in bricks; voxel extent = this × `Brick::kDim`.
        virtual IVec3 chunkBrickDims() const = 0;

        /// Construct a Voxel3D for `chunkCoord`, fill its content, and position it so
        /// chunk (cx, cy, cz) occupies world range `chunkCoord * chunkBrickDims *
        /// Brick::kDim * voxelSize`.
        virtual std::unique_ptr<Voxel3D> generateChunk(IVec3 chunkCoord, BrickPool &pool,
                                                       float voxelSize) = 0;
    };
}
