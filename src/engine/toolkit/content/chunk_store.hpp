#pragma once

#include <cassert>
#include <memory>
#include "math/ivec.hpp"

namespace RAGE {
    class Voxel3D;
}

namespace RAGE::Toolkit::Content {
    /**
     * @brief Source of `Voxel3D` chunks by integer coord. Implementations must be internally
     *        synchronised — `chunkAt` runs on the loader thread, `putChunk` on the main thread.
     */
    enum class ChunkStatus { Ready, Pending, Empty, Missing };

    struct ChunkResult {
        ChunkStatus status = ChunkStatus::Missing;
        std::unique_ptr<Voxel3D> chunk;
    };

    class ChunkStore {
    public:
        struct YRange {
            int32_t min = 0;
            int32_t max = 0;
        };

        virtual ~ChunkStore() = default;

        ChunkStore() = default;
        ChunkStore(const ChunkStore &) = delete;
        ChunkStore &operator=(const ChunkStore &) = delete;
        ChunkStore(ChunkStore &&) = delete;
        ChunkStore &operator=(ChunkStore &&) = delete;

        virtual ChunkResult chunkAt(IVec3 coord) = 0;

        virtual void putChunk(IVec3 coord, const Voxel3D &chunk) {
            (void)coord;
            (void)chunk;
            assert(!"putChunk called on a non-writable ChunkStore");
        }

        virtual bool isWritable() const { return false; }
        virtual IVec3 chunkBrickDims() const = 0;
        virtual YRange yRange() const = 0;
    };
}
