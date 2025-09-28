#pragma once
#include "renderable_node3D.hpp"

/**
 * A voxel in 3D space.
 */
class Voxel3D : public RenderableNode3D {
public:
    Voxel3D();
    Voxel3D(const Voxel3D &other);
    ~Voxel3D();

    static std::shared_ptr<Voxel3D> create();

    void render(Renderer &renderer) const override;

    void setColor(const Vector3 &color) { this->getMaterial()->setUniform("color", color); }
};