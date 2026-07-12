#pragma once

#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>

namespace RAGE {
    /**
     * Two-component floating-point vector.
     *
     * Public x/y fields for direct access; GLM is used under the hood for non-trivial ops
     * (length, normalisation, dot). fromGlm() / toGlm() round-trip explicitly when interoperating
     * with GLM-typed APIs.
     */
    class Vec2 {
    public:
        float x = 0.0f;
        float y = 0.0f;

        Vec2() = default;
        constexpr Vec2(float x_, float y_)
            : x(x_)
            , y(y_) {}
        constexpr explicit Vec2(float v)
            : x(v)
            , y(v) {}

        static Vec2 fromGlm(const glm::vec2 &g) { return { g.x, g.y }; }
        glm::vec2 toGlm() const { return { x, y }; }

        static Vec2 zero() { return { 0.0f, 0.0f }; }
        static Vec2 unitX() { return { 1.0f, 0.0f }; }
        static Vec2 unitY() { return { 0.0f, 1.0f }; }

        Vec2 operator-() const { return { -x, -y }; }
        Vec2 operator+(const Vec2 &o) const { return { x + o.x, y + o.y }; }
        Vec2 operator-(const Vec2 &o) const { return { x - o.x, y - o.y }; }
        Vec2 operator*(float s) const { return { x * s, y * s }; }
        Vec2 operator/(float s) const { return { x / s, y / s }; }
        Vec2 operator*(const Vec2 &o) const { return { x * o.x, y * o.y }; }
        Vec2 operator/(const Vec2 &o) const { return { x / o.x, y / o.y }; }

        Vec2 &operator+=(const Vec2 &o) {
            x += o.x;
            y += o.y;
            return *this;
        }
        Vec2 &operator-=(const Vec2 &o) {
            x -= o.x;
            y -= o.y;
            return *this;
        }
        Vec2 &operator*=(float s) {
            x *= s;
            y *= s;
            return *this;
        }
        Vec2 &operator/=(float s) {
            x /= s;
            y /= s;
            return *this;
        }

        bool operator==(const Vec2 &o) const { return x == o.x && y == o.y; }
        bool operator!=(const Vec2 &o) const { return !(*this == o); }

        float length() const { return std::sqrt((x * x) + (y * y)); }
        float lengthSquared() const { return (x * x) + (y * y); }
        float dot(const Vec2 &o) const { return (x * o.x) + (y * o.y); }
        Vec2 normalized() const { return fromGlm(glm::normalize(toGlm())); }
        Vec2 &normalize() {
            *this = normalized();
            return *this;
        }

        static Vec2 lerp(const Vec2 &a, const Vec2 &b, float t) {
            return { a.x + ((b.x - a.x) * t), a.y + ((b.y - a.y) * t) };
        }
    };

    inline Vec2 operator*(float s, const Vec2 &v) {
        return v * s;
    }

    /**
     * Three-component floating-point vector. The bread-and-butter type for positions, directions,
     * scale, and colour in the engine. See Vec2 for the wrapper-design notes.
     */
    class Vec3 {
    public:
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        Vec3() = default;
        constexpr Vec3(float x_, float y_, float z_)
            : x(x_)
            , y(y_)
            , z(z_) {}
        constexpr explicit Vec3(float v)
            : x(v)
            , y(v)
            , z(v) {}

        static Vec3 fromGlm(const glm::vec3 &g) { return { g.x, g.y, g.z }; }
        glm::vec3 toGlm() const { return { x, y, z }; }

        static Vec3 zero() { return { 0.0f, 0.0f, 0.0f }; }
        static Vec3 one() { return { 1.0f, 1.0f, 1.0f }; }
        static Vec3 unitX() { return { 1.0f, 0.0f, 0.0f }; }
        static Vec3 unitY() { return { 0.0f, 1.0f, 0.0f }; }
        static Vec3 unitZ() { return { 0.0f, 0.0f, 1.0f }; }

        /// Axis access for axis-generic code: 0 = x, 1 = y, anything else = z.
        float &operator[](int32_t i) {
            switch (i) {
                case 0: return x;
                case 1: return y;
                default: return z;
            }
        }
        const float &operator[](int32_t i) const {
            switch (i) {
                case 0: return x;
                case 1: return y;
                default: return z;
            }
        }

        Vec3 operator-() const { return { -x, -y, -z }; }
        Vec3 operator+(const Vec3 &o) const { return { x + o.x, y + o.y, z + o.z }; }
        Vec3 operator-(const Vec3 &o) const { return { x - o.x, y - o.y, z - o.z }; }
        Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
        Vec3 operator/(float s) const { return { x / s, y / s, z / s }; }
        Vec3 operator*(const Vec3 &o) const { return { x * o.x, y * o.y, z * o.z }; }
        Vec3 operator/(const Vec3 &o) const { return { x / o.x, y / o.y, z / o.z }; }

        Vec3 &operator+=(const Vec3 &o) {
            x += o.x;
            y += o.y;
            z += o.z;
            return *this;
        }
        Vec3 &operator-=(const Vec3 &o) {
            x -= o.x;
            y -= o.y;
            z -= o.z;
            return *this;
        }
        Vec3 &operator*=(float s) {
            x *= s;
            y *= s;
            z *= s;
            return *this;
        }
        Vec3 &operator/=(float s) {
            x /= s;
            y /= s;
            z /= s;
            return *this;
        }

        bool operator==(const Vec3 &o) const { return x == o.x && y == o.y && z == o.z; }
        bool operator!=(const Vec3 &o) const { return !(*this == o); }

        float length() const { return std::sqrt((x * x) + (y * y) + (z * z)); }
        float lengthSquared() const { return (x * x) + (y * y) + (z * z); }
        float dot(const Vec3 &o) const { return (x * o.x) + (y * o.y) + (z * o.z); }
        Vec3 cross(const Vec3 &o) const {
            return { (y * o.z) - (z * o.y), (z * o.x) - (x * o.z), (x * o.y) - (y * o.x) };
        }
        Vec3 normalized() const { return fromGlm(glm::normalize(toGlm())); }
        Vec3 &normalize() {
            *this = normalized();
            return *this;
        }

        static Vec3 lerp(const Vec3 &a, const Vec3 &b, float t) {
            return { a.x + ((b.x - a.x) * t), a.y + ((b.y - a.y) * t), a.z + ((b.z - a.z) * t) };
        }
    };

    inline Vec3 operator*(float s, const Vec3 &v) {
        return v * s;
    }

    /**
     * Four-component floating-point vector. Used for homogeneous coordinates, RGBA colour, and
     * other 4-tuples. See Vec2 for the wrapper-design notes.
     */
    class Vec4 {
    public:
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 0.0f;

        Vec4() = default;
        constexpr Vec4(float x_, float y_, float z_, float w_)
            : x(x_)
            , y(y_)
            , z(z_)
            , w(w_) {}
        constexpr explicit Vec4(float v)
            : x(v)
            , y(v)
            , z(v)
            , w(v) {}
        constexpr Vec4(const Vec3 &xyz, float w_)
            : x(xyz.x)
            , y(xyz.y)
            , z(xyz.z)
            , w(w_) {}

        static Vec4 fromGlm(const glm::vec4 &g) { return { g.x, g.y, g.z, g.w }; }
        glm::vec4 toGlm() const { return { x, y, z, w }; }

        Vec3 xyz() const { return { x, y, z }; }

        static Vec4 zero() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }

        Vec4 operator-() const { return { -x, -y, -z, -w }; }
        Vec4 operator+(const Vec4 &o) const { return { x + o.x, y + o.y, z + o.z, w + o.w }; }
        Vec4 operator-(const Vec4 &o) const { return { x - o.x, y - o.y, z - o.z, w - o.w }; }
        Vec4 operator*(float s) const { return { x * s, y * s, z * s, w * s }; }
        Vec4 operator/(float s) const { return { x / s, y / s, z / s, w / s }; }

        Vec4 &operator+=(const Vec4 &o) {
            x += o.x;
            y += o.y;
            z += o.z;
            w += o.w;
            return *this;
        }
        Vec4 &operator-=(const Vec4 &o) {
            x -= o.x;
            y -= o.y;
            z -= o.z;
            w -= o.w;
            return *this;
        }
        Vec4 &operator*=(float s) {
            x *= s;
            y *= s;
            z *= s;
            w *= s;
            return *this;
        }
        Vec4 &operator/=(float s) {
            x /= s;
            y /= s;
            z /= s;
            w /= s;
            return *this;
        }

        bool operator==(const Vec4 &o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
        bool operator!=(const Vec4 &o) const { return !(*this == o); }

        float length() const { return std::sqrt((x * x) + (y * y) + (z * z) + (w * w)); }
        float lengthSquared() const { return (x * x) + (y * y) + (z * z) + (w * w); }
        float dot(const Vec4 &o) const { return (x * o.x) + (y * o.y) + (z * o.z) + (w * o.w); }
        Vec4 normalized() const { return fromGlm(glm::normalize(toGlm())); }
        Vec4 &normalize() {
            *this = normalized();
            return *this;
        }
    };

    inline Vec4 operator*(float s, const Vec4 &v) {
        return v * s;
    }
}
