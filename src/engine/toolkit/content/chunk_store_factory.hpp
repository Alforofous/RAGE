#pragma once

#include <filesystem>
#include <memory>
#include "chunk_store.hpp"

namespace RAGE {
    class BrickPool;
}

namespace RAGE::Toolkit::Content {
    class ChunkGenerator;

    /**
     * @brief The one way applications assemble a chunk store. Composes the
     *        internal store classes so call sites never choose between them:
     *
     *        - `worldDir` empty  → generate-only (nothing persists)
     *        - `worldDir` set    → disk-first with generator fallback; every
     *          generated chunk is written through so revisits and restarts
     *          read from disk (WriteThrough::CacheBaselineReady)
     *
     * Chunk dimensions are taken from the generator; the file store validates
     * against them on load.
     */
    std::unique_ptr<ChunkStore> chunkStore(std::unique_ptr<ChunkGenerator> generator,
                                           BrickPool &pool, float voxelSize,
                                           ChunkStore::YRange yRange,
                                           const std::filesystem::path &worldDir = {});
}
