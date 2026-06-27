#pragma once

#include <cstddef>
#include <cstdint>

namespace RAGE {
    /**
     * One brick = an 8×8×8 dense block of packed-RGBA8 voxels.
     *
     * Layout is z-major, y-mid, x-fastest — matches the existing voxel buffer / mip
     * conventions so brick reads in the shader use the same flat-index math:
     *     flat = z * 64 + y * 8 + x  (0..511).
     *
     * Both the CPU `BrickPool` and the GPU SSBO array hold contiguous arrays of this
     * exact struct; the layout is binary-compatible by construction (POD, single
     * uint32_t field). Padding-free.
     *
     * Sized at 2 KB so allocations and GPU uploads work in whole bricks without
     * sub-brick partial copies.
     */
    struct Brick {
        static constexpr int32_t kDim = 8;
        static constexpr size_t  kVoxelCount = static_cast<size_t>(kDim) * kDim * kDim;

        uint32_t voxels[kVoxelCount]{};
    };
    static_assert(sizeof(Brick) == Brick::kVoxelCount * sizeof(uint32_t));
    static_assert(sizeof(Brick) == 2048, "Brick must be exactly 2 KB");

    /**
     * Pool index into a `BrickPool`. Zero is reserved as the "no brick" sentinel so a
     * default-zero-initialized world grid means "empty everywhere"; valid handles are
     * 1..N. Treat as opaque — never arithmetic on a handle outside the pool.
     */
    using BrickHandle = uint32_t;
    inline constexpr BrickHandle kEmptyBrick = 0u;

    /** Flat index within a brick, 0..511. */
    inline constexpr size_t brickVoxelIndex(int32_t x, int32_t y, int32_t z) {
        return (static_cast<size_t>(z) * Brick::kDim * Brick::kDim)
               + (static_cast<size_t>(y) * Brick::kDim) + static_cast<size_t>(x);
    }
}
