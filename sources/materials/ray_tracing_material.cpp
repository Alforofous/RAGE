#include "materials/ray_tracing_material.hpp"

RayTracingMaterial::RayTracingMaterial(
    const GLSLShader &raygenShader,
    const GLSLShader &closestHitShader,
    const GLSLShader &missShader,
    const GLSLShader *anyHitShader,
    const GLSLShader *intersectionShader) {
    this->setShader(RAYGEN_NAME, raygenShader);
    this->setShader(CLOSEST_HIT_NAME, closestHitShader);
    this->setShader(MISS_NAME, missShader);

    if (anyHitShader != nullptr) {
        this->setShader(ANY_HIT_NAME, *anyHitShader);
    }
    if (intersectionShader != nullptr) {
        this->setShader(INTERSECTION_NAME, *intersectionShader);
    }
}

GLSLShader RayTracingMaterial::getRaygenShader() const {
    return this->getShader(RAYGEN_NAME);
}

GLSLShader RayTracingMaterial::getClosestHitShader() const {
    return this->getShader(CLOSEST_HIT_NAME);
}

GLSLShader RayTracingMaterial::getMissShader() const {
    return this->getShader(MISS_NAME);
}