#pragma once

#include <filesystem>
#include <mutex>
#include "engine/content/chunk_store.hpp"

namespace RAGE {
    class BrickPool;
}

namespace RAGE::Content {
    /**
     * @brief Writable `ChunkStore` persisting chunks as one file per coord under a
     *        directory (`chunk_<x>_<y>_<z>.rgc`).
     *
     * Status semantics are overlay-oriented: `Missing` strictly means "no file for this
     * coord", so a composing store (`HybridChunkStore`) can fall through to a baseline.
     * An explicitly saved all-empty chunk decodes to `Empty` and therefore *masks* the
     * baseline — that is how an edit that clears procedural terrain persists.
     *
     * File format v1 (`RGC1`, little-endian):
     *   u32 magic, u32 flags (bit0 = empty), 3×i32 voxel dims,
     *   then RLE pairs (u32 runLength, u32 packedRGBA8) in x-fastest voxel order.
     * A corrupt or truncated file throws from `chunkAt`; the streamer worker logs and
     * degrades it to `Missing`.
     *
     * Internally synchronised: `chunkAt` runs on the streamer worker thread while
     * `putChunk` runs on the main thread. Writes go to a temp file first, then rename.
     */
    class FileChunkStore : public ChunkStore {
    public:
        /// Creates `directory` (and parents) if absent. Throws on filesystem failure.
        FileChunkStore(std::filesystem::path directory, BrickPool &pool, IVec3 chunkBrickDims,
                       float voxelSize, YRange yRange);

        IVec3 chunkBrickDims() const override { return chunkBrickDims_; }
        YRange yRange() const override { return yRange_; }
        bool isWritable() const override { return true; }

        ChunkResult chunkAt(IVec3 coord) override;

        /// Serializes `chunk` for `coord`, replacing any prior file. Throws if the
        /// chunk's dimensions don't match `chunkBrickDims() * 8`.
        void putChunk(IVec3 coord, const Voxel3D &chunk) override;

        /// True if a chunk file exists for `coord` (diagnostic / test helper).
        bool hasChunkFile(IVec3 coord) const;

    private:
        std::filesystem::path pathFor_(IVec3 coord) const;

        std::filesystem::path directory_;
        BrickPool &pool_;
        IVec3 chunkBrickDims_{};
        float voxelSize_ = 0.0f;
        YRange yRange_{};
        mutable std::mutex mtx_;
    };
}
