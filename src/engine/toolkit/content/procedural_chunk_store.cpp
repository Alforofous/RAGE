#include "procedural_chunk_store.hpp"

#include "engine/scene/voxel3d.hpp"

namespace RAGE::Toolkit::Content {
    ProceduralChunkStore::ProceduralChunkStore(std::unique_ptr<ChunkGenerator> generator,
                                                 BrickPool &pool, float voxelSize, YRange yRange)
        : generator_(std::move(generator))
        , pool_(pool)
        , voxelSize_(voxelSize)
        , yRange_(yRange) {}

    IVec3 ProceduralChunkStore::chunkBrickDims() const { return generator_->chunkBrickDims(); }

    ChunkResult ProceduralChunkStore::chunkAt(IVec3 coord) {
        ChunkResult result;
        result.chunk = generator_->generateChunk(coord, pool_, voxelSize_);
        result.status = result.chunk ? ChunkStatus::Ready : ChunkStatus::Empty;
        return result;
    }
}
