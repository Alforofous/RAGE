#include "node3d.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace RAGE {
    void Node3D::setPosition(const Vec3 &p) {
        position_ = p;
        markLocalDirty();
    }

    void Node3D::setRotation(const Quat &q) {
        rotation_ = q;
        markLocalDirty();
    }

    void Node3D::setScale(const Vec3 &s) {
        scale_ = s;
        markLocalDirty();
    }

    const Mat4 &Node3D::localMatrix() const {
        if (localDirty_) {
            localMatrix_ = Mat4::fromTRS(position_, rotation_, scale_);
            localDirty_ = false;
        }

        return localMatrix_;
    }

    const Mat4 &Node3D::worldMatrix() const {
        if (worldDirty_) {
            if (parent_ != nullptr) {
                worldMatrix_ = parent_->worldMatrix() * localMatrix();
            } else {
                worldMatrix_ = localMatrix();
            }
            worldDirty_ = false;
        }

        return worldMatrix_;
    }

    Node3D &Node3D::add(std::unique_ptr<Node3D> child) {
        if (child == nullptr) {
            throw std::runtime_error("Node3D::add: null child");
        }
        if (child.get() == this) {
            throw std::runtime_error("Node3D::add: cannot add node as child of itself");
        }
        if (child->parent_ != nullptr) {
            throw std::runtime_error("Node3D::add: child already has a parent; remove() first");
        }

        child->parent_ = this;
        child->invalidateWorldRecursive();
        children_.push_back(std::move(child));

        return *children_.back();
    }

    void Node3D::clearChildren() {
        children_.clear();
    }

    std::unique_ptr<Node3D> Node3D::remove(Node3D *child) {
        if (child == nullptr) {
            throw std::runtime_error("Node3D::remove: null child");
        }

        const auto it =
            std::ranges::find_if(children_, [child](const std::unique_ptr<Node3D> &c) { return c.get() == child; });
        if (it == children_.end()) {
            throw std::runtime_error("Node3D::remove: child not found in this node");
        }

        std::unique_ptr<Node3D> detached = std::move(*it);
        children_.erase(it);
        detached->parent_ = nullptr;
        detached->invalidateWorldRecursive();

        return detached;
    }

    void Node3D::markLocalDirty() noexcept {
        localDirty_ = true;
        invalidateWorldRecursive();
    }

    void Node3D::invalidateWorldRecursive() noexcept {
        worldDirty_ = true;
        for (const std::unique_ptr<Node3D> &c : children_) {
            c->invalidateWorldRecursive();
        }
    }
}
