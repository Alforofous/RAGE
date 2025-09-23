#pragma once
#include "vector3.hpp"
#include "materials/renderable_interfaces.hpp"
#include "renderable_node3D.hpp"

/**
 * A voxel in 3D space.
 */
class Voxel3D : public RenderableNode3D,
    public IPositionable<Vector3>,
    public ISizable<Vector3>,
    public IColorable<Vector3> {
public:
    Voxel3D();
    ~Voxel3D();

    void render(Renderer &renderer) const override;

    Vector3 getPosition() const override { return this->position; }
    Vector3 getSize() const override { return this->size; }
    Vector3 getColor() const override { return this->color; }
    void setPosition(const Vector3 &position) { this->position = position; }
    void setColor(const Vector3 &color) { this->color = color; }
    void setSize(const Vector3 &size) { this->size = size; }
private:
    Vector3 position;
    Vector3 color;
    Vector3 size;
};