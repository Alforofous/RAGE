#include "materials/voxel_ray_tracing_material.hpp"
#include "utils/file_reader.hpp"
#include <glm/glm.hpp>
#include <iostream>

namespace {
    constexpr float COLOR_NORMALIZATION_FACTOR = 255.0f;
}

VoxelRayTracingMaterial::VoxelRayTracingMaterial()
    : RayTracingMaterial(
        GLSLShader(FileUtils::readFile("shaders/voxel_ray_tracing_material.rgen"), ShaderKind::RAYGEN),
        GLSLShader(FileUtils::readFile("shaders/voxel_ray_tracing_material.rchit"), ShaderKind::CLOSEST_HIT),
        GLSLShader(FileUtils::readFile("shaders/voxel_ray_tracing_material.rmiss"), ShaderKind::MISS)
    ) {
}

VoxelRayTracingMaterial::~VoxelRayTracingMaterial() = default;

void VoxelRayTracingMaterial::onRenderSetup(const SetUniform &setUniform, const Camera *camera, const void *object) const {
    if (object == nullptr) {
        return;
    }

    struct CubeData {
        glm::vec3 position;
        float pad1;
        glm::vec3 size;
        float pad2;
        glm::vec3 color;
        float pad3;
    };

    struct CameraData {
        Matrix4 projInverse;
        Matrix4 viewInverse;
        Vector3 cameraPos;
    };

    CubeData cubeData{};
    
    const auto *node = static_cast<const Node3D *>(object);

    if (node != nullptr) {
        Vector3 pos = node->getPosition();
        cubeData.position = glm::vec3(pos.getX(), pos.getY(), pos.getZ());
        
        Vector3 fullSize = node->getScale();
        cubeData.size = glm::vec3(fullSize.getX(), fullSize.getY(), fullSize.getZ());
    }

    // For now, use different colors based on position to distinguish voxels
    if (cubeData.position.x > 0) {
        cubeData.color = glm::vec3(1.0, 0.0, 0.0);  // Red for positive X
    } else {
        cubeData.color = glm::vec3(0.0, 1.0, 0.0);  // Green for negative X
    }


    CameraData cameraData{};
    cameraData.projInverse = camera->getProjection();
    cameraData.viewInverse = camera->getView();
    cameraData.cameraPos = camera->getPosition();

    setUniform(0, &cameraData, sizeof(cameraData));
    setUniform(1, &cubeData, sizeof(cubeData));
}