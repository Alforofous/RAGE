#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <vector>
#include "engine/scene/brick.hpp"
#include "math/ivec.hpp"

namespace RAGE {
    class BrickPool;

    /** Fill-time progress reporter: receives a value in [0, 1]. */
    using FillProgressFn = std::function<void(float)>;
    /** Fill-time cancellation poll: return true to abort. */
    using FillCancelFn = std::function<bool()>;

    /**
     * The per-`Voxel3D` storage. A sparse 3-D grid of `BrickHandle`s — one slot per 8³
     * brick region of the voxel grid. Empty regions hold `kEmptyBrick` and cost no
     * memory beyond the slot itself; populated regions point at a brick in the shared
     * pool.
     *
     * `voxelDims` is the *voxel* extent (not rounded). `brickDims = ceil(voxelDims / 8)`
     * is the handle-grid extent. Voxels outside `voxelDims` but inside the trailing
     * partial bricks stay zero (the renderer-side DDA doesn't reach them).
     *
     * Handle ownership: every non-empty entry in `handles_` is allocated from the pool
     * and released when this `VoxelData` is destroyed or when `setVoxel` zeroes the
     * last non-empty voxel in a brick (deferred — see comment in `setVoxel`).
     *
     * Single-threaded. `BrickPool` references must outlive every `VoxelData` that holds
     * a handle from it.
     */
    class VoxelData {
    public:
        VoxelData(BrickPool &pool, IVec3 voxelDims);
        ~VoxelData();

        VoxelData(const VoxelData &) = delete;
        VoxelData &operator=(const VoxelData &) = delete;
        VoxelData(VoxelData &&) = delete;
        VoxelData &operator=(VoxelData &&) = delete;

        IVec3 voxelDims() const { return voxelDims_; }
        IVec3 brickDims() const { return brickDims_; }

        /** Read a single voxel by *voxel* coordinate. Returns 0 if out of bounds or empty. */
        uint32_t voxel(IVec3 c) const;

        /**
         * Write a single voxel. Allocates a brick if the target region is currently
         * empty; marks the brick dirty in the pool. Out-of-bounds writes are silently
         * ignored.
         *
         * @note Does not de-allocate a brick when it becomes all-zero (post-clear).
         *       That cleanup happens during a future explicit `compact()` step so the
         *       hot-path setVoxel stays branch-light. M3 does not ship `compact()`.
         */
        void setVoxel(IVec3 c, uint32_t packed);

        /**
         * Bulk import packed RGBA8 voxels matching `voxelDims()`. Allocates bricks
         * only for 8³ regions that contain at least one non-zero voxel, then bulk-fills
         * each. Mismatched `srcDims` throws.
         */
        void fillFromPackedRGBA8(const uint32_t *src, IVec3 srcDims,
                                  const FillProgressFn &onProgress = {},
                                  const FillCancelFn &onCancel = {});

        BrickHandle handleAt(IVec3 brickCoord) const;
        std::span<const BrickHandle> handles() const { return handles_; }

        /**
         * Invoke `fn(brickCoord, handle)` for every non-empty cell in this VoxelData's
         * handle grid. The order is implementation-defined; callers must not depend on
         * it (today: z-major, y-mid, x-fastest). Decouples consumers (WorldBrickGrid,
         * future LOD streaming) from the underlying storage layout — a hashed-sparse
         * implementation of VoxelData can ship the same callback without touching its
         * consumers.
         */
        void forEachOccupiedBrick(
            const std::function<void(IVec3 brickCoord, BrickHandle handle)> &fn) const;

        /**
         * Re-acquire every currently-occupied brick under the pool's current dedup
         * setting. Walks each handle, snapshots its content, calls `pool.acquireBrick`
         * (which dedups or duplicates depending on `isDedupEnabled()`), updates the
         * handle, releases the old slot. Idempotent if the setting hasn't changed
         * since the last call; collapses or expands storage when it has.
         *
         * Used by the debug UI's "Brick dedup" toggle to give an immediate visible
         * memory effect on flip.
         */
        void rebuildBricks();

        /** Size of the handle-grid storage (excluding the bricks themselves in the pool). */
        size_t handleGridBytes() const { return handles_.size() * sizeof(BrickHandle); }

        /**
         * What dense storage for the same `voxelDims` would have cost (4 bytes per voxel —
         * the encoding the brick pool also uses internally). Used by the memory tracker UI
         * to show the sparse-vs-dense savings.
         */
        size_t denseEquivalentBytes() const {
            return static_cast<size_t>(voxelDims_.x) * static_cast<size_t>(voxelDims_.y)
                   * static_cast<size_t>(voxelDims_.z) * sizeof(uint32_t);
        }

    private:
        size_t brickFlatIndex(IVec3 brickCoord) const;

        BrickPool *pool_;
        IVec3 voxelDims_;
        IVec3 brickDims_;
        std::vector<BrickHandle> handles_;
    };
}
