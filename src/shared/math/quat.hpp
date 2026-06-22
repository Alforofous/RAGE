#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "vec.hpp"

namespace RAGE {
    /**
     * Unit quaternion for 3D rotations. Layout (x, y, z, w) matches GLM. The default-constructed
     * value is the identity rotation (no rotation).
     *
     * GLM does the heavy lifting (slerp, multiplication, axis-angle conversion) via toGlm() /
     * fromGlm() round-trips. The wrapper exists so engine code never imports glm:: types directly.
     */
    class Quat {
    public:
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;

        Quat() = default;
        constexpr Quat(float x_, float y_, float z_, float w_)
            : x(x_)
            , y(y_)
            , z(z_)
            , w(w_) {}

        static Quat fromGlm(const glm::quat &g) { return { g.x, g.y, g.z, g.w }; }
        glm::quat toGlm() const { return { w, x, y, z }; }

        static Quat identity() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
        static Quat fromAxisAngle(const Vec3 &axis, float radians) {
            return fromGlm(glm::angleAxis(radians, axis.toGlm()));
        }
        static Quat fromEulerXYZ(const Vec3 &eulerRadians) { return fromGlm(glm::quat(eulerRadians.toGlm())); }

        Quat operator*(const Quat &o) const { return fromGlm(toGlm() * o.toGlm()); }
        Quat &operator*=(const Quat &o) {
            *this = *this * o;
            return *this;
        }

        bool operator==(const Quat &o) const { return x == o.x && y == o.y && z == o.z && w == o.w; }
        bool operator!=(const Quat &o) const { return !(*this == o); }

        float length() const { return glm::length(toGlm()); }
        Quat normalized() const { return fromGlm(glm::normalize(toGlm())); }
        Quat &normalize() {
            *this = normalized();
            return *this;
        }
        Quat conjugate() const { return { -x, -y, -z, w }; }
        Quat inverse() const { return fromGlm(glm::inverse(toGlm())); }

        Vec3 rotate(const Vec3 &v) const { return Vec3::fromGlm(toGlm() * v.toGlm()); }

        static Quat slerp(const Quat &a, const Quat &b, float t) {
            return fromGlm(glm::slerp(a.toGlm(), b.toGlm(), t));
        }
    };
}
