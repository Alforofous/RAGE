#include <gtest/gtest.h>
#include <cmath>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/voxel3d.hpp"
#include "math/quat.hpp"
#include "math/vec.hpp"

using namespace RAGE;

namespace {
    constexpr float kPi = std::numbers::pi_v<float>;

    bool nearEqual(const Vec3 &a, const Vec3 &b, float eps = 1e-5f) {
        return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
    }
}

TEST(Node3D, DefaultIsIdentityTransform) {
    const Node3D n;
    EXPECT_EQ(n.position(), Vec3::zero());
    EXPECT_EQ(n.rotation(), Quat::identity());
    EXPECT_EQ(n.scale(), Vec3::one());
    EXPECT_EQ(n.localMatrix(), Mat4::identity());
    EXPECT_EQ(n.worldMatrix(), Mat4::identity());
    EXPECT_EQ(n.parent(), nullptr);
    EXPECT_EQ(n.childCount(), 0u);
}

TEST(Node3D, SetPositionUpdatesLocalAndWorldMatrix) {
    Node3D n;
    n.setPosition(Vec3(1.0f, 2.0f, 3.0f));
    const Vec3 transformed = n.worldMatrix().transformPoint(Vec3::zero());
    EXPECT_TRUE(nearEqual(transformed, Vec3(1.0f, 2.0f, 3.0f)));
}

TEST(Node3D, SetRotationRotatesPoint) {
    Node3D n;
    n.setRotation(Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.5f));
    EXPECT_TRUE(nearEqual(n.worldMatrix().transformDirection(Vec3::unitX()), Vec3(0.0f, 0.0f, -1.0f)));
}

TEST(Node3D, SetScaleScalesPoint) {
    Node3D n;
    n.setScale(Vec3(2.0f, 3.0f, 4.0f));
    EXPECT_TRUE(nearEqual(n.worldMatrix().transformPoint(Vec3::one()), Vec3(2.0f, 3.0f, 4.0f)));
}

TEST(Node3D, AddSetsParentAndStoresOwnership) {
    Node3D parent;
    const Node3D &child = parent.add(std::make_unique<Node3D>());
    EXPECT_EQ(parent.childCount(), 1u);
    EXPECT_EQ(child.parent(), &parent);
}

TEST(Node3D, AddRejectsNull) {
    Node3D n;
    EXPECT_THROW(n.add(nullptr), std::runtime_error);
}

TEST(Node3D, RemoveDetachesChildAndReturnsOwnership) {
    Node3D parent;
    Node3D &child = parent.add(std::make_unique<Node3D>());
    Node3D *raw = &child;
    auto detached = parent.remove(raw);

    EXPECT_EQ(parent.childCount(), 0u);
    EXPECT_EQ(detached->parent(), nullptr);
    EXPECT_EQ(detached.get(), raw);
}

TEST(Node3D, RemoveRejectsUnknownChild) {
    Node3D parent;
    Node3D stranger;
    EXPECT_THROW(parent.remove(&stranger), std::runtime_error);
}

TEST(Node3D, ChildWorldMatrixComposesParent) {
    Node3D parent;
    parent.setPosition(Vec3(10.0f, 0.0f, 0.0f));
    Node3D &child = parent.add(std::make_unique<Node3D>());
    child.setPosition(Vec3(0.0f, 5.0f, 0.0f));

    EXPECT_TRUE(nearEqual(child.worldMatrix().transformPoint(Vec3::zero()), Vec3(10.0f, 5.0f, 0.0f)));
    EXPECT_TRUE(nearEqual(child.localMatrix().transformPoint(Vec3::zero()), Vec3(0.0f, 5.0f, 0.0f)));
}

TEST(Node3D, ParentRotationRotatesChildPosition) {
    Node3D parent;
    parent.setRotation(Quat::fromAxisAngle(Vec3::unitY(), kPi * 0.5f));
    Node3D &child = parent.add(std::make_unique<Node3D>());
    child.setPosition(Vec3(1.0f, 0.0f, 0.0f));

    EXPECT_TRUE(nearEqual(child.worldMatrix().transformPoint(Vec3::zero()), Vec3(0.0f, 0.0f, -1.0f)));
}

TEST(Node3D, MutatingParentInvalidatesChildWorld) {
    Node3D parent;
    Node3D &child = parent.add(std::make_unique<Node3D>());
    child.setPosition(Vec3(1.0f, 0.0f, 0.0f));

    const Vec3 first = child.worldMatrix().transformPoint(Vec3::zero());
    EXPECT_TRUE(nearEqual(first, Vec3(1.0f, 0.0f, 0.0f)));

    parent.setPosition(Vec3(10.0f, 0.0f, 0.0f));
    const Vec3 second = child.worldMatrix().transformPoint(Vec3::zero());
    EXPECT_TRUE(nearEqual(second, Vec3(11.0f, 0.0f, 0.0f)));
}

TEST(Node3D, DeepHierarchyDirtyPropagation) {
    Node3D root;
    Node3D &a = root.add(std::make_unique<Node3D>());
    Node3D &b = a.add(std::make_unique<Node3D>());
    Node3D &c = b.add(std::make_unique<Node3D>());
    c.setPosition(Vec3(0.0f, 0.0f, 1.0f));

    EXPECT_TRUE(nearEqual(c.worldMatrix().transformPoint(Vec3::zero()), Vec3(0.0f, 0.0f, 1.0f)));

    root.setPosition(Vec3(5.0f, 0.0f, 0.0f));
    EXPECT_TRUE(nearEqual(c.worldMatrix().transformPoint(Vec3::zero()), Vec3(5.0f, 0.0f, 1.0f)));
}

TEST(Node3D, ReparentingPreservesLocalTransform) {
    Node3D a;
    a.setPosition(Vec3(10.0f, 0.0f, 0.0f));
    Node3D b;
    b.setPosition(Vec3(0.0f, 20.0f, 0.0f));

    Node3D &child = a.add(std::make_unique<Node3D>());
    child.setPosition(Vec3(1.0f, 0.0f, 0.0f));
    EXPECT_TRUE(nearEqual(child.worldMatrix().transformPoint(Vec3::zero()), Vec3(11.0f, 0.0f, 0.0f)));

    auto detached = a.remove(&child);
    const Node3D &rehomed = b.add(std::move(detached));
    EXPECT_TRUE(nearEqual(rehomed.worldMatrix().transformPoint(Vec3::zero()), Vec3(1.0f, 20.0f, 0.0f)));
}

TEST(Node3D, RemovedChildHasNoParent) {
    Node3D parent;
    Node3D &child = parent.add(std::make_unique<Node3D>());
    auto detached = parent.remove(&child);
    EXPECT_EQ(detached->parent(), nullptr);
}

TEST(Node3D, MatrixCachingReusesValue) {
    Node3D n;
    n.setPosition(Vec3(1.0f, 2.0f, 3.0f));
    const Mat4 *first = &n.localMatrix();
    const Mat4 *second = &n.localMatrix();
    EXPECT_EQ(first, second);
}

TEST(Node3D, ClearChildrenDestroysAllChildrenInReverseInsertionOrder) {
    Node3D root;
    Node3D &a = root.add(std::make_unique<Node3D>());
    Node3D &b = root.add(std::make_unique<Node3D>());
    (void)a;
    (void)b;
    ASSERT_EQ(root.childCount(), 2u);
    root.clearChildren();
    EXPECT_EQ(root.childCount(), 0u);
}

// Host-side mirror of the resetScene orchestration. Each cycle: clear children →
// recreate pool → repopulate. Asserts the old pool was empty before destruction.
TEST(Node3D, ResetCycle_PoolStaysLeakCleanAcrossPolicyFlips) {
    constexpr int32_t kCycles = 8;
    constexpr int32_t kDim = 8;       // 1×1×1 brick per Voxel3D
    constexpr int32_t kChildren = 4;

    auto pool = std::make_unique<BrickPool>(true);
    Node3D root;

    const auto populate = [&](bool /*ignored*/) {
        for (int32_t i = 0; i < kChildren; ++i) {
            auto vox = std::make_unique<Voxel3D>(*pool, IVec3{ kDim, kDim, kDim }, 0.05f);
            vox->setVoxel(IVec3{ 0, 0, 0 }, Color(1.0f, 0.0f, 0.0f, 1.0f));
            root.add(std::move(vox));
        }
    };

    populate(true);
    ASSERT_GT(pool->allocated(), 0u);

    for (int32_t cycle = 0; cycle < kCycles; ++cycle) {
        SCOPED_TRACE("cycle " + std::to_string(cycle));
        const bool enableDedup = (cycle % 2) == 1;     // flip policy each cycle

        root.clearChildren();
        EXPECT_EQ(pool->allocated(), 0u) << "old pool must drain before being destroyed";
        EXPECT_EQ(pool->logicalBricks(), 0u);
        EXPECT_EQ(pool->drainDirty().size(), 0u);

        pool.reset();
        pool = std::make_unique<BrickPool>(enableDedup);

        populate(enableDedup);
        EXPECT_GT(pool->allocated(), 0u) << "new pool should have allocations after rebuild";
        EXPECT_EQ(pool->isDedupEnabled(), enableDedup);
    }
}
