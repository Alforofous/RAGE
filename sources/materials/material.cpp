#include "materials/material.hpp"

GLSLShader Material::getShader(const std::string &name) const {
    return this->shaders.at(name);
}

void Material::setShader(const std::string &name, const GLSLShader &shader) {
    this->shaders.insert_or_assign(name, shader);
}

void Material::removeShader(const std::string &name) {
    this->shaders.erase(name);
}

void Material::clearShaders() {
    this->shaders.clear();
}