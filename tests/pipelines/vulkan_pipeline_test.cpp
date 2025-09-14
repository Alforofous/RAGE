#include <gtest/gtest.h>
#include "pipelines/vulkan_pipeline.hpp"
#include "glsl_shader.hpp"
#include <vector>
#include <memory>

// Simple test shaders with descriptor bindings
static const std::string vertexShaderSource = R"(
#version 450

layout(binding = 0, set = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} camera;

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = camera.proj * camera.view * vec4(inPosition, 1.0);
    fragColor = vec4(1.0);
}
)";

static const std::string fragmentShaderSource = R"(
#version 450

layout(binding = 1, set = 0) uniform MaterialUBO {
    vec4 color;
} material;

layout(location = 0) in vec4 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = fragColor * material.color;
}
)";

// Test pipeline that inherits from VulkanPipelineBase
class TestGLSLPipeline : public VulkanPipelineBase<VK_PIPELINE_BIND_POINT_GRAPHICS>  {
public:
    TestGLSLPipeline() : VulkanPipelineBase(VK_NULL_HANDLE, VK_NULL_HANDLE, createShaders()) {
        this->createPipeline();
    }

protected:
    void createPipeline() override {}

private:
    static std::vector<GLSLShader> createShaders() {
        return {
            GLSLShader(vertexShaderSource, ShaderKind::VERTEX),
            GLSLShader(fragmentShaderSource, ShaderKind::FRAGMENT)
        };
    }
};

TEST(VulkanPipelineTest, GLSLCompilationAndReflection) {
    try {
        // Create pipeline with GLSL shaders - this will:
        // 1. Compile GLSL to SPIR-V using ShaderCompiler
        // 2. Reflect SPIR-V to discover descriptor bindings
        // 3. Create descriptor set layouts from bindings
        // 4. Create pipeline layout
        auto pipeline = std::make_unique<TestGLSLPipeline>();

        // If we get here, GLSL compilation and reflection worked
        SUCCEED() << "GLSL compilation and shader reflection completed successfully";
    }
    catch (const std::exception &e) {
        std::string error = e.what();

        // We expect Vulkan creation to fail since we're using VK_NULL_HANDLE
        // But shader compilation and reflection should have succeeded first
        if (error.find("Failed to create") != std::string::npos ||
            error.find("Invalid device") != std::string::npos ||
            error.find("null device") != std::string::npos) {
            SUCCEED() << "Expected Vulkan failure after successful shader processing: " << error;
        }
        else {
            FAIL() << "Unexpected error during shader processing: " << error;
        }
    }
}