#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include "engine/materials/material.hpp"
#include "gpu/vulkan/vulkan_descriptor_layout_cache.hpp"
#include "gpu/vulkan/vulkan_pipeline.hpp"
#include "gpu/vulkan/vulkan_shader_module.hpp"

namespace RAGE {
    /**
     * Maps a Material to the concrete VulkanPipeline that should execute it.
     *
     * The renderer hands a Material to the cache; the cache returns (or builds and stores) the
     * VulkanPipeline that matches the material's shader. Pipelines are keyed by shader-module
     * identity — two Materials that share the same VulkanShaderModule share the same VkPipeline.
     *
     * The cache owns its VulkanDescriptorSetLayoutCache so that descriptor-set layouts are
     * deduplicated across pipelines, and owns every VulkanPipeline it produces. All references
     * returned from getOrCreate() remain valid for the lifetime of the cache.
     *
     * This is the Vulkan-coupled inflection point: Material<TShader> is backend-agnostic, but
     * the cache that resolves Material → concrete pipeline must know the backend. A future
     * non-Vulkan backend would ship its own pipeline cache alongside.
     *
     * @note Engine layer is raycasting-only. Materials whose pipeline type is RayTracing throw
     *       (not yet implemented); materials with a Graphics shader stage throw on cache lookup.
     */
    class PipelineCache {
    public:
        PipelineCache() = delete;
        explicit PipelineCache(VkDevice device);
        ~PipelineCache() = default;

        PipelineCache(const PipelineCache &) = delete;
        PipelineCache &operator=(const PipelineCache &) = delete;
        PipelineCache(PipelineCache &&) = delete;
        PipelineCache &operator=(PipelineCache &&) = delete;

        VulkanPipeline &getOrCreate(const Material<VulkanShaderModule> &material);

        size_t size() const { return pipelines_.size(); }

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VulkanDescriptorSetLayoutCache layoutCache_;
        std::unordered_map<const VulkanShaderModule *, std::unique_ptr<VulkanPipeline>> pipelines_;
    };
}
