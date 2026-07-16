#pragma once

#include <memory>
#include "engine/toolkit/content/chunk_store.hpp"

namespace RAGE::Toolkit::Content {
    /**
     * @brief Composes a writable overlay store over a read-only baseline. `chunkAt`
     *        consults the overlay first; only a `Missing` result falls through to the
     *        baseline, so an overlay chunk saved as all-empty (`Empty`) still masks
     *        baseline content. Writes always target the overlay.
     *
     * Both stores must agree on `chunkBrickDims`; the vertical range is the union of
     * the two, so overlay edits above or below the baseline terrain still stream in.
     */
    class HybridChunkStore : public ChunkStore {
    public:
        /// Throws `std::invalid_argument` if either store is null or brick dims differ.
        HybridChunkStore(std::unique_ptr<ChunkStore> overlay, std::unique_ptr<ChunkStore> baseline);

        IVec3 chunkBrickDims() const override { return baseline_->chunkBrickDims(); }
        YRange yRange() const override;
        bool isWritable() const override { return overlay_->isWritable(); }

        ChunkResult chunkAt(IVec3 coord) override;
        void putChunk(IVec3 coord, const Voxel3D &chunk) override;

    private:
        std::unique_ptr<ChunkStore> overlay_;
        std::unique_ptr<ChunkStore> baseline_;
    };
}
