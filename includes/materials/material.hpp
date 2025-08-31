#pragma once
#include <string>
#include <map>
#include "glsl_shader.hpp"

/**
 * Material is a base class for all materials.
 * It contains a map of GLSL shaders.
 */
class Material {
public:
    Material() = default;
    virtual ~Material() = default;
    
    GLSLShader getShader(const std::string& name) const;
    void setShader(const std::string& name, const GLSLShader& shader);
    void removeShader(const std::string& name);
    void clearShaders();
    
    // Get all shaders for pipeline creation
    const std::map<std::string, GLSLShader>& getAllShaders() const { return this->shaders; }
    
private:
    std::map<std::string, GLSLShader> shaders;
};