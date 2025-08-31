#include "voxel3D.hpp"
#include "materials/voxel_ray_tracing_material.hpp"

namespace {
    constexpr int DEFAULT_COLOR_VALUE = 255;
}

Voxel3D::Voxel3D() : RenderableNode3D(new VoxelRayTracingMaterial()), position(0), size(1), color(DEFAULT_COLOR_VALUE) {
}
Voxel3D::~Voxel3D() {}

void Voxel3D::render(Renderer& renderer) const {
    RenderableNode3D::render(renderer);
}