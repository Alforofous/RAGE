#include "voxel_data.hpp"
#include "shared/profiling.hpp"

#include <algorithm>
#include <stdexcept>
#include "engine/scene/brick_pool.hpp"

namespace RAGE {
    namespace {
        IVec3 ceilDivBy(IVec3 a, int32_t b) {
            return IVec3{ (a.x + b - 1) / b, (a.y + b - 1) / b, (a.z + b - 1) / b };
        }

        int32_t floorDivBrick(int32_t v) {
            return (v >= 0) ? (v / Brick::kDim) : (-(((-v) + Brick::kDim - 1) / Brick::kDim));
        }

        int32_t floorModBrick(int32_t v) {
            const int32_t m = v % Brick::kDim;
            return m < 0 ? m + Brick::kDim : m;
        }

        int32_t nextPow2(int32_t v) {
            int32_t p = 1;
            while (p < v) {
                p *= 2;
            }
            return p;
        }

        bool boxContains(IVec3 boxMin, IVec3 boxDims, IVec3 c) {
            return c.x >= boxMin.x && c.x < boxMin.x + boxDims.x && c.y >= boxMin.y
                   && c.y < boxMin.y + boxDims.y && c.z >= boxMin.z && c.z < boxMin.z + boxDims.z;
        }

        /// newBox minus oldBox as disjoint BrickRegions (whole newBox when disjoint).
        void subtractBox(IVec3 newMin, IVec3 dims, IVec3 oldMin, IVec3 oldDims,
                         std::vector<BrickRegion> &out) {
            IVec3 remMin = newMin;
            IVec3 remMax = newMin + dims;
            const IVec3 oldMax = oldMin + oldDims;
            for (int32_t axis = 0; axis < 3; ++axis) {
                if (remMin[axis] >= remMax[axis]) {
                    return;
                }
                if (oldMin[axis] > remMin[axis]) {
                    IVec3 sliceMax = remMax;
                    sliceMax[axis] = std::min(remMax[axis], oldMin[axis]);
                    out.push_back(BrickRegion{ .minBrick = remMin, .brickDims = sliceMax - remMin });
                    remMin[axis] = sliceMax[axis];
                }
                if (oldMax[axis] < remMax[axis]) {
                    IVec3 sliceMin = remMin;
                    sliceMin[axis] = std::max(remMin[axis], oldMax[axis]);
                    out.push_back(BrickRegion{ .minBrick = sliceMin, .brickDims = remMax - sliceMin });
                    remMax[axis] = sliceMin[axis];
                }
                if (remMin[axis] >= remMax[axis]) {
                    return;
                }
            }
            // What remains overlaps oldBox on every axis: fully covered, emit nothing.
        }
    }

    VoxelData::VoxelData(IVec3 voxelDims)
        : voxelDims_(voxelDims) {
        if (voxelDims.x <= 0 || voxelDims.y <= 0 || voxelDims.z <= 0) {
            throw std::invalid_argument("VoxelData: voxelDims must be positive on all axes");
        }
        brickDims_ = ceilDivBy(voxelDims, Brick::kDim);
        storageDims_ = brickDims_;
        const size_t total = static_cast<size_t>(brickDims_.x) * static_cast<size_t>(brickDims_.y)
                             * static_cast<size_t>(brickDims_.z);
        handles_.assign(total, kEmptyBrick);
    }

    VoxelData::VoxelData(BrickPool &pool, IVec3 voxelDims)
        : VoxelData(voxelDims) {
        pool_ = &pool;
    }

    void VoxelData::adoptInto(BrickPool &pool) {
        if (pool_ != nullptr) {
            throw std::logic_error("VoxelData::adoptInto: already pool-backed");
        }
        if (!isAdoptable()) {
            throw std::logic_error("VoxelData::adoptInto: bulk load in progress");
        }
        pool_ = &pool;
        for (BrickHandle &h : handles_) {
            if (h == kEmptyBrick) {
                continue;
            }
            // Staged handle ids are staging index + 1; re-acquire through the pool
            // so adoption rides the dedup path.
            h = pool.acquireBrick(staging_[h.id - 1]);
        }
        staging_.clear();
        staging_.shrink_to_fit();
        ++version_;
    }

    VoxelData::~VoxelData() {
        if (pool_ == nullptr) {
            return;   // staged bricks die with staging_
        }
        for (BrickHandle h : handles_) {
            if (h != kEmptyBrick) {
                pool_->release(h);
            }
        }
    }

    size_t VoxelData::brickFlatIndex(IVec3 brickCoord) const {
        if (windowed_) {
            const IVec3 slot{ brickCoord.x & wrapMask_.x, brickCoord.y & wrapMask_.y,
                              brickCoord.z & wrapMask_.z };
            return (static_cast<size_t>(slot.z) * static_cast<size_t>(storageDims_.x)
                    * static_cast<size_t>(storageDims_.y))
                   + (static_cast<size_t>(slot.y) * static_cast<size_t>(storageDims_.x))
                   + static_cast<size_t>(slot.x);
        }
        return (static_cast<size_t>(brickCoord.z) * static_cast<size_t>(brickDims_.x)
                * static_cast<size_t>(brickDims_.y))
               + (static_cast<size_t>(brickCoord.y) * static_cast<size_t>(brickDims_.x))
               + static_cast<size_t>(brickCoord.x);
    }

    bool VoxelData::brickInWindow_(IVec3 brickCoord) const {
        return boxContains(windowOriginBrick_, brickDims_, brickCoord);
    }

    void VoxelData::convertToWindowed_() {
        storageDims_ = IVec3{ nextPow2(brickDims_.x), nextPow2(brickDims_.y),
                              nextPow2(brickDims_.z) };
        wrapMask_ = IVec3{ storageDims_.x - 1, storageDims_.y - 1, storageDims_.z - 1 };
        std::vector<BrickHandle> linear = std::move(handles_);
        handles_.assign(static_cast<size_t>(storageDims_.x) * static_cast<size_t>(storageDims_.y)
                            * static_cast<size_t>(storageDims_.z),
                        kEmptyBrick);
        windowed_ = true;
        // Relocate handle values into wrapped slots; origin is still {0,0,0} here, so
        // world brick == old linear coord. Brick contents never move.
        for (int32_t bz = 0; bz < brickDims_.z; ++bz) {
            for (int32_t by = 0; by < brickDims_.y; ++by) {
                for (int32_t bx = 0; bx < brickDims_.x; ++bx) {
                    const size_t oldFlat =
                        (static_cast<size_t>(bz) * static_cast<size_t>(brickDims_.x)
                         * static_cast<size_t>(brickDims_.y))
                        + (static_cast<size_t>(by) * static_cast<size_t>(brickDims_.x))
                        + static_cast<size_t>(bx);
                    if (linear[oldFlat] != kEmptyBrick) {
                        handles_[brickFlatIndex(IVec3{ bx, by, bz })] = linear[oldFlat];
                    }
                }
            }
        }
    }

    void VoxelData::releaseBrickAt_(IVec3 brickCoord) {
        const size_t flat = brickFlatIndex(brickCoord);
        if (handles_[flat] != kEmptyBrick) {
            if (pool_ != nullptr) {
                pool_->release(handles_[flat]);
            }
            handles_[flat] = kEmptyBrick;   // staged bricks are dropped at adoption
        }
    }

    std::vector<BrickRegion> VoxelData::setWindowCenterBrick(IVec3 centerBrick) {
        if (!windowed_) {
            convertToWindowed_();
        }
        const IVec3 newOrigin = IVec3{ centerBrick.x - (brickDims_.x / 2),
                                       centerBrick.y - (brickDims_.y / 2),
                                       centerBrick.z - (brickDims_.z / 2) };
        if (newOrigin == windowOriginBrick_) {
            return {};
        }
        const IVec3 oldOrigin = windowOriginBrick_;

        // Free every cell that left BEFORE moving the origin, so wrapped slots are
        // clean when their world meaning changes.
        for (int32_t bz = oldOrigin.z; bz < oldOrigin.z + brickDims_.z; ++bz) {
            for (int32_t by = oldOrigin.y; by < oldOrigin.y + brickDims_.y; ++by) {
                for (int32_t bx = oldOrigin.x; bx < oldOrigin.x + brickDims_.x; ++bx) {
                    const IVec3 c{ bx, by, bz };
                    if (!boxContains(newOrigin, brickDims_, c)) {
                        releaseBrickAt_(c);
                    }
                }
            }
        }
        windowOriginBrick_ = newOrigin;
        ++version_;

        std::vector<BrickRegion> entered;
        subtractBox(newOrigin, brickDims_, oldOrigin, brickDims_, entered);
        return entered;
    }

    BrickHandle VoxelData::handleAt(IVec3 brickCoord) const {
        if (!brickInWindow_(brickCoord)) {
            return kEmptyBrick;
        }
        return handles_[brickFlatIndex(brickCoord)];
    }

    uint32_t VoxelData::voxel(IVec3 c) const {
        if (!windowed_
            && (c.x < 0 || c.x >= voxelDims_.x || c.y < 0 || c.y >= voxelDims_.y || c.z < 0
                || c.z >= voxelDims_.z)) {
            return 0u;
        }
        const IVec3 brickCoord{ floorDivBrick(c.x), floorDivBrick(c.y), floorDivBrick(c.z) };
        if (windowed_ && !brickInWindow_(brickCoord)) {
            return 0u;
        }
        const BrickHandle h = handles_[brickFlatIndex(brickCoord)];
        if (h == kEmptyBrick) {
            return 0u;
        }
        const int32_t lx = floorModBrick(c.x);
        const int32_t ly = floorModBrick(c.y);
        const int32_t lz = floorModBrick(c.z);
        // Staged handle ids are staging index + 1 (0 is the empty sentinel).
        const Brick &brick = pool_ != nullptr ? pool_->brick(h) : staging_[h.id - 1];
        return brick.voxels[brickVoxelIndex(lx, ly, lz)];
    }

    void VoxelData::setVoxel(IVec3 c, uint32_t packed) {
        if (!windowed_
            && (c.x < 0 || c.x >= voxelDims_.x || c.y < 0 || c.y >= voxelDims_.y || c.z < 0
                || c.z >= voxelDims_.z)) {
            return;
        }
        const IVec3 brickCoord{ floorDivBrick(c.x), floorDivBrick(c.y), floorDivBrick(c.z) };
        if (windowed_ && !brickInWindow_(brickCoord)) {
            return;
        }
        const size_t flat = brickFlatIndex(brickCoord);
        BrickHandle h = handles_[flat];
        const int32_t lx = floorModBrick(c.x);
        const int32_t ly = floorModBrick(c.y);
        const int32_t lz = floorModBrick(c.z);
        if (pool_ == nullptr) {
            if (h == kEmptyBrick) {
                if (packed == 0u) {
                    return;
                }
                h = BrickHandle{ static_cast<uint32_t>(staging_.size()) + 1u };
                staging_.emplace_back();
                handles_[flat] = h;
            }
            staging_[h.id - 1].voxels[brickVoxelIndex(lx, ly, lz)] = packed;
            ++version_;
            return;
        }
        if (h == kEmptyBrick) {
            if (packed == 0u) {
                return;
            }
            h = pool_->allocate();
            handles_[flat] = h;
        } else {
            const BrickHandle writable = pool_->prepareForWrite(h);
            if (writable != h) {
                handles_[flat] = writable;
                h = writable;
            }
        }
        pool_->writeVoxel(h, brickVoxelIndex(lx, ly, lz), packed);
        ++version_;
    }

    void VoxelData::fillFromPackedRGBA8(const uint32_t *src, IVec3 srcDims, const FillProgressFn &onProgress,
                                        const FillCancelFn &onCancel) {
        const Core::ProfileZone zone("VoxelData.Fill");
        if (windowed_) {
            throw std::logic_error(
                "VoxelData::fillFromPackedRGBA8: unsupported after the window has moved");
        }
        if (src == nullptr) {
            throw std::invalid_argument("VoxelData::fillFromPackedRGBA8: src is null");
        }
        if (srcDims.x != voxelDims_.x || srcDims.y != voxelDims_.y || srcDims.z != voxelDims_.z) {
            throw std::invalid_argument("VoxelData::fillFromPackedRGBA8: srcDims must match voxelDims()");
        }

        const auto srcFlat = [&](int32_t x, int32_t y, int32_t z) -> size_t {
            return (static_cast<size_t>(z) * static_cast<size_t>(srcDims.x) * static_cast<size_t>(srcDims.y))
                   + (static_cast<size_t>(y) * static_cast<size_t>(srcDims.x)) + static_cast<size_t>(x);
        };

        const int32_t totalRows = brickDims_.z * brickDims_.y;
        int32_t rowIdx = 0;
        for (int32_t bz = 0; bz < brickDims_.z; ++bz) {
            for (int32_t by = 0; by < brickDims_.y; ++by) {
                if (onCancel && onCancel()) {
                    return;
                }
                if (onProgress && totalRows > 0) {
                    onProgress(static_cast<float>(rowIdx) / static_cast<float>(totalRows));
                }
                ++rowIdx;
                for (int32_t bx = 0; bx < brickDims_.x; ++bx) {
                    const int32_t x0 = bx * Brick::kDim;
                    const int32_t y0 = by * Brick::kDim;
                    const int32_t z0 = bz * Brick::kDim;
                    const int32_t x1 = std::min(x0 + Brick::kDim, voxelDims_.x);
                    const int32_t y1 = std::min(y0 + Brick::kDim, voxelDims_.y);
                    const int32_t z1 = std::min(z0 + Brick::kDim, voxelDims_.z);

                    bool anyNonZero = false;
                    for (int32_t z = z0; z < z1 && !anyNonZero; ++z) {
                        for (int32_t y = y0; y < y1 && !anyNonZero; ++y) {
                            for (int32_t x = x0; x < x1; ++x) {
                                if (src[srcFlat(x, y, z)] != 0u) {
                                    anyNonZero = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!anyNonZero) {
                        continue;
                    }

                    Brick local{};
                    for (int32_t z = z0; z < z1; ++z) {
                        for (int32_t y = y0; y < y1; ++y) {
                            for (int32_t x = x0; x < x1; ++x) {
                                local.voxels[brickVoxelIndex(x - x0, y - y0, z - z0)] = src[srcFlat(x, y, z)];
                            }
                        }
                    }
                    const size_t flat = brickFlatIndex({ bx, by, bz });
                    if (pool_ == nullptr) {
                        if (handles_[flat] == kEmptyBrick) {
                            handles_[flat] =
                                BrickHandle{ static_cast<uint32_t>(staging_.size()) + 1u };
                            staging_.emplace_back();
                        }
                        staging_[handles_[flat].id - 1] = local;
                        continue;
                    }
                    if (handles_[flat] != kEmptyBrick) {
                        pool_->release(handles_[flat]);
                    }
                    handles_[flat] = pool_->acquireBrick(local);
                }
            }
        }
        ++version_;
    }

    void VoxelData::clearBricks(IVec3 minBrick, IVec3 brickDims) {
        bool any = false;
        for (int32_t bz = minBrick.z; bz < minBrick.z + brickDims.z; ++bz) {
            for (int32_t by = minBrick.y; by < minBrick.y + brickDims.y; ++by) {
                for (int32_t bx = minBrick.x; bx < minBrick.x + brickDims.x; ++bx) {
                    const IVec3 c{ bx, by, bz };
                    if (!brickInWindow_(c)) {
                        continue;
                    }
                    if (handles_[brickFlatIndex(c)] != kEmptyBrick) {
                        releaseBrickAt_(c);
                        any = true;
                    }
                }
            }
        }
        if (any) {
            ++version_;
        }
    }

    void VoxelData::adoptBricksFrom(VoxelData &source, IVec3 dstMinBrick) {
        if (pool_ == nullptr || source.pool_ == nullptr) {
            throw std::logic_error(
                "VoxelData::adoptBricksFrom: both volumes must be pool-backed (adopted)");
        }
        if (source.pool_ != pool_) {
            throw std::invalid_argument("VoxelData::adoptBricksFrom: pools differ");
        }
        const IVec3 srcOrigin = source.windowOriginBrick_;
        for (int32_t bz = srcOrigin.z; bz < srcOrigin.z + source.brickDims_.z; ++bz) {
            for (int32_t by = srcOrigin.y; by < srcOrigin.y + source.brickDims_.y; ++by) {
                for (int32_t bx = srcOrigin.x; bx < srcOrigin.x + source.brickDims_.x; ++bx) {
                    const size_t srcFlat = source.brickFlatIndex(IVec3{ bx, by, bz });
                    const BrickHandle h = source.handles_[srcFlat];
                    if (h == kEmptyBrick) {
                        continue;
                    }
                    source.handles_[srcFlat] = kEmptyBrick;
                    const IVec3 dst = dstMinBrick + (IVec3{ bx, by, bz } - srcOrigin);
                    if (!brickInWindow_(dst)) {
                        pool_->release(h);
                        continue;
                    }
                    const size_t dstFlat = brickFlatIndex(dst);
                    if (handles_[dstFlat] != kEmptyBrick) {
                        pool_->release(handles_[dstFlat]);
                    }
                    handles_[dstFlat] = h;
                }
            }
        }
        ++version_;
        ++source.version_;
    }

    void VoxelData::forEachOccupiedBrick(
        const std::function<void(IVec3, BrickHandle)> &fn) const {
        const IVec3 o = windowOriginBrick_;
        for (int32_t bz = o.z; bz < o.z + brickDims_.z; ++bz) {
            for (int32_t by = o.y; by < o.y + brickDims_.y; ++by) {
                for (int32_t bx = o.x; bx < o.x + brickDims_.x; ++bx) {
                    const BrickHandle h = handles_[brickFlatIndex({ bx, by, bz })];
                    if (h == kEmptyBrick) {
                        continue;
                    }
                    fn(IVec3{ bx, by, bz }, h);
                }
            }
        }
    }

}
