#include "voxel3D.hpp"
#include "materials/voxel_ray_tracing_material.hpp"

namespace {
    constexpr float DEFAULT_COLOR_VALUE = 1.0f;
    constexpr float DEFAULT_SIZE = 1.0f;
}

Voxel3D::Voxel3D() : RenderableNode3D(new VoxelRayTracingMaterial()), position(0.0f), size(DEFAULT_SIZE), color(DEFAULT_COLOR_VALUE) {
}

Voxel3D::Voxel3D(const Voxel3D &other)
    : RenderableNode3D(other),
    position(other.position),
    size(other.size),
    color(other.color) {
}

Voxel3D::~Voxel3D() {}

void Voxel3D::render(Renderer &renderer) const {
    RenderableNode3D::render(renderer);
}