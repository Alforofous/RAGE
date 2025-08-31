#pragma once
#include <vector>
#include <string>
#include <shaderc/shaderc.h>

class ShaderCompiler {
public:
    static std::vector<uint32_t> compileGLSL(const std::string &source, shaderc_shader_kind kind, const std::string &entryPoint = "main");
    static std::string getLastError();
private:
    static std::string LAST_ERROR;
};