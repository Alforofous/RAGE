#pragma once

#include <cstddef>
#include <cstdint>

namespace RAGE {
    /**
     * Three-component integer vector. Used primarily for voxel-grid indices and other discrete
     * 3D coordinates where Vec3's floating-point storage would invite rounding errors.
     *
     * Minimal API by design — just enough arithmetic for the use cases that already exist. Add
     * more ops when a concrete consumer needs them.
     */
    struct IVec3 {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;

        constexpr IVec3() = default;
        constexpr IVec3(int32_t x_, int32_t y_, int32_t z_)
            : x(x_)
            , y(y_)
            , z(z_) {}

        /// Axis access for axis-generic code: 0 = x, 1 = y, anything else = z.
        constexpr int32_t &operator[](int32_t i) {
            switch (i) {
                case 0: return x;
                case 1: return y;
                default: return z;
            }
        }
        constexpr const int32_t &operator[](int32_t i) const {
            switch (i) {
                case 0: return x;
                case 1: return y;
                default: return z;
            }
        }

        static constexpr IVec3 zero() { return { 0, 0, 0 }; }

        constexpr IVec3 operator+(const IVec3 &o) const { return { x + o.x, y + o.y, z + o.z }; }
        constexpr IVec3 operator-(const IVec3 &o) const { return { x - o.x, y - o.y, z - o.z }; }
        constexpr IVec3 operator*(int32_t s) const { return { x * s, y * s, z * s }; }
        constexpr IVec3 operator-() const { return { -x, -y, -z }; }

        bool operator==(const IVec3 &) const = default;

        constexpr int64_t volume() const {
            return static_cast<int64_t>(x) * static_cast<int64_t>(y) * static_cast<int64_t>(z);
        }
    };

    /// Hash functor for `IVec3` keys in unordered containers (splitmix-style mixing).
    struct IVec3Hash {
        size_t operator()(const IVec3 &v) const noexcept {
            const uint64_t x = static_cast<uint32_t>(v.x);
            const uint64_t y = static_cast<uint32_t>(v.y);
            const uint64_t z = static_cast<uint32_t>(v.z);
            uint64_t h = x * 0x9E3779B97F4A7C15ull;
            h ^= y + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
            h ^= z + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
            return static_cast<size_t>(h);
        }
    };

}
