#pragma once
#include "utils/shader_compiler.hpp"
#include <string>

struct GLSLShader {
    std::string source;
    ShaderKind kind;

    GLSLShader(std::string src, ShaderKind kind)
        : source(std::move(src)), kind(kind) {}
};