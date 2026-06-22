#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "quat.hpp"
#include "vec.hpp"

namespace RAGE {
    /**
     * 3x3 column-major matrix. Used for normal transforms and the rotation portion of a Mat4.
     * Default-constructed value is the identity.
     *
     * Storage and arithmetic delegate to glm::mat3.
     */
    class Mat3 {
    public:
        Mat3() = default;
        constexpr explicit Mat3(float diagonal)
            : m_(diagonal) {}

        static Mat3 fromGlm(const glm::mat3 &g) {
            Mat3 r;
            r.m_ = g;
            return r;
        }
        const glm::mat3 &toGlm() const { return m_; }

        static Mat3 identity() { return Mat3(1.0f); }
        static Mat3 fromQuat(const Quat &q) { return fromGlm(glm::mat3_cast(q.toGlm())); }

        Mat3 operator*(const Mat3 &o) const { return fromGlm(m_ * o.m_); }
        Vec3 operator*(const Vec3 &v) const { return Vec3::fromGlm(m_ * v.toGlm()); }

        bool operator==(const Mat3 &o) const { return m_ == o.m_; }
        bool operator!=(const Mat3 &o) const { return !(*this == o); }

        Mat3 transposed() const { return fromGlm(glm::transpose(m_)); }
        Mat3 inverted() const { return fromGlm(glm::inverse(m_)); }
        float determinant() const { return glm::determinant(m_); }

        Vec3 column(int i) const { return Vec3::fromGlm(m_[i]); }
        const float *data() const { return &m_[0][0]; }

    private:
        glm::mat3 m_{ 1.0f };
    };

    /**
     * 4x4 column-major matrix. Engine workhorse for object transforms and camera matrices.
     * Default-constructed value is the identity.
     *
     * Storage and arithmetic delegate to glm::mat4. Factory methods cover the common
     * transform-construction recipes (translation, rotation, scale, perspective, lookAt).
     *
     * @note Column-major matches Vulkan/GLM convention. For a typical
     *       model-from-TRS construction, multiply in T * R * S order.
     */
    class Mat4 {
    public:
        Mat4() = default;
        constexpr explicit Mat4(float diagonal)
            : m_(diagonal) {}

        static Mat4 fromGlm(const glm::mat4 &g) {
            Mat4 r;
            r.m_ = g;
            return r;
        }
        const glm::mat4 &toGlm() const { return m_; }

        static Mat4 identity() { return Mat4(1.0f); }
        static Mat4 translation(const Vec3 &t) { return fromGlm(glm::translate(glm::mat4(1.0f), t.toGlm())); }
        static Mat4 scale(const Vec3 &s) { return fromGlm(glm::scale(glm::mat4(1.0f), s.toGlm())); }
        static Mat4 rotation(const Quat &q) { return fromGlm(glm::mat4_cast(q.toGlm())); }
        static Mat4 fromTRS(const Vec3 &translation, const Quat &rotation, const Vec3 &scale) {
            return Mat4::translation(translation) * Mat4::rotation(rotation) * Mat4::scale(scale);
        }

        static Mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar) {
            glm::mat4 p = glm::perspective(fovYRadians, aspect, zNear, zFar);
            p[1][1] *= -1.0f;
            return fromGlm(p);
        }
        static Mat4 orthographic(float left, float right, float bottom, float top, float zNear, float zFar) {
            return fromGlm(glm::ortho(left, right, bottom, top, zNear, zFar));
        }
        static Mat4 lookAt(const Vec3 &eye, const Vec3 &center, const Vec3 &up) {
            return fromGlm(glm::lookAt(eye.toGlm(), center.toGlm(), up.toGlm()));
        }

        Mat4 operator*(const Mat4 &o) const { return fromGlm(m_ * o.m_); }
        Vec4 operator*(const Vec4 &v) const { return Vec4::fromGlm(m_ * v.toGlm()); }
        Mat4 &operator*=(const Mat4 &o) {
            m_ *= o.m_;
            return *this;
        }

        bool operator==(const Mat4 &o) const { return m_ == o.m_; }
        bool operator!=(const Mat4 &o) const { return !(*this == o); }

        Mat4 transposed() const { return fromGlm(glm::transpose(m_)); }
        Mat4 inverted() const { return fromGlm(glm::inverse(m_)); }
        float determinant() const { return glm::determinant(m_); }

        Vec4 column(int i) const { return Vec4::fromGlm(m_[i]); }
        Mat3 toMat3() const { return Mat3::fromGlm(glm::mat3(m_)); }

        Vec3 transformPoint(const Vec3 &p) const { return (operator*(Vec4(p, 1.0f))).xyz(); }
        Vec3 transformDirection(const Vec3 &d) const { return (operator*(Vec4(d, 0.0f))).xyz(); }

        const float *data() const { return &m_[0][0]; }

    private:
        glm::mat4 m_{ 1.0f };
    };
}
