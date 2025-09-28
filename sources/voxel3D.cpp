#include "voxel3D.hpp"
#include "materials/voxel_ray_tracing_material.hpp"

Voxel3D::Voxel3D() : RenderableNode3D(new VoxelRayTracingMaterial()) {
}

Voxel3D::Voxel3D(const Voxel3D &other) = default;

Voxel3D::~Voxel3D() = default;

std::shared_ptr<Voxel3D> Voxel3D::create() {
    return std::make_shared<Voxel3D>();
}

void Voxel3D::render(Renderer &renderer) const {
    RenderableNode3D::render(renderer);
}