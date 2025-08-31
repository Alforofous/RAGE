#pragma once
#include <shaderc/shaderc.h>
#include <string>

struct GLSLShader {
    std::string source;
    shaderc_shader_kind kind;

    GLSLShader(std::string src, shaderc_shader_kind kind)
        : source(std::move(src)), kind(kind) {}
};