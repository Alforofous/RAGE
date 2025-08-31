#pragma once
#include "int_vector3.hpp"

/**
 * A voxel in 3D space.
 */
class Voxel3D : public RenderableNode3D {
public:
    Voxel3D();
    ~Voxel3D();

    void render(Renderer& renderer) const override;

    IntVector3 getPosition() const { return this->position; }
    IntVector3 getSize() const { return this->size; }
    IntVector3 getColor() const { return this->color; }
    void setPosition(const IntVector3& position) { this->position = position; }
    void setColor(const IntVector3& color) { this->color = color; }
    void setSize(const IntVector3& size) { this->size = size; }
private:
    IntVector3 position;
    IntVector3 color;
    IntVector3 size;
};