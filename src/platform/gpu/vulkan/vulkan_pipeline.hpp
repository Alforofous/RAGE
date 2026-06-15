#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>
#include "gpu_queue_kind.hpp"
#include "vulkan_shader_module.hpp"

namespace RAGE {
    template <QueueKind K> class VulkanRecorder;

    struct PipelineExecuteContext {
        std::span<const VkDescriptorSet> descriptorSets = {};
        std::span<const std::byte> pushConstants = {};
        uint32_t groupsX = 1;
        uint32_t groupsY = 1;
        uint32_t groupsZ = 1;
    };

    /**
     * Abstract base for all RAGE pipeline objects.
     *
     * Each concrete subclass (compute, ray-tracing, …) owns a VkPipeline plus the shared
     * VkPipelineLayout describing its descriptor sets and push-constant ranges. Higher layers
     * dispatch pipelines polymorphically through execute(): the renderer never downcasts to a
     * concrete pipeline type. The PipelineExecuteContext carries per-call inputs (descriptor sets,
     * push-constant bytes, dispatch / trace dimensions); how each subclass interprets those fields
     * is part of its contract.
     *
     * Pipeline base owns the VkPipelineLayout and the list of descriptor-set layouts it was built
     * from (the layouts themselves are owned by VulkanDescriptorSetLayoutCache, not by Pipeline).
     * Subclasses own their VkPipeline. Move-only.
     *
     * @note No rasterization pipeline ships at the engine layer; the raycaster paradigm covers
     *       compute and ray-tracing subclasses only.
     */
    class VulkanPipeline {
    public:
        VulkanPipeline() = delete;
        virtual ~VulkanPipeline();

        VulkanPipeline(const VulkanPipeline &) = delete;
        VulkanPipeline &operator=(const VulkanPipeline &) = delete;
        VulkanPipeline(VulkanPipeline &&) = delete;
        VulkanPipeline &operator=(VulkanPipeline &&) = delete;

        virtual void execute(VulkanRecorder<queue_kind::Graphics> &recorder, const PipelineExecuteContext &ctx) = 0;

        VkPipelineLayout pipelineLayout() const { return pipelineLayout_; }
        std::span<const VkDescriptorSetLayout> descriptorSetLayouts() const { return setLayouts_; }

    protected:
        VulkanPipeline(VkDevice device, std::span<const VkDescriptorSetLayout> setLayouts,
                       std::span<const ShaderPushConstantRange> pushConstants);

        VkDevice device_ = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> setLayouts_;
    };
}
