#include "node3D.hpp"

Node3D::Node3D() = default;

Node3D::~Node3D() {
    this->removeFromParent();
    
    // Make a copy of children pointers since removeChild modifies the vector
    std::vector<Node3D*> childrenCopy = this->children;
    for (Node3D *child : childrenCopy) {
        this->removeChild(child);
    }
    this->children.clear();
}

void Node3D::setPosition(const Vector3& position) {
    this->position = position;
    this->matrixNeedsUpdate = true;
}

void Node3D::setRotation(const Vector3& rotation) {
    // Convert Euler angles to quaternion if needed
    this->quaternion = Vector4(rotation.getX(), rotation.getY(), rotation.getZ(), 1.0f);
    this->matrixNeedsUpdate = true;
}

void Node3D::removeFromParent() {
    if (this->parent != nullptr) {
        this->parent->removeChild(this);
    }
}

void Node3D::addChild(Node3D *child) {
    child->parent = this;
    this->children.push_back(child);
}

void Node3D::removeChild(Node3D *child) {
    if (child == nullptr) {
        return;
    }

    if (child->parent == this) {
        child->parent = nullptr;
    }

    auto it = std::find(this->children.begin(), this->children.end(), child);
    if (it != this->children.end()) {
        this->children.erase(it);
    }
}

void Node3D::traverse(const std::function<void(const Node3D *)>& callback) const {
    callback(this);
    for (Node3D *child : this->children) {
        child->traverse(callback);
    }
}