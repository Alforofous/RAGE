#include <gtest/gtest.h>
#include <memory>
#include <utility>
#include "engine/materials/material.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/renderable_node3d.hpp"
#include "gpu/gpu_types.hpp"
#include "math/vec.hpp"

using namespace RAGE;

namespace {
    class MockShader {
    public:
        ShaderStage stage() const { return ShaderStage::Compute; }
    };

    using TestRenderable = RenderableNode3D<MockShader>;
    using TestMaterial = Material<MockShader>;
}

TEST(RenderableNode3D, DefaultIsVisibleWithoutMaterial) {
    const TestRenderable r;
    EXPECT_TRUE(r.visible());
    EXPECT_FALSE(r.hasMaterial());
    EXPECT_EQ(r.material(), nullptr);
}

TEST(RenderableNode3D, ConstructWithMaterialStoresIt) {
    auto mat = std::make_shared<TestMaterial>(nullptr);
    const TestRenderable r(mat);
    EXPECT_TRUE(r.hasMaterial());
    EXPECT_EQ(r.material(), mat);
}

TEST(RenderableNode3D, SetMaterialReplacesIt) {
    auto matA = std::make_shared<TestMaterial>(nullptr);
    auto matB = std::make_shared<TestMaterial>(nullptr);
    TestRenderable r(matA);
    r.setMaterial(matB);
    EXPECT_EQ(r.material(), matB);
}

TEST(RenderableNode3D, SetVisibleToggles) {
    TestRenderable r;
    r.setVisible(false);
    EXPECT_FALSE(r.visible());
    r.setVisible(true);
    EXPECT_TRUE(r.visible());
}

TEST(RenderableNode3D, InheritsNode3DTransform) {
    TestRenderable r;
    r.setPosition(Vec3(4.0f, 5.0f, 6.0f));
    const Vec3 transformed = r.worldMatrix().transformPoint(Vec3::zero());
    EXPECT_FLOAT_EQ(transformed.x, 4.0f);
    EXPECT_FLOAT_EQ(transformed.y, 5.0f);
    EXPECT_FLOAT_EQ(transformed.z, 6.0f);
}

TEST(RenderableNode3D, CanBeAttachedAsChildOfNode3D) {
    Node3D rig;
    rig.setPosition(Vec3(10.0f, 0.0f, 0.0f));

    auto child = std::make_unique<TestRenderable>(std::make_shared<TestMaterial>(nullptr));
    TestRenderable *raw = child.get();
    rig.add(std::move(child));

    const Vec3 worldPos = raw->worldMatrix().transformPoint(Vec3::zero());
    EXPECT_FLOAT_EQ(worldPos.x, 10.0f);
    EXPECT_EQ(raw->parent(), &rig);
}

TEST(RenderableNode3D, SharesMaterialAcrossInstances) {
    auto mat = std::make_shared<TestMaterial>(nullptr);
    const TestRenderable a(mat);
    const TestRenderable b(mat);
    EXPECT_EQ(a.material().get(), b.material().get());
    EXPECT_EQ(mat.use_count(), 3);
}
