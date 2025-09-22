#pragma once
#include "ray_tracing_material.hpp"

class VoxelRayTracingMaterial : public RayTracingMaterial {
public:
    VoxelRayTracingMaterial();
    ~VoxelRayTracingMaterial();

    void onRenderSetup(SetUniformFunction setUniform, Camera *camera, void *object) override;
};