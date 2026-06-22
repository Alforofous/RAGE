#include "pipeline_cache.hpp"
#include <stdexcept>
#include "gpu/vulkan/vulkan_compute_pipeline.hpp"

namespace RAGE {
    PipelineCache::PipelineCache(VkDevice device)
        : device_(device)
        , layoutCache_(device) {
        if (device_ == VK_NULL_HANDLE) {
            throw std::runtime_error("PipelineCache: null device");
        }
    }

    VulkanPipeline &PipelineCache::getOrCreate(const Material<VulkanShaderModule> &material) {
        const std::shared_ptr<const VulkanShaderModule> &shader = material.shader();
        if (!shader) {
            throw std::runtime_error("PipelineCache::getOrCreate: material has no shader");
        }

        const VulkanShaderModule *key = shader.get();
        const auto it = pipelines_.find(key);
        if (it != pipelines_.end()) {
            return *it->second;
        }

        std::unique_ptr<VulkanPipeline> pipeline;
        switch (material.pipelineType()) {
            case PipelineBindPoint::Compute:
                pipeline = std::make_unique<VulkanComputePipeline>(device_, layoutCache_, *shader);
                break;
            case PipelineBindPoint::RayTracing:
                throw std::runtime_error("PipelineCache::getOrCreate: ray-tracing pipelines not yet supported");
            case PipelineBindPoint::Graphics:
                throw std::runtime_error("PipelineCache::getOrCreate: graphics pipelines excluded at the engine layer");
        }

        VulkanPipeline *raw = pipeline.get();
        pipelines_.emplace(key, std::move(pipeline));

        return *raw;
    }
}
