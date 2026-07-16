#include "hybrid_chunk_store.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include "engine/scene/voxel3d.hpp"
#include "shared/logger.hpp"

namespace RAGE::Toolkit::Content {
    HybridChunkStore::HybridChunkStore(std::unique_ptr<ChunkStore> overlay,
                                       std::unique_ptr<ChunkStore> baseline,
                                       WriteThrough writeThrough)
        : overlay_(std::move(overlay))
        , baseline_(std::move(baseline))
        , writeThrough_(writeThrough) {
        if (!overlay_ || !baseline_) {
            throw std::invalid_argument("HybridChunkStore: overlay and baseline are required");
        }
        if (writeThrough_ == WriteThrough::CacheBaselineReady && !overlay_->isWritable()) {
            throw std::invalid_argument(
                "HybridChunkStore: write-through requires a writable overlay");
        }
        const IVec3 a = overlay_->chunkBrickDims();
        const IVec3 b = baseline_->chunkBrickDims();
        if (a.x != b.x || a.y != b.y || a.z != b.z) {
            throw std::invalid_argument("HybridChunkStore: overlay/baseline chunkBrickDims differ");
        }
    }

    ChunkStore::YRange HybridChunkStore::yRange() const {
        const YRange o = overlay_->yRange();
        const YRange b = baseline_->yRange();
        return YRange{ .min = std::min(o.min, b.min), .max = std::max(o.max, b.max) };
    }

    ChunkResult HybridChunkStore::chunkAt(IVec3 coord) {
        ChunkResult fromOverlay = overlay_->chunkAt(coord);
        if (fromOverlay.status != ChunkStatus::Missing) {
            return fromOverlay;
        }
        ChunkResult fromBaseline = baseline_->chunkAt(coord);
        if (writeThrough_ == WriteThrough::CacheBaselineReady
            && fromBaseline.status == ChunkStatus::Ready && fromBaseline.chunk != nullptr) {
            try {
                overlay_->putChunk(coord, *fromBaseline.chunk);
            } catch (const std::exception &e) {
                // A cache miss-to-write must never break loading — the chunk is valid
                // regardless of whether persisting it worked.
                Core::log(Core::LogLevel::Error,
                          (std::string("HybridChunkStore: write-through failed: ") + e.what())
                              .c_str());
            }
        }
        return fromBaseline;
    }

    void HybridChunkStore::putChunk(IVec3 coord, const Voxel3D &chunk) {
        overlay_->putChunk(coord, chunk);
    }
}
