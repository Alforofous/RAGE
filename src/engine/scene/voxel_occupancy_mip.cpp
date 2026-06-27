#include "voxel_occupancy_mip.hpp"
#include <algorithm>
#include <cstring>

namespace RAGE {
    namespace {
        IVec3 halveDims(IVec3 dim) {
            return IVec3{ std::max(1, (dim.x + 1) / 2), std::max(1, (dim.y + 1) / 2),
                          std::max(1, (dim.z + 1) / 2) };
        }

        uint32_t levelByteSize(IVec3 dim) {
            const uint32_t cells = static_cast<uint32_t>(dim.x) * static_cast<uint32_t>(dim.y)
                                   * static_cast<uint32_t>(dim.z);
            const uint32_t bytes = (cells + 7u) / 8u;
            return (bytes + 3u) & ~3u;
        }

        void setBit(uint8_t *base, uint32_t bitIdx) {
            base[bitIdx >> 3u] |= static_cast<uint8_t>(1u << (bitIdx & 7u));
        }

        template <typename SourceTest>
        bool blockOccupied(IVec3 dstCell, IVec3 srcDim, const SourceTest &isCellOccupied) {
            for (int32_t dz = 0; dz < 2; ++dz) {
                const int32_t sz = (dstCell.z * 2) + dz;
                if (sz >= srcDim.z) {
                    continue;
                }
                for (int32_t dy = 0; dy < 2; ++dy) {
                    const int32_t sy = (dstCell.y * 2) + dy;
                    if (sy >= srcDim.y) {
                        continue;
                    }
                    for (int32_t dx = 0; dx < 2; ++dx) {
                        const int32_t sx = (dstCell.x * 2) + dx;
                        if (sx >= srcDim.x) {
                            continue;
                        }
                        if (isCellOccupied(IVec3{ sx, sy, sz })) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        template <typename SourceTest>
        bool fillLevel(IVec3 dstDim, uint8_t *dstData, IVec3 srcDim, const SourceTest &isCellOccupied,
                       const MipBuildCancelFn &shouldCancel) {
            for (int32_t z = 0; z < dstDim.z; ++z) {
                if (shouldCancel && shouldCancel()) {
                    return false;
                }
                for (int32_t y = 0; y < dstDim.y; ++y) {
                    for (int32_t x = 0; x < dstDim.x; ++x) {
                        if (blockOccupied({ x, y, z }, srcDim, isCellOccupied)) {
                            setBit(dstData, mipBitIndex({ x, y, z }, dstDim));
                        }
                    }
                }
            }
            return true;
        }

        size_t voxelCoordToIndex(IVec3 c, IVec3 dim) {
            return (static_cast<size_t>(c.z) * static_cast<size_t>(dim.x) * static_cast<size_t>(dim.y))
                   + (static_cast<size_t>(c.y) * static_cast<size_t>(dim.x)) + static_cast<size_t>(c.x);
        }
    }

    uint32_t mipBitIndex(IVec3 cell, IVec3 levelDims) {
        return (static_cast<uint32_t>(cell.z) * static_cast<uint32_t>(levelDims.x)
                * static_cast<uint32_t>(levelDims.y))
               + (static_cast<uint32_t>(cell.y) * static_cast<uint32_t>(levelDims.x))
               + static_cast<uint32_t>(cell.x);
    }

    bool mipReadBit(const uint8_t *levelData, uint32_t bitIdx) {
        return (levelData[bitIdx >> 3u] & static_cast<uint8_t>(1u << (bitIdx & 7u))) != 0u;
    }

    OccupancyMipLayout computeOccupancyMipLayout(IVec3 sourceDims, uint32_t maxLevels) {
        OccupancyMipLayout layout;
        if (maxLevels == 0 || sourceDims.x <= 0 || sourceDims.y <= 0 || sourceDims.z <= 0) {
            return layout;
        }

        IVec3 dim = halveDims(sourceDims);
        uint32_t total = 0;
        for (uint32_t L = 0; L < maxLevels; ++L) {
            if (dim.x <= 1 && dim.y <= 1 && dim.z <= 1) {
                break;
            }
            layout.levelDims.push_back(dim);
            layout.levelByteOffsets.push_back(total);
            total += levelByteSize(dim);
            dim = halveDims(dim);
        }
        layout.totalBytes = total;
        return layout;
    }

    void buildOccupancyMip(const uint32_t *voxelData, IVec3 sourceDims, const OccupancyMipLayout &layout,
                           uint8_t *mipOutput, const MipBuildCancelFn &shouldCancel) {
        if (layout.totalBytes == 0 || layout.levelDims.empty()) {
            return;
        }
        std::memset(mipOutput, 0, layout.totalBytes);

        if (!fillLevel(layout.levelDims[0], mipOutput + layout.levelByteOffsets[0], sourceDims,
                       [&](IVec3 c) -> bool { return (voxelData[voxelCoordToIndex(c, sourceDims)] >> 24u) > 0u; },
                       shouldCancel)) {
            return;
        }

        for (size_t L = 1; L < layout.levelDims.size(); ++L) {
            const IVec3 prevDim = layout.levelDims[L - 1];
            const uint8_t *prevData = mipOutput + layout.levelByteOffsets[L - 1];
            if (!fillLevel(layout.levelDims[L], mipOutput + layout.levelByteOffsets[L], prevDim,
                           [&](IVec3 c) -> bool { return mipReadBit(prevData, mipBitIndex(c, prevDim)); },
                           shouldCancel)) {
                return;
            }
        }
    }
}
