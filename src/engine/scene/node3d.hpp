#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>
#include "math/mat.hpp"
#include "math/quat.hpp"
#include "math/vec.hpp"

namespace RAGE {
    /**
     * Base scene-graph node: a transform plus a parent-children hierarchy.
     *
     * Ownership: parents own children via std::unique_ptr; children keep a raw Node3D* back to
     * their parent. Nodes are non-copyable and non-movable — children rely on the parent's
     * address being stable.
     *
     * Transform fields (position, rotation, scale) live directly on the node and are mutated
     * through setters that mark cached local and world matrices dirty. World-matrix invalidation
     * propagates down the subtree because every descendant's world transform composes with this
     * node's. Both matrices recompute on demand the next time they are read.
     *
     * Subclasses (Camera, RenderableNode3D, Voxel3D, …) inherit polymorphically; the destructor
     * is virtual so containers of base nodes destroy concrete subclasses correctly.
     *
     * @note Long-lived external references should be raw Node3D* for now; if dangling-pointer
     *       hazards appear (scripting, editor undo, hot reload), a typed NodeHandle wrapper can
     *       be added at that point. See `.claude/plans/core-architecture.md`.
     */
    class Node3D {
    public:
        Node3D() = default;
        virtual ~Node3D() = default;

        Node3D(const Node3D &) = delete;
        Node3D &operator=(const Node3D &) = delete;
        Node3D(Node3D &&) = delete;
        Node3D &operator=(Node3D &&) = delete;

        const Vec3 &position() const { return position_; }
        const Quat &rotation() const { return rotation_; }
        const Vec3 &scale() const { return scale_; }

        void setPosition(const Vec3 &p);
        void setRotation(const Quat &q);
        void setScale(const Vec3 &s);

        const Mat4 &localMatrix() const;
        const Mat4 &worldMatrix() const;

        Node3D &add(std::unique_ptr<Node3D> child);
        std::unique_ptr<Node3D> remove(Node3D *child);

        void clearChildren();

        Node3D *parent() const { return parent_; }
        std::span<const std::unique_ptr<Node3D>> children() const { return children_; }
        size_t childCount() const { return children_.size(); }

        /**
         * @brief Monotonic counter incremented on every mutation of this node or its
         *        subtree — child add/remove/clear and transform changes all bump it,
         *        propagating up the parent chain. Poll on the root to cheaply detect
         *        "did the scene change since last frame". Main-thread only.
         */
        uint64_t treeVersion() const { return treeVersion_; }

    private:
        void markLocalDirty() noexcept;
        void invalidateWorldRecursive() noexcept;
        void bumpTreeVersion() noexcept;

        Vec3 position_ = Vec3::zero();
        Quat rotation_ = Quat::identity();
        Vec3 scale_ = Vec3::one();

        uint64_t treeVersion_ = 0;
        mutable Mat4 localMatrix_ = Mat4::identity();
        mutable Mat4 worldMatrix_ = Mat4::identity();
        mutable bool localDirty_ = false;
        mutable bool worldDirty_ = false;

        std::vector<std::unique_ptr<Node3D>> children_;
        Node3D *parent_ = nullptr;
    };
}
