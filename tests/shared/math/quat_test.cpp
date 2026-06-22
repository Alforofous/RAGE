#include <gtest/gtest.h>
#include <cmath>
#include <numbers>
#include "math/quat.hpp"
#include "math/vec.hpp"

using namespace RAGE;

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;

    bool nearEqual(const Vec3 &a, const Vec3 &b, float eps = 1e-5f) {
        return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
    }
}

TEST(Quat, DefaultIsIdentity) {
    const Quat q;
    EXPECT_EQ(q, Quat::identity());
}

TEST(Quat, IdentityRotateIsNoOp) {
    const Quat q = Quat::identity();
    const Vec3 v(1.0f, 2.0f, 3.0f);
    EXPECT_TRUE(nearEqual(q.rotate(v), v));
}

TEST(Quat, AxisAngleAroundYRotatesXToNegZ) {
    const Quat q = Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.5f);
    const Vec3 rotated = q.rotate(Vec3::unitX());
    EXPECT_TRUE(nearEqual(rotated, Vec3(0.0f, 0.0f, -1.0f)));
}

TEST(Quat, AxisAngleFullTurnRestoresVector) {
    const Quat q = Quat::fromAxisAngle(Vec3::unitZ(), 2.0f * kPi);
    EXPECT_TRUE(nearEqual(q.rotate(Vec3::unitX()), Vec3::unitX()));
}

TEST(Quat, InverseUndoesRotation) {
    const Quat q = Quat::fromAxisAngle(Vec3(0.0f, 1.0f, 1.0f).normalized(), 0.7f);
    const Vec3 v(1.0f, 2.0f, 3.0f);
    const Vec3 rotated = q.rotate(v);
    const Vec3 restored = q.inverse().rotate(rotated);
    EXPECT_TRUE(nearEqual(restored, v));
}

TEST(Quat, MultiplicationComposesRotations) {
    const Quat a = Quat::fromAxisAngle(Vec3::unitX(), kPi * 0.5f);
    const Quat b = Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.5f);
    const Quat ab = a * b;
    const Vec3 viaCombined = ab.rotate(Vec3::unitZ());
    const Vec3 viaSequential = a.rotate(b.rotate(Vec3::unitZ()));
    EXPECT_TRUE(nearEqual(viaCombined, viaSequential));
}

TEST(Quat, NormalizedHasUnitLength) {
    Quat q(1.0f, 2.0f, 3.0f, 4.0f);
    q.normalize();
    EXPECT_NEAR(q.length(), 1.0f, 1e-5f);
}

TEST(Quat, SlerpEndpoints) {
    const Quat a = Quat::identity();
    const Quat b = Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.5f);
    EXPECT_EQ(Quat::slerp(a, b, 0.0f), a);
    const Quat midA = Quat::slerp(a, b, 1.0f);
    EXPECT_NEAR(midA.x, b.x, 1e-5f);
    EXPECT_NEAR(midA.y, b.y, 1e-5f);
    EXPECT_NEAR(midA.z, b.z, 1e-5f);
    EXPECT_NEAR(midA.w, b.w, 1e-5f);
}

TEST(Quat, GlmRoundTrip) {
    const Quat q = Quat::fromAxisAngle(Vec3::unitX(), 1.23f);
    EXPECT_EQ(Quat::fromGlm(q.toGlm()), q);
}
