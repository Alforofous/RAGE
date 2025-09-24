#include "materials/voxel_ray_tracing_material.hpp"
#include "materials/renderable_interfaces.hpp"
#include "renderable_node3D.hpp"
#include "utils/file_reader.hpp"
#include <glm/glm.hpp>

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
    
    const auto *renderable = static_cast<const RenderableNode3D *>(object);
    const auto *positionable = dynamic_cast<const IPositionable<Vector3> *>(renderable);
    const auto *sizable = dynamic_cast<const ISizable<Vector3> *>(renderable);
    const auto *colorable = dynamic_cast<const IColorable<Vector3> *>(renderable);

    if (positionable != nullptr) {
        Vector3 pos = positionable->getPosition();
        cubeData.position = glm::vec3(pos.getX(), pos.getY(), pos.getZ());
    }

    if (sizable != nullptr) {
        Vector3 fullSize = sizable->getSize();
        cubeData.size = glm::vec3(fullSize.getX(), fullSize.getY(), fullSize.getZ());
    }

    if (colorable != nullptr) {
        Vector3 color = colorable->getColor();
        cubeData.color = glm::vec3(color.getX(), color.getY(), color.getZ());
    }


    CameraData cameraData{};
    cameraData.projInverse = camera->getProjection();
    cameraData.viewInverse = camera->getView();
    cameraData.cameraPos = camera->getPosition();

    setUniform(0, &cameraData, sizeof(cameraData));
    setUniform(1, &cubeData, sizeof(cubeData));
}