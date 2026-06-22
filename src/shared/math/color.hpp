#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace RAGE {
    /**
     * RGBA color in linear 0..1 floating-point space.
     *
     * Public fields, designated-initializer friendly. Provides packing to a 32-bit RGBA8 layout
     * (little-endian byte order: R in bits 0..7, G in 8..15, B in 16..23, A in 24..31), which is
     * the storage format used by the voxel grid and any other shader binding that wants a
     * 32-bit-per-pixel input.
     *
     * @note Linear space, not sRGB-encoded. Display gamma conversion is the renderer's concern,
     *       not Color's.
     */
    struct Color {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;

        constexpr Color() = default;
        constexpr Color(float r_, float g_, float b_, float a_ = 1.0f)
            : r(r_)
            , g(g_)
            , b(b_)
            , a(a_) {}

        static constexpr Color transparent() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }
        static constexpr Color black() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
        static constexpr Color white() { return { 1.0f, 1.0f, 1.0f, 1.0f }; }
        static constexpr Color red() { return { 1.0f, 0.0f, 0.0f, 1.0f }; }
        static constexpr Color green() { return { 0.0f, 1.0f, 0.0f, 1.0f }; }
        static constexpr Color blue() { return { 0.0f, 0.0f, 1.0f, 1.0f }; }

        uint32_t toRGBA8() const {
            const auto byteOf = [](float v) -> uint32_t {
                return static_cast<uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
            };

            return (byteOf(r) << 0u) | (byteOf(g) << 8u) | (byteOf(b) << 16u) | (byteOf(a) << 24u);
        }

        static Color fromRGBA8(uint32_t packed) {
            const auto chan = [&](uint32_t shift) -> float {
                return static_cast<float>((packed >> shift) & 0xFFu) / 255.0f;
            };

            return { chan(0u), chan(8u), chan(16u), chan(24u) };
        }

        bool operator==(const Color &) const = default;
    };
}
