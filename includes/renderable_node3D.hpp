#pragma once
#include "node3D.hpp"
#include "renderer.hpp"
#include "materials/material.hpp"

/**
 * A node that can be rendered.
 */
class RenderableNode3D : public Node3D {
public:
    RenderableNode3D(Material *material);
    virtual ~RenderableNode3D();

    virtual void render(Renderer& renderer) const = 0;
    Material* getMaterial() const { return material; }

protected:
    Material *material;
};