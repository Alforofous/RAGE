#include "vulkan_pipeline.hpp"
#include <stdexcept>
#include <vector>

namespace RAGE {
    VulkanPipeline::VulkanPipeline(VkDevice device, std::span<const VkDescriptorSetLayout> setLayouts,
                                   std::span<const ShaderPushConstantRange> pushConstants)
        : device_(device)
        , setLayouts_(setLayouts.begin(), setLayouts.end()) {
        if (device_ == VK_NULL_HANDLE) {
            throw std::runtime_error("VulkanPipeline: null device");
        }

        std::vector<VkPushConstantRange> vkRanges;
        vkRanges.reserve(pushConstants.size());
        for (const ShaderPushConstantRange &pc : pushConstants) {
            VkPushConstantRange r{};
            r.stageFlags = pc.stages;
            r.offset = pc.offset;
            r.size = pc.size;
            vkRanges.push_back(r);
        }

        VkPipelineLayoutCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount = static_cast<uint32_t>(setLayouts_.size());
        ci.pSetLayouts = setLayouts_.data();
        ci.pushConstantRangeCount = static_cast<uint32_t>(vkRanges.size());
        ci.pPushConstantRanges = vkRanges.data();

        if (vkCreatePipelineLayout(device_, &ci, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("VulkanPipeline: vkCreatePipelineLayout failed");
        }
    }

    VulkanPipeline::~VulkanPipeline() {
        if (pipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        }
    }
}
