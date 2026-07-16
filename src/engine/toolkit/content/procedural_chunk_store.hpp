#pragma once

#include <memory>
#include "engine/toolkit/content/chunk_generator.hpp"
#include "engine/toolkit/content/chunk_store.hpp"

namespace RAGE {
    class BrickPool;
}

namespace RAGE::Toolkit::Content {
    /** @brief `ChunkStore` that synthesizes chunks on demand via a `ChunkGenerator`. */
    class ProceduralChunkStore : public ChunkStore {
    public:
        ProceduralChunkStore(std::unique_ptr<ChunkGenerator> generator, BrickPool &pool,
                             float voxelSize, YRange yRange);

        IVec3 chunkBrickDims() const override;
        YRange yRange() const override { return yRange_; }
        ChunkResult chunkAt(IVec3 coord) override;

    private:
        std::unique_ptr<ChunkGenerator> generator_;
        BrickPool &pool_;
        float voxelSize_;
        YRange yRange_;
    };
}
