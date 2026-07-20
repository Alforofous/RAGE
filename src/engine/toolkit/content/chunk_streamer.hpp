#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "engine/toolkit/content/chunk_store.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"

namespace RAGE {
    class Voxel3D;
}

namespace RAGE::Toolkit::Content {
    /**
     * @brief Keeps a world `Voxel3D` filled around a focus point: drives the
     *        volume's storage window from the focus, loads missing chunks inside a
     *        cylinder (XZ radius Euclidean, Y from `store.yRange()`) on an internal
     *        worker thread, and adopts finished chunks' bricks into the volume.
     *        Cells the window leaves behind are freed by the volume itself; chunks
     *        are plain data passing through — never scene nodes.
     *
     * The world volume's `brickDims` must equal the cylinder's bounding box
     * ((2·radius+1)·chunkBricks XZ, yRange·chunkBricks Y); update() throws
     * otherwise (capacity injection: the app derives both from one config).
     */
    class ChunkStreamer {
    public:
        /**
         * @brief Binds a store, a world volume, and the cylinder radius (chunks).
         *        Throws immediately when the world volume's window does not match
         *        the radius/yRange-derived extent (capacity injection mismatch).
         */
        ChunkStreamer(ChunkStore &store, Voxel3D &world, int32_t horizontalRadius);
        ~ChunkStreamer();

        ChunkStreamer(const ChunkStreamer &) = delete;
        ChunkStreamer &operator=(const ChunkStreamer &) = delete;
        ChunkStreamer(ChunkStreamer &&) = delete;
        ChunkStreamer &operator=(ChunkStreamer &&) = delete;

        /// Per-frame driver: derives the focus chunk from a world-space position
        /// (the camera) and keeps the world volume filled around it.
        void update(Vec3 worldFocus);
        /// Chunk-coordinate variant for callers that track focus themselves.
        void update(IVec3 focusChunk);

        /// Block until the worker finishes every pending and in-flight chunk for the
        /// current focus, then drain the ready queue. Test helper — production uses `update`.
        void flushAsync(IVec3 focusChunk);

        int32_t horizontalRadius() const { return hRadius_; }

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
        void slideWindow_(IVec3 focusChunk);
        void pruneOutOfWindow_(IVec3 focusChunk);
        void repopulatePending_(IVec3 focusChunk);
        void applyResult_(IVec3 coord, ChunkResult result);

        ChunkStore &store_;
        Voxel3D &world_;
        int32_t hRadius_ = 0;

        std::unordered_set<IVec3, IVec3Hash> loaded_;
        std::unordered_map<IVec3, ChunkStatus, IVec3Hash> skipped_;
        std::unordered_set<IVec3, IVec3Hash> deferred_;
        IVec3 lastFocus_{};
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
