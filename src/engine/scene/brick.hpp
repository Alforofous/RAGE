#pragma once

#include <cstddef>
#include <cstdint>

namespace RAGE {
    /// 8×8×8 dense block of packed-RGBA8 voxels. flat = z*64 + y*8 + x. 2 KB exactly.
    struct Brick {
        static constexpr int32_t kDim = 8;
        static constexpr size_t  kVoxelCount = static_cast<size_t>(kDim) * kDim * kDim;

        uint32_t voxels[kVoxelCount]{};
    };
    static_assert(sizeof(Brick) == Brick::kVoxelCount * sizeof(uint32_t));
    static_assert(sizeof(Brick) == 2048, "Brick must be exactly 2 KB");

    /// Opaque pool index. 0 is the empty-brick sentinel. Layout-compatible with uint32_t for GPU memcpy.
    struct BrickHandle {
        uint32_t id = 0;

        constexpr BrickHandle() = default;
        constexpr explicit BrickHandle(uint32_t v) : id(v) {}

        constexpr bool operator==(const BrickHandle &) const = default;
        constexpr bool operator!=(const BrickHandle &) const = default;
    };
    static_assert(sizeof(BrickHandle) == sizeof(uint32_t),
                  "BrickHandle must be layout-compatible with uint32_t for GPU upload");
    static_assert(alignof(BrickHandle) == alignof(uint32_t));

    inline constexpr BrickHandle kEmptyBrick{};

    inline constexpr size_t brickVoxelIndex(int32_t x, int32_t y, int32_t z) {
        return (static_cast<size_t>(z) * Brick::kDim * Brick::kDim)
               + (static_cast<size_t>(y) * Brick::kDim) + static_cast<size_t>(x);
    }
}
