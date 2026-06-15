#include "vulkan_compute_pipeline.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "vulkan_command_buffer.hpp"
#include "vulkan_command_pool.hpp"
#include "vulkan_descriptor_layout_cache.hpp"
#include "vulkan_shader_module.hpp"

namespace {
    std::vector<VkDescriptorSetLayout> buildSetLayouts(RAGE::VulkanDescriptorSetLayoutCache &cache,
                                                       std::span<const RAGE::ShaderDescriptorBinding> bindings) {
        std::unordered_map<uint32_t, std::vector<RAGE::ShaderDescriptorBinding>> grouped;
        uint32_t maxSet = 0;
        for (const RAGE::ShaderDescriptorBinding &b : bindings) {
            grouped[b.set].push_back(b);
            maxSet = std::max(maxSet, b.set);
        }

        std::vector<VkDescriptorSetLayout> layouts;
        if (bindings.empty()) {
            return layouts;
        }

        layouts.reserve(maxSet + 1);
        for (uint32_t s = 0; s <= maxSet; ++s) {
            const auto it = grouped.find(s);
            if (it != grouped.end()) {
                layouts.push_back(cache.getOrCreate(it->second));
            } else {
                layouts.push_back(cache.getOrCreate({}));
            }
        }

        return layouts;
    }
}

namespace RAGE {
    VulkanComputePipeline::VulkanComputePipeline(VkDevice device, VulkanDescriptorSetLayoutCache &layoutCache,
                                                 const VulkanShaderModule &shader)
        : VulkanPipeline(device, buildSetLayouts(layoutCache, shader.bindings()), shader.pushConstants()) {
        if (shader.stage() != ShaderStage::Compute) {
            throw std::runtime_error("VulkanComputePipeline: shader is not a compute stage");
        }

        localWorkgroupSize_ = shader.localWorkgroupSize();

        for (const ShaderPushConstantRange &pc : shader.pushConstants()) {
            const uint32_t end = pc.offset + pc.size;
            pushConstantSize_ = std::max(pushConstantSize_, end);
            pushConstantStages_ |= pc.stages;
        }

        VkPipelineShaderStageCreateInfo stageCI{};
        stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageCI.module = shader.handle();
        stageCI.pName = shader.entryPoint().c_str();

        VkComputePipelineCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        ci.stage = stageCI;
        ci.layout = pipelineLayout_;

        if (vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline_) != VK_SUCCESS) {
            throw std::runtime_error("VulkanComputePipeline: vkCreateComputePipelines failed");
        }
    }

    VulkanComputePipeline::~VulkanComputePipeline() {
        if (pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, pipeline_, nullptr);
        }
    }

    void VulkanComputePipeline::execute(VulkanRecorder<queue_kind::Graphics> &recorder,
                                        const PipelineExecuteContext &ctx) {
        recorder.bindPipeline(PipelineBindPoint::Compute, pipeline_);

        if (!ctx.descriptorSets.empty()) {
            recorder.bindDescriptorSets(PipelineBindPoint::Compute, pipelineLayout_, 0, ctx.descriptorSets);
        }

        if (!ctx.pushConstants.empty() && pushConstantSize_ > 0) {
            recorder.pushConstants(pipelineLayout_, pushConstantStages_, 0, ctx.pushConstants);
        }

        recorder.dispatch(ctx.groupsX, ctx.groupsY, ctx.groupsZ);
    }

    std::array<uint32_t, 3> VulkanComputePipeline::groupCountsFor(uint32_t targetWidth, uint32_t targetHeight,
                                                                  uint32_t targetDepth) const {
        const auto divUp = [](uint32_t n, uint32_t d) -> uint32_t {
            return (d == 0) ? 0u : (n + d - 1) / d;
        };

        return { divUp(targetWidth, localWorkgroupSize_[0]), divUp(targetHeight, localWorkgroupSize_[1]),
                 divUp(targetDepth, localWorkgroupSize_[2]) };
    }
}
