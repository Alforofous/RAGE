#include "materials/voxel_ray_tracing_material.hpp"
#include "materials/renderable_interfaces.hpp"
#include "utils/file_reader.hpp"

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

void VoxelRayTracingMaterial::onRenderSetup(SetUniformFunction setUniform, Camera *camera, void *object) {
    if (object == nullptr) {
        return;
    }

    struct CubeData {
        Vector3 position;
        Vector3 size;
        Vector3 color;
    };

    struct CameraData {
        Matrix4 projInverse;
        Matrix4 viewInverse;
        Vector3 cameraPos;
    };

    CubeData cubeData{};
    const auto *positionable = dynamic_cast<const IPositionable<Vector3> *>(static_cast<IPositionable<Vector3> *>(object));
    if (positionable != nullptr) {
        cubeData.position = positionable->getPosition();
    }

    const auto *sizable = dynamic_cast<const ISizable<Vector3> *>(static_cast<ISizable<Vector3> *>(object));
    if (sizable != nullptr) {
        cubeData.size = sizable->getSize();
    }

    const auto *colorable = dynamic_cast<const IColorable<Vector3> *>(static_cast<IColorable<Vector3> *>(object));
    if (colorable != nullptr) {
        cubeData.color = colorable->getColor();
    }

    CameraData cameraData{};
    cameraData.projInverse = camera->getProjection();
    cameraData.viewInverse = camera->getView();
    cameraData.cameraPos = camera->getPosition();

    Uniform<CameraData> cameraUniform(cameraData);
    setUniform("camera", static_cast<const UniformBase &>(cameraUniform));

    Uniform<CubeData> cubeUniform(cubeData);
    setUniform("cube", static_cast<const UniformBase &>(cubeUniform));
}