#pragma once

#include <memory>
#include <utility>
#include "gpu/gpu_shader_module.hpp"
#include "gpu/gpu_types.hpp"

namespace RAGE {
    /**
     * Material: shader + pipeline-type metadata.
     *
     * Templated on a shader type satisfying GpuShaderModule so Material itself is backend-agnostic.
     * Materials are intended to be shared across renderables (THREE.js style) — two scene nodes
     * holding the same Material resolve to the same VkPipeline via the PipelineCache.
     *
     * Material is intentionally **stateless w.r.t. per-frame data**. Per-object push constants,
     * descriptor-set bindings beyond the implicit render target, and any other per-instance state
     * live on the RenderableNode3D's `prepareFrame` hook. This makes the sharing semantics
     * unambiguous: two renderables sharing one Material share the shader and the pipeline, but
     * each contributes its own data to its own dispatch via its own prepareFrame.
     *
     * The pipeline bind point (Compute / RayTracing) is derived from the shader's stage at
     * construction. The engine layer excludes graphics pipelines: a shader with a Vertex/Fragment
     * stage is constructible into a Material, but downstream PipelineCache rejects it.
     *
     * @param TShader Shader-module type satisfying the GpuShaderModule concept.
     */
    template <GpuShaderModule TShader> class Material {
    public:
        explicit Material(std::shared_ptr<const TShader> shader)
            : shader_(std::move(shader))
            , pipelineType_(deriveBindPoint(shader_.get())) {}

        virtual ~Material() = default;

        Material(const Material &) = delete;
        Material &operator=(const Material &) = delete;
        Material(Material &&) = delete;
        Material &operator=(Material &&) = delete;

        const std::shared_ptr<const TShader> &shader() const { return shader_; }
        PipelineBindPoint pipelineType() const { return pipelineType_; }

    private:
        static PipelineBindPoint deriveBindPoint(const TShader *shader) {
            if (shader == nullptr) {
                return PipelineBindPoint::Compute;
            }
            switch (shader->stage()) {
                case ShaderStage::Compute:
                    return PipelineBindPoint::Compute;
                case ShaderStage::RayGeneration:
                case ShaderStage::Miss:
                case ShaderStage::ClosestHit:
                case ShaderStage::AnyHit:
                case ShaderStage::Intersection:
                case ShaderStage::Callable:
                    return PipelineBindPoint::RayTracing;
                case ShaderStage::Vertex:
                case ShaderStage::Fragment:
                default:
                    return PipelineBindPoint::Graphics;
            }
        }

        std::shared_ptr<const TShader> shader_;
        PipelineBindPoint pipelineType_;
    };
}
