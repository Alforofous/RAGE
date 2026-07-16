#pragma once

#include <memory>
#include "engine/toolkit/content/chunk_store.hpp"

namespace RAGE::Toolkit::Content {
    /// Write-through policy for `HybridChunkStore`: whether baseline results are
    /// cached into the overlay so later loads (and future sessions) read from it.
    enum class WriteThrough {
        Disabled,
        /// Cache baseline `Ready` chunks into the overlay on the loader thread.
        /// `Empty` results are NOT cached (a file per empty chunk is spam; region
        /// files are the future answer). Cache failures log and never break loading.
        CacheBaselineReady,
    };

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
        /// Throws `std::invalid_argument` if either store is null or brick dims differ,
        /// or if write-through is requested with a non-writable overlay.
        HybridChunkStore(std::unique_ptr<ChunkStore> overlay, std::unique_ptr<ChunkStore> baseline,
                         WriteThrough writeThrough = WriteThrough::Disabled);

        IVec3 chunkBrickDims() const override { return baseline_->chunkBrickDims(); }
        YRange yRange() const override;
        bool isWritable() const override { return overlay_->isWritable(); }

        ChunkResult chunkAt(IVec3 coord) override;
        void putChunk(IVec3 coord, const Voxel3D &chunk) override;

    private:
        std::unique_ptr<ChunkStore> overlay_;
        std::unique_ptr<ChunkStore> baseline_;
        WriteThrough writeThrough_ = WriteThrough::Disabled;
    };
}
