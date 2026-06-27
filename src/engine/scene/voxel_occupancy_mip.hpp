#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include "math/ivec.hpp"

namespace RAGE {
    /**
     * Optional cooperative-cancellation callback the builder polls between Z slices. Return
     * true to stop the build early (leaves the output buffer in a partially-filled state).
     * Useful for app shutdown — main thread sets an atomic, builder bails out within ~ms.
     */
    using MipBuildCancelFn = std::function<bool()>;

    /**
     * Layout of a multi-level occupancy mip pyramid for a 3-D voxel grid. Each level halves
     * each axis (rounded up, clamped to ≥ 1). Levels stop when adding another would collapse
     * to a single bit, or when a configured max-level cap is reached — whichever comes first.
     *
     * `levelDims[L]` is the cell count of mip level L along each axis (level 0 here is the
     * first mip level above the source voxel grid). `levelByteOffsets[L]` is the byte offset
     * into a single packed buffer where level L's bits begin. `totalBytes` is the size of
     * that packed buffer.
     *
     * @note Bits within a level are packed in linear order:
     *       `bitIndex(x, y, z) = z * dim.x * dim.y + y * dim.x + x`.
     */
    struct OccupancyMipLayout {
        std::vector<IVec3> levelDims;
        std::vector<uint32_t> levelByteOffsets;
        uint32_t totalBytes = 0;
    };

    /**
     * Compute the layout for an occupancy mip pyramid given the source voxel-grid dimensions
     * and a max-level cap. Pure function; allocates no GPU resources.
     *
     * Returns an empty layout (`totalBytes == 0`, `levelDims` empty) when no useful mip level
     * exists — typically when the source grid is already 1×1×1, or when `maxLevels == 0`.
     */
    OccupancyMipLayout computeOccupancyMipLayout(IVec3 sourceDims, uint32_t maxLevels);

    /**
     * Build the mip pyramid's packed bit data from a source voxel grid.
     *
     * `voxelData` points at `sourceDims.x * sourceDims.y * sourceDims.z` packed RGBA8 voxels
     * (one `uint32_t` per voxel, alpha byte at bits 24–31). A voxel is "occupied" when its
     * alpha byte is non-zero.
     *
     * `mipOutput` must point at a buffer of at least `layout.totalBytes` writable bytes. It
     * is fully overwritten — callers don't need to pre-zero it.
     *
     * Pure function: no allocation, no I/O, no Vulkan dependencies. Easily unit-testable.
     */
    void buildOccupancyMip(const uint32_t *voxelData, IVec3 sourceDims, const OccupancyMipLayout &layout,
                           uint8_t *mipOutput, const MipBuildCancelFn &shouldCancel = {});

    /**
     * Linear bit index for a cell within a level of the given dimensions.
     * Exposed for tests / shaders that need to read individual bits from the packed buffer.
     */
    uint32_t mipBitIndex(IVec3 cell, IVec3 levelDims);

    /** Read one bit from packed level data. Pure. */
    bool mipReadBit(const uint8_t *levelData, uint32_t bitIdx);
}
