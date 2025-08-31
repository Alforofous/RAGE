#pragma once
#include <vector>
#include "vector3.hpp"
#include "vector4.hpp"
#include "matrix4.hpp"
#include <functional>

/**
 * A node with 3D transform which can be used to represent a 3D object
 * with position, rotation, scale and parent-child relationship.
 */
class Node3D {
public:
    Node3D();
    virtual ~Node3D();

    void setMatrix(const Matrix4& matrix);
    void setPosition(const Vector3& position);
    void setRotation(const Vector3& rotation);
    void setQuaternion(const Vector4& quaternion);
    void setScale(const Vector3& scale);
    void setParent(Node3D *parent);
    void addChild(Node3D *child);
    void removeChild(Node3D *child);
    void removeFromParent();

    Vector3 getPosition() const;
    Vector3 getRotation() const;
    Vector4 getQuaternion() const;
    Vector3 getScale() const;
    Node3D *getParent() const;
    const std::vector<Node3D *>& getChildren() const;
    Matrix4 getTransform() const;
    void traverse(const std::function<void(const Node3D *)>& callback) const;
private:
    Node3D *parent = nullptr;
    std::vector<Node3D *> children;
    Matrix4 matrix = Matrix4(1.0f);
    Vector3 position = Vector3(0.0f);
    Vector4 quaternion = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    Vector3 scale = Vector3(1.0f);
    bool matrixNeedsUpdate = true;
};