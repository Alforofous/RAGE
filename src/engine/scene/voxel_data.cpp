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
    }

    VoxelData::VoxelData(BrickPool &pool, IVec3 voxelDims)
        : pool_(&pool)
        , voxelDims_(voxelDims) {
        if (voxelDims.x <= 0 || voxelDims.y <= 0 || voxelDims.z <= 0) {
            throw std::invalid_argument("VoxelData: voxelDims must be positive on all axes");
        }
        brickDims_ = ceilDivBy(voxelDims, Brick::kDim);
        const size_t total = static_cast<size_t>(brickDims_.x) * static_cast<size_t>(brickDims_.y)
                             * static_cast<size_t>(brickDims_.z);
        handles_.assign(total, kEmptyBrick);
    }

    VoxelData::~VoxelData() {
        for (BrickHandle h : handles_) {
            if (h != kEmptyBrick) {
                pool_->release(h);
            }
        }
    }

    size_t VoxelData::brickFlatIndex(IVec3 brickCoord) const {
        return (static_cast<size_t>(brickCoord.z) * static_cast<size_t>(brickDims_.x)
                * static_cast<size_t>(brickDims_.y))
               + (static_cast<size_t>(brickCoord.y) * static_cast<size_t>(brickDims_.x))
               + static_cast<size_t>(brickCoord.x);
    }

    BrickHandle VoxelData::handleAt(IVec3 brickCoord) const {
        if (brickCoord.x < 0 || brickCoord.x >= brickDims_.x || brickCoord.y < 0
            || brickCoord.y >= brickDims_.y || brickCoord.z < 0 || brickCoord.z >= brickDims_.z) {
            return kEmptyBrick;
        }
        return handles_[brickFlatIndex(brickCoord)];
    }

    uint32_t VoxelData::voxel(IVec3 c) const {
        if (c.x < 0 || c.x >= voxelDims_.x || c.y < 0 || c.y >= voxelDims_.y || c.z < 0
            || c.z >= voxelDims_.z) {
            return 0u;
        }
        const IVec3 brickCoord{ c.x / Brick::kDim, c.y / Brick::kDim, c.z / Brick::kDim };
        const BrickHandle h = handles_[brickFlatIndex(brickCoord)];
        if (h == kEmptyBrick) {
            return 0u;
        }
        const int32_t lx = c.x % Brick::kDim;
        const int32_t ly = c.y % Brick::kDim;
        const int32_t lz = c.z % Brick::kDim;
        return pool_->brick(h).voxels[brickVoxelIndex(lx, ly, lz)];
    }

    void VoxelData::setVoxel(IVec3 c, uint32_t packed) {
        if (c.x < 0 || c.x >= voxelDims_.x || c.y < 0 || c.y >= voxelDims_.y || c.z < 0
            || c.z >= voxelDims_.z) {
            return;
        }
        const IVec3 brickCoord{ c.x / Brick::kDim, c.y / Brick::kDim, c.z / Brick::kDim };
        const size_t flat = brickFlatIndex(brickCoord);
        BrickHandle h = handles_[flat];
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
        const int32_t lx = c.x % Brick::kDim;
        const int32_t ly = c.y % Brick::kDim;
        const int32_t lz = c.z % Brick::kDim;
        pool_->writeVoxel(h, brickVoxelIndex(lx, ly, lz), packed);
    }

    void VoxelData::fillFromPackedRGBA8(const uint32_t *src, IVec3 srcDims, const FillProgressFn &onProgress,
                                        const FillCancelFn &onCancel) {
        const Core::ProfileZone zone("VoxelData.Fill");
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
                    if (handles_[flat] != kEmptyBrick) {
                        pool_->release(handles_[flat]);
                    }
                    handles_[flat] = pool_->acquireBrick(local);
                }
            }
        }
    }

    void VoxelData::forEachOccupiedBrick(
        const std::function<void(IVec3, BrickHandle)> &fn) const {
        for (int32_t bz = 0; bz < brickDims_.z; ++bz) {
            for (int32_t by = 0; by < brickDims_.y; ++by) {
                for (int32_t bx = 0; bx < brickDims_.x; ++bx) {
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
