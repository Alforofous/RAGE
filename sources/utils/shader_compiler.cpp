#include "utils/shader_compiler.hpp"
#include <shaderc/shaderc.hpp>
#include <stdexcept>

namespace {
    shaderc_shader_kind shaderKindToShadercShaderKind(ShaderKind kind) {
        switch (kind) {
        case ShaderKind::VERTEX:
            return shaderc_vertex_shader;
        case ShaderKind::FRAGMENT:
            return shaderc_fragment_shader;
        case ShaderKind::COMPUTE:
            return shaderc_compute_shader;
        case ShaderKind::RAYGEN:
            return shaderc_raygen_shader;
        case ShaderKind::MISS:
            return shaderc_miss_shader;
        case ShaderKind::CLOSEST_HIT:
            return shaderc_closesthit_shader;
        case ShaderKind::ANY_HIT:
            return shaderc_anyhit_shader;
        }
        throw std::runtime_error("Invalid shader kind");
    }
}

std::string ShaderCompiler::LAST_ERROR;

std::vector<uint32_t> ShaderCompiler::compileGLSL(const std::string &source, ShaderKind kind, const std::string &entryPoint) {
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    options.SetTargetSpirv(shaderc_spirv_version_1_4);
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

    shaderc_shader_kind shadercKind = shaderKindToShadercShaderKind(kind);
    auto result = compiler.CompileGlslToSpv(source, shadercKind, "shader.glsl", entryPoint.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        ShaderCompiler::LAST_ERROR = result.GetErrorMessage();
        throw std::runtime_error("Shader compilation failed: " + ShaderCompiler::LAST_ERROR);
    }

    return { result.cbegin(), result.cend() };
}

std::string ShaderCompiler::getLastError() {
    return ShaderCompiler::LAST_ERROR;
}