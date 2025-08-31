#pragma once
#include "node3D.hpp"

/**
 * A scene is a collection of nodes.
 * It is responsible for updating and rendering the nodes.
 */
class Scene : public Node3D {
public:
    Scene();
    ~Scene();
};