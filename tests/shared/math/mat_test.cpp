#include <gtest/gtest.h>
#include <cmath>
#include <numbers>
#include "math/mat.hpp"
#include "math/quat.hpp"
#include "math/vec.hpp"

using namespace RAGE;

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;

    bool nearEqual(const Vec3 &a, const Vec3 &b, float eps = 1e-5f) {
        return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
    }
    bool nearEqual(const Vec4 &a, const Vec4 &b, float eps = 1e-5f) {
        return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps &&
               std::fabs(a.w - b.w) < eps;
    }
}

TEST(Mat4, DefaultIsIdentity) {
    const Mat4 m;
    EXPECT_EQ(m, Mat4::identity());
}

TEST(Mat4, TranslationMovesPoint) {
    const Mat4 m = Mat4::translation(Vec3(5.0f, 6.0f, 7.0f));
    EXPECT_TRUE(nearEqual(m.transformPoint(Vec3(1.0f, 1.0f, 1.0f)), Vec3(6.0f, 7.0f, 8.0f)));
}

TEST(Mat4, TranslationDoesNotMoveDirection) {
    const Mat4 m = Mat4::translation(Vec3(5.0f, 6.0f, 7.0f));
    EXPECT_TRUE(nearEqual(m.transformDirection(Vec3::unitX()), Vec3::unitX()));
}

TEST(Mat4, ScaleScalesPoint) {
    const Mat4 m = Mat4::scale(Vec3(2.0f, 3.0f, 4.0f));
    EXPECT_TRUE(nearEqual(m.transformPoint(Vec3(1.0f, 1.0f, 1.0f)), Vec3(2.0f, 3.0f, 4.0f)));
}

TEST(Mat4, RotationFromQuatMatchesQuatRotate) {
    const Quat q = Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.5f);
    const Mat4 m = Mat4::rotation(q);
    const Vec3 viaMatrix = m.transformDirection(Vec3::unitX());
    const Vec3 viaQuat = q.rotate(Vec3::unitX());
    EXPECT_TRUE(nearEqual(viaMatrix, viaQuat));
}

TEST(Mat4, FromTRSAppliesScaleThenRotateThenTranslate) {
    const Vec3 t(10.0f, 0.0f, 0.0f);
    const Quat r = Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.5f);
    const Vec3 s(2.0f, 2.0f, 2.0f);
    const Mat4 m = Mat4::fromTRS(t, r, s);
    const Vec3 transformed = m.transformPoint(Vec3::unitX());
    EXPECT_TRUE(nearEqual(transformed, Vec3(10.0f, 0.0f, -2.0f)));
}

TEST(Mat4, InverseTimesOriginalIsIdentity) {
    const Mat4 m =
        Mat4::fromTRS(Vec3(1.0f, 2.0f, 3.0f), Quat::fromAxisAngle(Vec3::unitY(), 0.7f), Vec3(2.0f, 2.0f, 2.0f));
    const Mat4 product = m * m.inverted();
    for (int i = 0; i < 4; ++i) {
        const Vec4 col = product.column(i);
        Vec4 expected = Vec4::zero();
        switch (i) {
            case 0:
                expected = Vec4(1.0f, 0.0f, 0.0f, 0.0f);
                break;
            case 1:
                expected = Vec4(0.0f, 1.0f, 0.0f, 0.0f);
                break;
            case 2:
                expected = Vec4(0.0f, 0.0f, 1.0f, 0.0f);
                break;
            case 3:
                expected = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
                break;
            default:
                break;
        }
        EXPECT_TRUE(nearEqual(col, expected));
    }
}

TEST(Mat4, LookAtFromOriginLooksAtTarget) {
    const Mat4 m = Mat4::lookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3::zero(), Vec3::unitY());
    const Vec3 target = m.transformPoint(Vec3::zero());
    EXPECT_NEAR(target.z, -5.0f, 1e-5f);
}

TEST(Mat4, MultiplicationCombinesTransforms) {
    const Mat4 t = Mat4::translation(Vec3(1.0f, 0.0f, 0.0f));
    const Mat4 s = Mat4::scale(Vec3(2.0f, 2.0f, 2.0f));
    const Mat4 combined = t * s;
    EXPECT_TRUE(nearEqual(combined.transformPoint(Vec3::unitX()), Vec3(3.0f, 0.0f, 0.0f)));
}

TEST(Mat4, ToMat3StripsTranslation) {
    const Mat4 m = Mat4::translation(Vec3(10.0f, 20.0f, 30.0f));
    const Mat3 m3 = m.toMat3();
    EXPECT_TRUE(nearEqual(m3 * Vec3::unitX(), Vec3::unitX()));
}

TEST(Mat3, IdentityRotatesNothing) {
    const Mat3 m = Mat3::identity();
    EXPECT_TRUE(nearEqual(m * Vec3(1.0f, 2.0f, 3.0f), Vec3(1.0f, 2.0f, 3.0f)));
}

TEST(Mat3, FromQuatMatchesQuatRotate) {
    const Quat q = Quat::fromAxisAngle(Vec3::unitZ(), kPi * 0.5f);
    const Mat3 m = Mat3::fromQuat(q);
    EXPECT_TRUE(nearEqual(m * Vec3::unitX(), q.rotate(Vec3::unitX())));
}

TEST(Mat3, TransposedInvertsOrthonormal) {
    const Mat3 r = Mat3::fromQuat(Quat::fromAxisAngle(Vec3::unitY(), 0.5f));
    const Mat3 product = r * r.transposed();
    EXPECT_TRUE(nearEqual(product * Vec3::unitX(), Vec3::unitX()));
}
