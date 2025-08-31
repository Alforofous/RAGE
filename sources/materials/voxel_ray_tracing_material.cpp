#include "materials/voxel_ray_tracing_material.hpp"
#include "utils/file_reader.hpp"

VoxelRayTracingMaterial::VoxelRayTracingMaterial()
    : RayTracingMaterial(
        GLSLShader(FileUtils::readFile("shaders/voxel_ray_tracing_material.rgen"), shaderc_raygen_shader),
        GLSLShader(FileUtils::readFile("shaders/voxel_ray_tracing_material.rchit"), shaderc_closesthit_shader),
        GLSLShader(FileUtils::readFile("shaders/voxel_ray_tracing_material.rmiss"), shaderc_miss_shader)
    ) {
}

VoxelRayTracingMaterial::~VoxelRayTracingMaterial() = default;