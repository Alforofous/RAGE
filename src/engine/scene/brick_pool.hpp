#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>
#include "engine/scene/brick.hpp"

namespace RAGE {
    /**
     * Renderer-owned, contiguous, growable pool of 8³ voxel bricks.
     *
     * The pool is the single source of truth for brick memory. `VoxelData` instances
     * hold `BrickHandle`s (= pool indices) instead of inline voxel storage; multiple
     * scene-tree nodes can hold the same handle (instancing, M3-E). Index 0 is reserved
     * as the "no brick" sentinel so a zero-initialised world grid means "empty
     * everywhere"; the brick at index 0 is never written to.
     *
     * Allocation: free-list first, otherwise append. Growth doubles. The GPU buffer is
     * recreated when the pool grows past its current capacity (rare — log-N events).
     *
     * Dirty tracking: per-brick boolean flags + a compact dirty-handle list. Callers
     * mutate bricks via `brick(h)` and call `markDirty(h)`. The renderer drains
     * `dirtyHandles()` each frame and uploads only those bricks.
     *
     * Single-threaded by design — no internal locking. M3 does not allow background
     * brick edits; that's an M5 streaming concern.
     */
    class BrickPool {
    public:
        /**
         * Max number of bricks the pool will hold. Bricks_/dirtyFlags_ are pre-reserved
         * at this capacity, so allocate() never grows the underlying vector and the
         * references returned by brick() stay valid for the pool's lifetime — letting
         * the render thread read a brick reference while the worker thread is writing a
         * *different* brick without an iterator-invalidation race.
         *
         * Sized to match the renderer's GPU pool capacity. Allocations past this throw.
         */
        static constexpr size_t kMaxBricks = 16384;

        BrickPool();

        /**
         * Allocate a new brick. Returns a handle in `1..capacity()`. The brick's voxels
         * are zero-initialised. Marks the new brick dirty so the renderer will upload
         * it on the next frame.
         */
        BrickHandle allocate();

        /**
         * Return a previously-allocated handle to the free list. The brick contents are
         * not cleared (cheap re-use); the next allocator caller gets a zeroed brick.
         * Passing `kEmptyBrick` is a silent no-op.
         */
        void release(BrickHandle h);

        Brick &brick(BrickHandle h);
        const Brick &brick(BrickHandle h) const;

        /** Total brick slots (including index 0 and currently-free slots). */
        size_t capacity() const { return bricks_.size(); }

        /** Number of bricks currently allocated (excluding index 0 + free list). */
        size_t allocated() const { return bricks_.size() - 1u - freeList_.size(); }

        /**
         * Mark a brick as needing GPU re-upload. Cheap to call repeatedly per frame
         * for the same handle — the flag dedupes. Passing `kEmptyBrick` is ignored.
         */
        void markDirty(BrickHandle h);

        /**
         * Atomically take the current dirty list and clear it. Returns a freshly-owned
         * vector so the caller can iterate without holding the pool's lock — the
         * renderer drains every frame and copies the named bricks to the GPU buffer.
         */
        std::vector<BrickHandle> drainDirty();

    private:
        mutable std::mutex mutex_;
        std::vector<Brick> bricks_;
        std::vector<BrickHandle> freeList_;
        std::vector<uint8_t> dirtyFlags_;       // 0/1 per brick handle
        std::vector<BrickHandle> dirtyHandles_;
    };
}
