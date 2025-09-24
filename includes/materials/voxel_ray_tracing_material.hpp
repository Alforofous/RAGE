#pragma once
#include "ray_tracing_material.hpp"

class VoxelRayTracingMaterial : public RayTracingMaterial {
public:
    VoxelRayTracingMaterial();
    ~VoxelRayTracingMaterial();

    void onRenderSetup(const SetUniform &setUniform, const Camera *camera, const void *object) const override;
};