#include <gtest/gtest.h>
#include "math/vec.hpp"

using namespace RAGE;

TEST(Vec3, DefaultConstructIsZero) {
    const Vec3 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3, ScalarBroadcastConstruct) {
    const Vec3 v(2.5f);
    EXPECT_FLOAT_EQ(v.x, 2.5f);
    EXPECT_FLOAT_EQ(v.y, 2.5f);
    EXPECT_FLOAT_EQ(v.z, 2.5f);
}

TEST(Vec3, StaticFactories) {
    EXPECT_EQ(Vec3::zero(), Vec3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(Vec3::one(), Vec3(1.0f, 1.0f, 1.0f));
    EXPECT_EQ(Vec3::unitX(), Vec3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(Vec3::unitY(), Vec3(0.0f, 1.0f, 0.0f));
    EXPECT_EQ(Vec3::unitZ(), Vec3(0.0f, 0.0f, 1.0f));
}

TEST(Vec3, ArithmeticOperators) {
    const Vec3 a(1.0f, 2.0f, 3.0f);
    const Vec3 b(4.0f, 5.0f, 6.0f);
    EXPECT_EQ(a + b, Vec3(5.0f, 7.0f, 9.0f));
    EXPECT_EQ(b - a, Vec3(3.0f, 3.0f, 3.0f));
    EXPECT_EQ(a * 2.0f, Vec3(2.0f, 4.0f, 6.0f));
    EXPECT_EQ(2.0f * a, Vec3(2.0f, 4.0f, 6.0f));
    EXPECT_EQ(b / 2.0f, Vec3(2.0f, 2.5f, 3.0f));
    EXPECT_EQ(-a, Vec3(-1.0f, -2.0f, -3.0f));
}

TEST(Vec3, HadamardProductAndDivision) {
    const Vec3 a(2.0f, 3.0f, 4.0f);
    const Vec3 b(5.0f, 6.0f, 7.0f);
    EXPECT_EQ(a * b, Vec3(10.0f, 18.0f, 28.0f));
    EXPECT_EQ(b / a, Vec3(2.5f, 2.0f, 1.75f));
}

TEST(Vec3, CompoundAssignment) {
    Vec3 a(1.0f, 2.0f, 3.0f);
    a += Vec3(1.0f, 1.0f, 1.0f);
    EXPECT_EQ(a, Vec3(2.0f, 3.0f, 4.0f));
    a -= Vec3(1.0f, 1.0f, 1.0f);
    EXPECT_EQ(a, Vec3(1.0f, 2.0f, 3.0f));
    a *= 2.0f;
    EXPECT_EQ(a, Vec3(2.0f, 4.0f, 6.0f));
    a /= 2.0f;
    EXPECT_EQ(a, Vec3(1.0f, 2.0f, 3.0f));
}

TEST(Vec3, LengthAndLengthSquared) {
    const Vec3 v(3.0f, 4.0f, 0.0f);
    EXPECT_FLOAT_EQ(v.length(), 5.0f);
    EXPECT_FLOAT_EQ(v.lengthSquared(), 25.0f);
}

TEST(Vec3, DotProduct) {
    EXPECT_FLOAT_EQ(Vec3::unitX().dot(Vec3::unitY()), 0.0f);
    EXPECT_FLOAT_EQ(Vec3::unitX().dot(Vec3::unitX()), 1.0f);
    EXPECT_FLOAT_EQ(Vec3(1.0f, 2.0f, 3.0f).dot(Vec3(4.0f, -5.0f, 6.0f)), 12.0f);
}

TEST(Vec3, CrossProductRightHanded) {
    EXPECT_EQ(Vec3::unitX().cross(Vec3::unitY()), Vec3::unitZ());
    EXPECT_EQ(Vec3::unitY().cross(Vec3::unitZ()), Vec3::unitX());
    EXPECT_EQ(Vec3::unitZ().cross(Vec3::unitX()), Vec3::unitY());
}

TEST(Vec3, NormalizedHasUnitLength) {
    const Vec3 v(2.0f, 0.0f, 0.0f);
    const Vec3 n = v.normalized();
    EXPECT_FLOAT_EQ(n.length(), 1.0f);
    EXPECT_EQ(n, Vec3::unitX());
}

TEST(Vec3, NormalizeMutatesInPlace) {
    Vec3 v(0.0f, 5.0f, 0.0f);
    v.normalize();
    EXPECT_EQ(v, Vec3::unitY());
}

TEST(Vec3, LerpEndpointsAndMidpoint) {
    const Vec3 a(0.0f, 0.0f, 0.0f);
    const Vec3 b(2.0f, 4.0f, 6.0f);
    EXPECT_EQ(Vec3::lerp(a, b, 0.0f), a);
    EXPECT_EQ(Vec3::lerp(a, b, 1.0f), b);
    EXPECT_EQ(Vec3::lerp(a, b, 0.5f), Vec3(1.0f, 2.0f, 3.0f));
}

TEST(Vec3, GlmRoundTrip) {
    const Vec3 v(1.5f, -2.5f, 3.5f);
    const glm::vec3 g = v.toGlm();
    EXPECT_EQ(Vec3::fromGlm(g), v);
}

TEST(Vec2, BasicOps) {
    const Vec2 a(1.0f, 2.0f);
    const Vec2 b(3.0f, 4.0f);
    EXPECT_EQ(a + b, Vec2(4.0f, 6.0f));
    EXPECT_FLOAT_EQ(a.dot(b), 11.0f);
    EXPECT_FLOAT_EQ(Vec2(3.0f, 4.0f).length(), 5.0f);
    EXPECT_EQ(Vec2(0.0f, 3.0f).normalized(), Vec2::unitY());
}

TEST(Vec4, ConstructAndXyzSlice) {
    const Vec4 v(1.0f, 2.0f, 3.0f, 4.0f);
    EXPECT_EQ(v.xyz(), Vec3(1.0f, 2.0f, 3.0f));
    EXPECT_FLOAT_EQ(v.w, 4.0f);
}

TEST(Vec4, ConstructFromVec3AndW) {
    const Vec4 v(Vec3(1.0f, 2.0f, 3.0f), 4.0f);
    EXPECT_EQ(v, Vec4(1.0f, 2.0f, 3.0f, 4.0f));
}

TEST(Vec4, ArithmeticAndLength) {
    const Vec4 a(1.0f, 0.0f, 0.0f, 0.0f);
    const Vec4 b(0.0f, 1.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(a.dot(b), 0.0f);
    EXPECT_FLOAT_EQ(Vec4(2.0f, 0.0f, 0.0f, 0.0f).length(), 2.0f);
}

TEST(Vec3Index, SubscriptReadsAndWritesAxes) {
    Vec3 v(1.0f, 2.0f, 3.0f);
    EXPECT_EQ(v[0], 1.0f);
    EXPECT_EQ(v[1], 2.0f);
    EXPECT_EQ(v[2], 3.0f);
    v[1] = 9.0f;
    EXPECT_EQ(v.y, 9.0f);
    const Vec3 c(4.0f, 5.0f, 6.0f);
    EXPECT_EQ(c[2], 6.0f);
}
