#include "materials/voxel_ray_tracing_material.hpp"
#include "utils/file_reader.hpp"

VoxelRayTracingMaterial::VoxelRayTracingMaterial()
    : RayTracingMaterial(
        GLSLShader(FileUtils::readFile("shaders/voxel_ray_tracing_material.rgen"), ShaderKind::RAYGEN),
        GLSLShader(FileUtils::readFile("shaders/voxel_ray_tracing_material.rchit"), ShaderKind::CLOSEST_HIT),
        GLSLShader(FileUtils::readFile("shaders/voxel_ray_tracing_material.rmiss"), ShaderKind::MISS)
    ) {
}

VoxelRayTracingMaterial::~VoxelRayTracingMaterial() = default;