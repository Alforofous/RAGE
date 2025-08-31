#include "utils/shader_compiler.hpp"
#include <shaderc/shaderc.hpp>
#include <stdexcept>

std::string ShaderCompiler::LAST_ERROR;

std::vector<uint32_t> ShaderCompiler::compileGLSL(const std::string &source, shaderc_shader_kind kind, const std::string &entryPoint) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    // Set SPIR-V version to 1.4 for ray tracing support
    options.SetTargetSpirv(shaderc_spirv_version_1_4);

    // Set Vulkan environment to 1.2 (supports ray tracing)
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

    auto result = compiler.CompileGlslToSpv(source, kind, "shader.glsl", entryPoint.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        ShaderCompiler::LAST_ERROR = result.GetErrorMessage();
        throw std::runtime_error("Shader compilation failed: " + ShaderCompiler::LAST_ERROR);
    }

    return { result.cbegin(), result.cend() };
}

std::string ShaderCompiler::getLastError() {
    return ShaderCompiler::LAST_ERROR;
}