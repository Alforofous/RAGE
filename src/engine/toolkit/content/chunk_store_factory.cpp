#include "chunk_store_factory.hpp"

#include <utility>
#include "chunk_generator.hpp"
#include "file_chunk_store.hpp"
#include "hybrid_chunk_store.hpp"
#include "procedural_chunk_store.hpp"

namespace RAGE::Toolkit::Content {
    std::unique_ptr<ChunkStore> chunkStore(std::unique_ptr<ChunkGenerator> generator,
                                           BrickPool &pool, float voxelSize,
                                           ChunkStore::YRange yRange,
                                           const std::filesystem::path &worldDir) {
        const IVec3 chunkBrickDims = generator->chunkBrickDims();
        auto procedural = std::make_unique<ProceduralChunkStore>(std::move(generator), pool,
                                                                 voxelSize, yRange);
        if (worldDir.empty()) {
            return procedural;
        }
        auto fileStore = std::make_unique<FileChunkStore>(worldDir, pool, chunkBrickDims,
                                                          voxelSize, yRange);
        return std::make_unique<HybridChunkStore>(std::move(fileStore), std::move(procedural),
                                                  WriteThrough::CacheBaselineReady);
    }
}
