#pragma once
#include <string>
#include <map>
#include <memory>
#include <functional>
#include "glsl_shader.hpp"
#include "uniform.hpp"

// Forward declaration for renderable interface
class RenderableNode3D;

// Function type for setting uniforms in pipeline
using SetUniformFunction = std::function<void (const std::string &, const UniformBase &)>;

/**
 * Material is a base class for all materials.
 * It contains GLSL shaders and uniform data.
 */
class Material {
public:
    Material() = default;
    virtual ~Material() = default;

    // Shader management
    GLSLShader getShader(const std::string &name) const;
    void setShader(const std::string &name, const GLSLShader &shader);
    void removeShader(const std::string &name);
    void clearShaders();

    // Get all shaders for pipeline creation
    const std::map<std::string, GLSLShader>& getAllShaders() const { return this->shaders; }

    // Uniform management
    template<typename T>
    void setUniform(const std::string &name, const T &value) {
        this->uniforms[name] = std::make_unique<Uniform<T> >(value);
    }

    template<typename T>
    Uniform<T> *getUniform(const std::string &name) {
        auto it = this->uniforms.find(name);
        if (it != this->uniforms.end()) {
            return dynamic_cast<Uniform<T> *>(it->second.get());
        }

        return nullptr;
    }

    const std::map<std::string, std::unique_ptr<UniformBase> >& getAllUniforms() const {
        return this->uniforms;
    }

    virtual void onRenderSetup(SetUniformFunction setUniform, void *object) = 0;

private:
    std::map<std::string, GLSLShader> shaders;
    std::map<std::string, std::unique_ptr<UniformBase> > uniforms;
};