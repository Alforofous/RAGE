#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "engine/content/chunk_store.hpp"
#include "math/ivec.hpp"

namespace RAGE {
    class Node3D;
    class Voxel3D;
}

namespace RAGE::Content {
    struct IVec3Hash {
        size_t operator()(const IVec3 &v) const noexcept {
            const uint64_t x = static_cast<uint32_t>(v.x);
            const uint64_t y = static_cast<uint32_t>(v.y);
            const uint64_t z = static_cast<uint32_t>(v.z);
            uint64_t h = x * 0x9E3779B97F4A7C15ull;
            h ^= y + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
            h ^= z + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
            return static_cast<size_t>(h);
        }
    };

    /**
     * @brief Maintains a cylinder of loaded chunks around a focus point. XZ extent is set by
     *        `horizontalRadius` (Euclidean), Y extent comes from `store.yRange()`. Chunk
     *        generation runs on an internal worker thread; `update()` is near-free.
     */
    class Streamer {
    public:
        using ChunkPrepareHook = std::function<void(Voxel3D &, IVec3 chunkCoord)>;

        /**
         * @brief Placement events, fired on the `update()` caller's thread. `Placed`
         *        fires after the chunk is attached to the scene tree; `Evicted` fires
         *        while the chunk is still alive, just before it is destroyed. Consumers
         *        (e.g. an incremental world grid) patch instead of re-deriving the world.
         */
        using ChunkPlacedHook = std::function<void(IVec3 chunkCoord, Voxel3D &chunk)>;
        using ChunkEvictedHook = std::function<void(IVec3 chunkCoord, Voxel3D &chunk)>;

        Streamer(ChunkStore &store, Node3D &parent);
        ~Streamer();

        Streamer(const Streamer &) = delete;
        Streamer &operator=(const Streamer &) = delete;
        Streamer(Streamer &&) = delete;
        Streamer &operator=(Streamer &&) = delete;

        void setOnChunkPrepare(ChunkPrepareHook hook) { onPrepare_ = std::move(hook); }
        void setOnChunkPlaced(ChunkPlacedHook hook) { onPlaced_ = std::move(hook); }
        void setOnChunkEvicted(ChunkEvictedHook hook) { onEvicted_ = std::move(hook); }

        void update(IVec3 focusChunk, int32_t horizontalRadius);

        /// Block until the worker finishes every pending and in-flight chunk for the given
        /// focus/radius, then drain the ready queue. Test helper — production uses `update`.
        void flushAsync(IVec3 focusChunk, int32_t horizontalRadius);

        size_t loadedCount() const { return loaded_.size(); }
        size_t skippedCount() const { return skipped_.size(); }
        size_t pendingCount() const;

        bool isLoaded(IVec3 coord) const { return loaded_.contains(coord); }
        bool isSkipped(IVec3 coord) const { return skipped_.contains(coord); }

    private:
        struct ReadyEntry {
            IVec3 coord;
            ChunkResult result;
        };

        void workerMain_();
        void evictOutOfRange_(IVec3 focusChunk, int32_t hRadius);
        void repopulatePending_(IVec3 focusChunk, int32_t hRadius);
        void applyResult_(IVec3 coord, ChunkResult result);

        ChunkStore &store_;
        Node3D &parent_;
        ChunkPrepareHook onPrepare_;
        ChunkPlacedHook onPlaced_;
        ChunkEvictedHook onEvicted_;

        std::unordered_map<IVec3, Voxel3D *, IVec3Hash> loaded_;
        std::unordered_map<IVec3, ChunkStatus, IVec3Hash> skipped_;
        std::unordered_set<IVec3, IVec3Hash> deferred_;
        IVec3 lastFocus_{};
        int32_t lastRadius_ = -1;
        bool hasUpdated_ = false;

        mutable std::mutex mtx_;
        std::condition_variable workerCv_;
        std::condition_variable idleCv_;
        std::deque<IVec3> pending_;
        std::unordered_set<IVec3, IVec3Hash> inFlight_;
        std::deque<ReadyEntry> ready_;
        bool shouldStop_ = false;
        std::thread worker_;
    };
}
