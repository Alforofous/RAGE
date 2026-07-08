#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "engine/scene/brick.hpp"

namespace RAGE {
    /**
     * @brief Shared pool of 8³ voxel bricks. `VoxelData` instances hold `BrickHandle`s
     *        into it instead of inline storage; multiple owners may share the same
     *        handle (instancing). Index 0 is reserved as the empty-brick sentinel.
     *        With dedup on, identical content collapses to one slot via content hash.
     *        Thread-safe; the renderer reads while the asset-loader thread allocates.
     */
    /**
     * @brief Pool sizing + policy, injected by the application ("configure the pipeline").
     *        The engine owns no capacity constants — see the app's world config for how
     *        `maxBricks` is derived from view distance. Both fields are fixed for the
     *        pool's lifetime; replace the whole pool to change them.
     */
    struct BrickPoolConfig {
        size_t maxBricks = 16384;
        bool enableDedup = true;
    };

    class BrickPool {
    public:
        explicit BrickPool(BrickPoolConfig config = {});

        /// Fresh exclusively-owned brick, NOT in the dedup map. For in-place mutation.
        BrickHandle allocate();

        /// Get-or-create the slot whose content equals `contents`. The bulk-fill entry point.
        BrickHandle acquireBrick(const Brick &contents);

        /// Refcount--; slot returns to the free list at zero. `kEmptyBrick` is a no-op.
        void release(BrickHandle h);

        const Brick &brick(BrickHandle h) const;

        /// Write a voxel + mark dirty. Caller must own `h` exclusively (run `prepareForWrite` first if dedup'd).
        void writeVoxel(BrickHandle h, uint32_t localIndex, uint32_t packed);

        /// Diagnostic only — stale the moment it's returned. Use `prepareForWrite` to mutate safely.
        uint32_t refCount(BrickHandle h) const;

        /**
         * @brief Return a handle the caller can safely mutate. If shared, clones to a
         *        new slot and drops the caller's old share. If exclusive, detaches
         *        from the dedup table. Atomic under one mutex hold — the only safe
         *        write path; the check-then-act pattern races against acquireBrick.
         */
        BrickHandle prepareForWrite(BrickHandle h);

        void markDirty(BrickHandle h);
        std::vector<BrickHandle> drainDirty();

        size_t capacity() const;
        size_t allocated() const;
        /// Sum of refcounts across allocated bricks. `logicalBricks / allocated` = dedup ratio.
        size_t logicalBricks() const;
        size_t allocatedBytes() const { return allocated() * sizeof(Brick); }
        size_t maxBricks() const { return maxBricks_; }
        size_t reservedBytes() const { return maxBricks_ * sizeof(Brick); }
        bool isDedupEnabled() const { return dedupEnabled_; }

    private:
        mutable std::mutex mutex_;
        size_t maxBricks_ = 0;
        std::vector<Brick> bricks_;
        std::vector<BrickHandle> freeList_;
        std::vector<uint32_t> refCount_;
        std::vector<uint8_t> dirtyFlags_;
        std::vector<BrickHandle> dirtyHandles_;
        bool dedupEnabled_ = true;
        std::unordered_multimap<size_t, BrickHandle> contentHashToHandle_;

        BrickHandle allocateSlotLocked_();
        void removeFromDedupLocked_(BrickHandle h);
        void markDirtyLocked_(BrickHandle h);
    };
}
