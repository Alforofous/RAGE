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
     * @brief Sparse handle-grid into a shared `BrickPool` — one slot per 8³ voxel
     *        region. `brickDims = ceil(voxelDims / 8)`. The pool must outlive every
     *        `VoxelData` that holds a handle from it.
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

        /// 0 if out of bounds or empty.
        uint32_t voxel(IVec3 c) const;

        /// Out-of-bounds writes are silently ignored. Does NOT shrink — call `compact()` (future).
        void setVoxel(IVec3 c, uint32_t packed);

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

        BrickPool *pool_;
        IVec3 voxelDims_;
        IVec3 brickDims_;
        std::vector<BrickHandle> handles_;
    };
}
