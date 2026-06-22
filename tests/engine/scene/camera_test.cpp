#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <numbers>
#include "engine/scene/camera.hpp"
#include "engine/scene/node3d.hpp"
#include "math/quat.hpp"
#include "math/vec.hpp"

using namespace RAGE;

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;

    bool nearEqual(const Vec3 &a, const Vec3 &b, float eps = 1e-4f) {
        return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
    }
}

TEST(Camera, ConstructsWithParams) {
    const Camera cam(kPi * 0.25f, 16.0f / 9.0f, 0.1f, 100.0f);
    EXPECT_FLOAT_EQ(cam.fov(), kPi * 0.25f);
    EXPECT_FLOAT_EQ(cam.aspect(), 16.0f / 9.0f);
    EXPECT_FLOAT_EQ(cam.zNear(), 0.1f);
    EXPECT_FLOAT_EQ(cam.zFar(), 100.0f);
}

TEST(Camera, IsANode3DSoTransformsApply) {
    Camera cam(kPi * 0.5f, 1.0f, 0.1f, 100.0f);
    cam.setPosition(Vec3(5.0f, 6.0f, 7.0f));
    EXPECT_TRUE(nearEqual(cam.worldMatrix().transformPoint(Vec3::zero()), Vec3(5.0f, 6.0f, 7.0f)));
}

TEST(Camera, ViewMatrixIsInverseOfWorld) {
    Camera cam(kPi * 0.5f, 1.0f, 0.1f, 100.0f);
    cam.setPosition(Vec3(0.0f, 0.0f, 5.0f));
    const Mat4 view = cam.viewMatrix();
    EXPECT_TRUE(nearEqual(view.transformPoint(Vec3(0.0f, 0.0f, 5.0f)), Vec3::zero()));
}

TEST(Camera, ViewMatrixPlacesWorldOriginInFrontOfCamera) {
    Camera cam(kPi * 0.5f, 1.0f, 0.1f, 100.0f);
    cam.setPosition(Vec3(0.0f, 0.0f, 10.0f));
    const Mat4 view = cam.viewMatrix();
    const Vec3 worldOriginInCameraSpace = view.transformPoint(Vec3::zero());
    EXPECT_NEAR(worldOriginInCameraSpace.z, -10.0f, 1e-4f);
}

TEST(Camera, ProjectionMatrixCaches) {
    const Camera cam(kPi * 0.5f, 1.0f, 0.1f, 100.0f);
    const Mat4 *first = &cam.projectionMatrix();
    const Mat4 *second = &cam.projectionMatrix();
    EXPECT_EQ(first, second);
}

TEST(Camera, SetAspectInvalidatesProjection) {
    Camera cam(kPi * 0.5f, 1.0f, 0.1f, 100.0f);
    const Mat4 firstProj = cam.projectionMatrix();
    cam.setAspect(2.0f);
    const Mat4 secondProj = cam.projectionMatrix();
    EXPECT_NE(firstProj, secondProj);
    EXPECT_FLOAT_EQ(cam.aspect(), 2.0f);
}

TEST(Camera, SetProjectionUpdatesAllFields) {
    Camera cam(kPi * 0.5f, 1.0f, 0.1f, 100.0f);
    cam.setProjection(kPi * 0.25f, 2.0f, 1.0f, 500.0f);
    EXPECT_FLOAT_EQ(cam.fov(), kPi * 0.25f);
    EXPECT_FLOAT_EQ(cam.aspect(), 2.0f);
    EXPECT_FLOAT_EQ(cam.zNear(), 1.0f);
    EXPECT_FLOAT_EQ(cam.zFar(), 500.0f);
}

TEST(Camera, ViewProjectionEqualsProjTimesView) {
    Camera cam(kPi * 0.5f, 1.0f, 0.1f, 100.0f);
    cam.setPosition(Vec3(1.0f, 2.0f, 3.0f));
    cam.setRotation(Quat::fromAxisAngle(Vec3::unitY(), 0.5f));

    const Mat4 expected = cam.projectionMatrix() * cam.viewMatrix();
    EXPECT_EQ(cam.viewProjectionMatrix(), expected);
}

TEST(Camera, AttachedAsChildFollowsParent) {
    Node3D rig;
    rig.setPosition(Vec3(10.0f, 0.0f, 0.0f));

    auto camPtr = std::make_unique<Camera>(kPi * 0.5f, 1.0f, 0.1f, 100.0f);
    Camera *camRaw = camPtr.get();
    rig.add(std::move(camPtr));

    EXPECT_TRUE(nearEqual(camRaw->worldMatrix().transformPoint(Vec3::zero()), Vec3(10.0f, 0.0f, 0.0f)));
    const Mat4 view = camRaw->viewMatrix();
    EXPECT_NEAR(view.transformPoint(Vec3(10.0f, 0.0f, 0.0f)).length(), 0.0f, 1e-4f);
}

TEST(Camera, RigRotationRotatesViewDirection) {
    Node3D rig;
    rig.setRotation(Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.5f));

    auto camPtr = std::make_unique<Camera>(kPi * 0.5f, 1.0f, 0.1f, 100.0f);
    Camera *camRaw = camPtr.get();
    rig.add(std::move(camPtr));

    const Vec3 forwardInWorld = camRaw->worldMatrix().transformDirection(Vec3(0.0f, 0.0f, -1.0f));
    EXPECT_TRUE(nearEqual(forwardInWorld, Vec3(-1.0f, 0.0f, 0.0f)));
}
