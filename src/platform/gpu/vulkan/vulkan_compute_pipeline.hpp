#pragma once

#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>
#include "vulkan_pipeline.hpp"

namespace RAGE {
    class VulkanDescriptorSetLayoutCache;
    class VulkanShaderModule;

    /**
     * A compute VkPipeline built from a single reflected SPIR-V compute shader.
     *
     * The constructor derives descriptor-set layouts and push-constant ranges from the shader's
     * reflection metadata, queries them through the supplied layout cache, builds a
     * VkPipelineLayout via the VulkanPipeline base, and creates the VkPipeline. The shader's
     * declared local workgroup size is captured so callers can later compute group counts from
     * a target extent (see groupCountsFor()).
     *
     * execute() binds the compute pipeline, binds the descriptor sets in ctx (starting at set 0),
     * pushes the constant bytes in ctx (when non-empty, addressing the full push-constant range),
     * and dispatches (ctx.groupsX, ctx.groupsY, ctx.groupsZ). The caller is responsible for
     * inserting any required image-layout transitions before and after execute().
     *
     * Move-only. The VkPipeline is destroyed in the destructor; the layout cache must outlive
     * this pipeline.
     */
    class VulkanComputePipeline : public VulkanPipeline {
    public:
        VulkanComputePipeline() = delete;
        VulkanComputePipeline(VkDevice device, VulkanDescriptorSetLayoutCache &layoutCache,
                              const VulkanShaderModule &shader);
        ~VulkanComputePipeline() override;

        void execute(VulkanRecorder<queue_kind::Graphics> &recorder, const PipelineExecuteContext &ctx) override;

        std::array<uint32_t, 3> localWorkgroupSize() const { return localWorkgroupSize_; }

        std::array<uint32_t, 3> groupCountsFor(uint32_t targetWidth, uint32_t targetHeight,
                                               uint32_t targetDepth = 1) const;

    private:
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        std::array<uint32_t, 3> localWorkgroupSize_{ 1, 1, 1 };
        uint32_t pushConstantSize_ = 0;
        VkShaderStageFlags pushConstantStages_ = 0;
    };
}
