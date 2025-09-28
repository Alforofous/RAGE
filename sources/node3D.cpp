#include "node3D.hpp"

Node3D::Node3D() = default;

Node3DRef Node3D::create() {
    return std::make_shared<Node3D>();
}

Node3D::~Node3D() {
    this->dispatchEvent<dispose>(this);
    this->children.clear();
}

Node3D::Node3D(const Node3D &other) = default;

void Node3D::setPosition(const Vector3 &position) {
    this->position = position;
    this->matrixNeedsUpdate = true;
}

void Node3D::setRotation(const Vector3 &rotation) {
    // Convert Euler angles to quaternion if needed
    this->quaternion = Vector4(rotation.getX(), rotation.getY(), rotation.getZ(), 1.0f);
    this->matrixNeedsUpdate = true;
}

void Node3D::removeFromParent() {
    if (auto parent = this->parent.lock()) {
        parent->removeChild(shared_from_this());
    }
}

void Node3D::addChild(const Node3DRef &child) {
    child->parent = shared_from_this();
    this->children.push_back(child);
    this->dispatchEvent<nodeadded>(child.get());
}

void Node3D::removeChild(const Node3DRef &child) {
    if (!child) {
        return;
    }

    child->parent.reset();

    auto it = std::find(this->children.begin(), this->children.end(), child);
    if (it != this->children.end()) {
        this->children.erase(it);
        this->dispatchEvent<noderemoved>(child.get());
    }
}

/** Traverse the node and all its children recursively */
void Node3D::traverse(const std::function<void(Node3DRef)> &callback) {
    callback(shared_from_this());
    for (auto &child : this->children) {
        child->traverse(callback);
    }
}

Vector3 Node3D::getPosition() const {
    return this->position;
}

Vector3 Node3D::getScale() const {
    return this->scale;
}

void Node3D::setScale(const Vector3 &scale) {
    this->scale = scale;
    this->matrixNeedsUpdate = true;
}

const std::vector<Node3DRef>& Node3D::getChildren() const {
    return this->children;
}

Node3DRef Node3D::getParent() const {
    return this->parent.lock();
}