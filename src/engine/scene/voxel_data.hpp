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
         *        and window move. Consumers (renderer GPU sync) poll it to detect
         *        "did this volume change since I last uploaded it".
         */
        uint64_t version() const { return version_; }

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

        BrickPool *pool_;
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
