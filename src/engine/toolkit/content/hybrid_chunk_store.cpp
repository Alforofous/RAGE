#include "hybrid_chunk_store.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include "engine/scene/voxel3d.hpp"

namespace RAGE::Toolkit::Content {
    HybridChunkStore::HybridChunkStore(std::unique_ptr<ChunkStore> overlay,
                                       std::unique_ptr<ChunkStore> baseline)
        : overlay_(std::move(overlay))
        , baseline_(std::move(baseline)) {
        if (!overlay_ || !baseline_) {
            throw std::invalid_argument("HybridChunkStore: overlay and baseline are required");
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
        return baseline_->chunkAt(coord);
    }

    void HybridChunkStore::putChunk(IVec3 coord, const Voxel3D &chunk) {
        overlay_->putChunk(coord, chunk);
    }
}
