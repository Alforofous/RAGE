#pragma once
#include <string>
#include "material.hpp"
#include "glsl_shader.hpp"

const std::string RAYGEN_NAME = "raygen";
const std::string CLOSEST_HIT_NAME = "closestHit";
const std::string MISS_NAME = "miss";
const std::string ANY_HIT_NAME = "anyHit";
const std::string INTERSECTION_NAME = "intersection";

class RayTracingMaterial : public Material {
public:
    RayTracingMaterial(
        const GLSLShader &raygenShader,
        const GLSLShader &closestHitShader,
        const GLSLShader &missShader,
        const GLSLShader *anyHitShader = nullptr,
        const GLSLShader *intersectionShader = nullptr
    );

    GLSLShader getRaygenShader() const;
    GLSLShader getClosestHitShader() const;
    GLSLShader getMissShader() const;
};