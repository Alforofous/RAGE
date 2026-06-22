#include <gtest/gtest.h>
#include <memory>
#include "engine/materials/material.hpp"
#include "gpu/gpu_shader_module.hpp"
#include "gpu/gpu_types.hpp"

using namespace RAGE;

namespace {
    class MockShader {
    public:
        explicit MockShader(ShaderStage s)
            : stage_(s) {}
        ShaderStage stage() const { return stage_; }

    private:
        ShaderStage stage_;
    };

    static_assert(GpuShaderModule<MockShader>);
}

TEST(Material, NullShaderDefaultsToComputeBindPoint) {
    const Material<MockShader> m(nullptr);
    EXPECT_EQ(m.pipelineType(), PipelineBindPoint::Compute);
    EXPECT_EQ(m.shader(), nullptr);
}

TEST(Material, ComputeShaderYieldsComputeBindPoint) {
    auto shader = std::make_shared<const MockShader>(ShaderStage::Compute);
    const Material<MockShader> m(shader);
    EXPECT_EQ(m.pipelineType(), PipelineBindPoint::Compute);
    EXPECT_EQ(m.shader(), shader);
}

TEST(Material, RayGenShaderYieldsRayTracingBindPoint) {
    auto shader = std::make_shared<const MockShader>(ShaderStage::RayGeneration);
    const Material<MockShader> m(shader);
    EXPECT_EQ(m.pipelineType(), PipelineBindPoint::RayTracing);
}

TEST(Material, VertexShaderYieldsGraphicsBindPoint) {
    auto shader = std::make_shared<const MockShader>(ShaderStage::Vertex);
    const Material<MockShader> m(shader);
    EXPECT_EQ(m.pipelineType(), PipelineBindPoint::Graphics);
}

TEST(Material, ShaderIsShareableAcrossMaterials) {
    auto shader = std::make_shared<const MockShader>(ShaderStage::Compute);
    const Material<MockShader> a(shader);
    const Material<MockShader> b(shader);
    EXPECT_EQ(a.shader(), b.shader());
    EXPECT_EQ(shader.use_count(), 3);
}
