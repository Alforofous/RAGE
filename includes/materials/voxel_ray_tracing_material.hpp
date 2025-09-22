#pragma once
#include "ray_tracing_material.hpp"

class VoxelRayTracingMaterial : public RayTracingMaterial {
public:
    VoxelRayTracingMaterial();
    ~VoxelRayTracingMaterial();

    void onRenderSetup(VulkanPipeline *pipeline, Camera *camera, void *object) override;
};