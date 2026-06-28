#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
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
     * Allocation: free-list first, otherwise append. Bricks are pre-reserved at
     * `kMaxBricks` so handles stay valid across concurrent allocate calls.
     *
     * **Dedup (M4)**. When dedup is enabled, `acquireBrick(contents)` looks up the
     * given brick contents in a content-hash table and returns the existing handle
     * (with refcount++) if any allocated brick is bit-identical. Identical bricks
     * across the scene (uniform regions, repeated patterns, instanced geometry)
     * collapse to a single physical brick in the pool. When dedup is disabled,
     * `acquireBrick` always allocates a fresh slot — the diagnostic A/B counterpart.
     *
     * Mutation of a deduplicated brick goes through `prepareForWrite(h)` which
     * atomically (under one mutex hold) detects shared bricks and clones, or detaches
     * an exclusive brick from the dedup table — returning a private handle ready to
     * mutate. This is the only sanctioned write path; callers do not check refcount
     * themselves (the check-then-act pattern races against concurrent acquireBrick /
     * release from the asset-loader thread).
     *
     * Dirty tracking: per-handle boolean flags + a compact dirty-handle list. Callers
     * mutate bricks via `brick(h)` and call `markDirty(h)`. The renderer drains
     * `drainDirty()` each frame and uploads only those bricks.
     *
     * Single-threaded by design from the rendering side; allocate / release / dedup
     * map operations all serialise on an internal mutex so worker-thread asset loads
     * can run concurrently with the renderer reading the pool.
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
         * Allocate a fresh, exclusively-owned brick. Refcount = 1, contents zero,
         * **not** registered in the dedup map. Use this when the caller intends to
         * mutate the brick in place (e.g. `VoxelData::setVoxel`). Marks dirty so the
         * renderer will upload on the next frame.
         */
        BrickHandle allocate();

        /**
         * Acquire a handle whose pool slot holds the given `contents`.
         * - When **dedup is enabled** and an allocated brick has bit-identical
         *   content, returns that brick's handle and refcount++.
         * - Otherwise allocates a fresh slot, copies `contents` into it, and (when
         *   dedup is enabled) registers it in the content-hash table.
         *
         * This is the M4 entry point — bulk fill paths (`VoxelData::fillFromPackedRGBA8`)
         * should call this so identical bricks across the scene collapse to a single
         * physical slot.
         */
        BrickHandle acquireBrick(const Brick &contents);

        /**
         * Decrement the refcount on `h`. When it reaches zero the slot is returned to
         * the free list and the dedup map entry (if any) is removed. Releasing
         * `kEmptyBrick` is a silent no-op. Releasing an out-of-range handle throws.
         */
        void release(BrickHandle h);

        /**
         * Read-only access to a brick's voxel storage. Mutations go through
         * `writeVoxel` so the pool can mark the brick dirty in lock-step with the
         * write — never `const_cast` the returned reference.
         */
        const Brick &brick(BrickHandle h) const;

        /**
         * Write a single voxel into the brick at `h` and mark it dirty for the next
         * GPU upload. The caller must own `h` exclusively — pass through
         * `prepareForWrite` first if the handle was obtained from `acquireBrick` and
         * might be shared. `localIndex` is `brickVoxelIndex(lx, ly, lz)`.
         */
        void writeVoxel(BrickHandle h, uint32_t localIndex, uint32_t packed);

        /** Current refcount on `h`. Read-only diagnostic — never key control flow off
         *  this value (the result is stale the moment it's returned). To mutate a
         *  brick, use `prepareForWrite` which atomically handles sharing. */
        uint32_t refCount(BrickHandle h) const;

        /**
         * Return a handle whose pool slot is privately-owned by the caller and detached
         * from the dedup table, safe to mutate. If `h` was shared (refcount > 1) the
         * pool clones to a fresh slot and decrements the old refcount; the returned
         * handle is then *new*. If `h` was exclusive (refcount == 1) the pool detaches
         * it from the dedup table and returns the same handle. Both cases run under a
         * single mutex hold, so concurrent acquireBrick / release from other threads
         * cannot wedge the dedup invariants.
         *
         * Returns `kEmptyBrick` unchanged. Callers writing to a previously-empty cell
         * should call `allocate()` first.
         */
        BrickHandle prepareForWrite(BrickHandle h);

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

        /** Total brick slots (including index 0 and currently-free slots). */
        size_t capacity() const;

        /** Number of bricks currently allocated (excluding index 0 + free list). */
        size_t allocated() const;

        /**
         * Sum of refcounts across all allocated bricks (excluding sentinel). With
         * dedup off this equals `allocated()`; with dedup on the ratio
         * `logicalBricks / allocated` is the dedup factor.
         */
        size_t logicalBricks() const;

        /** Pool memory currently used by allocated bricks (excluding sentinel, free list). */
        size_t allocatedBytes() const { return allocated() * sizeof(Brick); }

        /** Pool memory reserved upfront, regardless of how many bricks are in use. */
        size_t reservedBytes() const { return kMaxBricks * sizeof(Brick); }

        /** Toggle content-hash deduplication. Takes effect on future `acquireBrick`
         *  calls; existing bricks are unaffected. Callers wanting a "rebuild" effect
         *  must walk the scene and re-acquire each brick after toggling. */
        void setDedupEnabled(bool enabled);
        bool isDedupEnabled() const;

    private:
        mutable std::mutex mutex_;
        std::vector<Brick> bricks_;
        std::vector<BrickHandle> freeList_;
        std::vector<uint32_t> refCount_;       // refCount_[h] = ref count for handle h
        std::vector<uint8_t> dirtyFlags_;
        std::vector<BrickHandle> dirtyHandles_;
        bool dedupEnabled_ = true;
        std::unordered_multimap<size_t, BrickHandle> contentHashToHandle_;

        // Helpers (callers hold mutex).
        BrickHandle allocateSlotLocked_();
        void removeFromDedupLocked_(BrickHandle h);
        void markDirtyLocked_(BrickHandle h);
    };
}
