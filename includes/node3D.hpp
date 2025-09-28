#pragma once
#include <vector>
#include <memory>
#include "vector3.hpp"
#include "vector4.hpp"
#include "matrix4.hpp"
#include <functional>
#include "event_dispatcher.hpp"
#include "event_type.hpp"

class Node3D;

// Convenient type aliases
using Node3DRef = std::shared_ptr<Node3D>;
using Node3DWeakRef = std::weak_ptr<Node3D>;

DEFINE_EVENT_NAME(nodeadded);
DEFINE_EVENT_NAME(noderemoved);
DEFINE_EVENT_NAME(dispose);

using Node3DEventMap = EventMap<
    EventType<nodeadded, const Node3D *>,
    EventType<noderemoved, const Node3D *>,
    EventType<dispose, const Node3D *>
>;

/**
 * A node with 3D transform which can be used to represent a 3D object
 * with position, rotation, scale and parent-child relationship.
 * Uses shared_ptr for automatic reference-counted cleanup.
 */
class Node3D :
    public EventDispatcher<Node3DEventMap>,
    public std::enable_shared_from_this<Node3D> {
public:
    Node3D();
    Node3D(const Node3D &other);
    virtual ~Node3D();

    static Node3DRef create();

    void setMatrix(const Matrix4 &matrix);
    void setPosition(const Vector3 &position);
    void setRotation(const Vector3 &rotation);
    void setQuaternion(const Vector4 &quaternion);
    void setScale(const Vector3 &scale);
    void setParent(Node3DRef parent);
    void addChild(const Node3DRef &child);
    void removeChild(const Node3DRef &child);
    void removeFromParent();

    Vector3 getPosition() const;
    Vector3 getRotation() const;
    Vector4 getQuaternion() const;
    Vector3 getScale() const;
    Node3DRef getParent() const;
    const std::vector<Node3DRef>& getChildren() const;
    Matrix4 getTransform() const;
    void traverse(const std::function<void(Node3DRef)> &callback);
private:
    Node3DWeakRef parent;
    std::vector<Node3DRef> children;
    Matrix4 matrix = Matrix4(1.0f);
    Vector3 position = Vector3(0.0f);
    Vector4 quaternion = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    Vector3 scale = Vector3(1.0f);
    bool matrixNeedsUpdate = true;
};