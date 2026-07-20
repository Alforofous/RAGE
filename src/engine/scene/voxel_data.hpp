#pragma once

#include <atomic>
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
     * @brief An axis-aligned box of bricks in window (world-brick) coordinates.
     *        `setWindowCenterBrick` reports freshly-entered, empty regions this way
     *        so a feeder (streamer, generator) knows what to fill.
     */
    struct BrickRegion {
        IVec3 minBrick{ 0, 0, 0 };
        IVec3 brickDims{ 0, 0, 0 };
    };

    /**
     * @brief Sparse handle-grid into a shared `BrickPool` — one slot per 8³ voxel
     *        region. `brickDims = ceil(voxelDims / 8)`. The pool must outlive every
     *        `VoxelData` that holds a handle from it.
     *
     * Coordinates are conceptually unbounded: `voxelDims` sizes a storage *window*
     * that defaults to [0, voxelDims) and never moves on its own — so a volume that
     * never calls `setWindowCenterBrick` behaves as a plain dense volume forever
     * (this is the api-north-star one-primitive contract). The first window move
     * converts storage to power-of-two wrapped addressing internally (handle values
     * relocate; brick contents never copy) and thereafter cells that leave the
     * window release their bricks automatically.
     */
    class VoxelData {
    public:
        VoxelData(BrickPool &pool, IVec3 voxelDims);

        /**
         * @brief Pool-less staging construction (api-north-star N8): voxels live in
         *        internal CPU bricks until a pipeline adopts the volume into its
         *        shared pool on first sight. Reads/writes work identically before
         *        and after adoption.
         */
        explicit VoxelData(IVec3 voxelDims);

        ~VoxelData();

        VoxelData(const VoxelData &) = delete;
        VoxelData &operator=(const VoxelData &) = delete;
        VoxelData(VoxelData &&) = delete;
        VoxelData &operator=(VoxelData &&) = delete;

        IVec3 voxelDims() const { return voxelDims_; }
        IVec3 brickDims() const { return brickDims_; }

        /// 0 if outside the current window or empty.
        uint32_t voxel(IVec3 c) const;

        /// Writes outside the current window are silently ignored (assert planned once
        /// call sites are audited — N7b). Does NOT shrink — call `compact()` (future).
        void setVoxel(IVec3 c, uint32_t packed);

        /**
         * @brief Slide the window so it is centered on `centerBrick` (window min =
         *        center − brickDims/2). Cells that leave the window release their
         *        bricks back to the pool immediately; the returned regions are the
         *        cells that entered (empty, awaiting content), covering the whole
         *        window on the first call or after a teleport. Converts storage to
         *        wrapped addressing on first use — after that, `handles()` is in
         *        storage order (`storageBrickDims()`), not linear `brickDims` order.
         */
        std::vector<BrickRegion> setWindowCenterBrick(IVec3 centerBrick);

        /**
         * @brief Monotonic mutation stamp: bumped by every voxel write, bulk fill,
         *        window move, and brick adoption. Consumers (renderer GPU sync) poll
         *        it to detect "did this volume change since I last uploaded it".
         */
        uint64_t version() const { return version_; }

        /// True once brick handles reference a shared pool (constructed with one, or
        /// adopted). Consumers that read pool bricks directly must check this first.
        bool isAdopted() const { return pool_ != nullptr; }

        /// False while a bulk fill is running on another thread — the pipeline must
        /// not adopt (or read) the volume until the load completes.
        bool isAdoptable() const { return !bulkLoading_.load(std::memory_order_acquire); }

        /// Bracket a worker-thread bulk fill; begin on the owning thread BEFORE the
        /// worker starts, end on the worker when the fill is done.
        void beginBulkLoad() { bulkLoading_.store(true, std::memory_order_release); }
        void endBulkLoad() { bulkLoading_.store(false, std::memory_order_release); }

        /**
         * @brief Move every staged brick into `pool` (through the dedup path) and
         *        switch this volume to pool-backed handles. Throws if already
         *        adopted. Called by the render pipeline on first sight; tests may
         *        call it directly.
         */
        void adoptInto(BrickPool &pool);

        /**
         * @brief Transfer every occupied brick from `source` into this volume, placed
         *        at `dstMinBrick + sourceBrickCoord`. Handle values move — brick
         *        contents never copy, and `source` is left empty (its destructor will
         *        release nothing). Destination cells that already held bricks release
         *        them; source bricks that would land outside this volume's window are
         *        released instead of adopted. Both volumes must share one pool.
         */
        void adoptBricksFrom(VoxelData &source, IVec3 dstMinBrick);

        /**
         * @brief Free every brick in the box [minBrick, minBrick + brickDims) that
         *        lies inside the current window; cleared cells read empty. The
         *        region-shaped counterpart of setVoxel(0) for feeders that evict
         *        (streamer cylinder eviction).
         */
        void clearBricks(IVec3 minBrick, IVec3 brickDims);

        /// Window minimum in brick coordinates ({0,0,0} until the window first moves).
        IVec3 windowOriginBrick() const { return windowOriginBrick_; }
        /// True once setWindowCenterBrick has converted storage to wrapped addressing.
        bool isWindowed() const { return windowed_; }
        /// Allocated handle-grid dims: == brickDims() while dense, pow2-rounded after.
        IVec3 storageBrickDims() const { return storageDims_; }

        /// Bulk import; throws on dim mismatch. Only allocates bricks for non-empty 8³ regions.
        void fillFromPackedRGBA8(const uint32_t *src, IVec3 srcDims,
                                  const FillProgressFn &onProgress = {},
                                  const FillCancelFn &onCancel = {});

        BrickHandle handleAt(IVec3 brickCoord) const;
        std::span<const BrickHandle> handles() const { return handles_; }

        /// Iterate non-empty cells. Order is implementation-defined.
        void forEachOccupiedBrick(
            const std::function<void(IVec3 brickCoord, BrickHandle handle)> &fn) const;

        size_t handleGridBytes() const { return handles_.size() * sizeof(BrickHandle); }

        size_t denseEquivalentBytes() const {
            return static_cast<size_t>(voxelDims_.x) * static_cast<size_t>(voxelDims_.y)
                   * static_cast<size_t>(voxelDims_.z) * sizeof(uint32_t);
        }

    private:
        size_t brickFlatIndex(IVec3 brickCoord) const;
        bool brickInWindow_(IVec3 brickCoord) const;
        void convertToWindowed_();
        void releaseBrickAt_(IVec3 brickCoord);

        BrickPool *pool_ = nullptr;
        std::vector<Brick> staging_;
        std::atomic<bool> bulkLoading_{ false };
        IVec3 voxelDims_;
        IVec3 brickDims_;
        IVec3 storageDims_{ 0, 0, 0 };
        IVec3 wrapMask_{ 0, 0, 0 };
        IVec3 windowOriginBrick_{ 0, 0, 0 };
        uint64_t version_ = 0;
        bool windowed_ = false;
        std::vector<BrickHandle> handles_;
    };
}
