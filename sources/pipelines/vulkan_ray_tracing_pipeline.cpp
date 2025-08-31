#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include <stdexcept>

VulkanRayTracingPipeline::VulkanRayTracingPipeline(
    const VulkanContext *context,
    const GLSLShader &rayGenShader,
    const GLSLShader &missShader,
    const GLSLShader &closestHitShader
)
    : VulkanPipeline(context->device, std::vector<GLSLShader>{ rayGenShader, missShader, closestHitShader }),
    context(context),
    rayGenShader(this->createShaderModuleFromSPIRV(0)),  // Use compiled SPIR-V from base class
    missShader(this->createShaderModuleFromSPIRV(1)),    // Use compiled SPIR-V from base class
    closestHitShader(this->createShaderModuleFromSPIRV(2)) {
    // Use compiled SPIR-V from base class
    this->createRayTracingPipeline();
}

VulkanRayTracingPipeline::~VulkanRayTracingPipeline() {
    this->destroyShaderModule(this->rayGenShader);
    this->destroyShaderModule(this->missShader);
    this->destroyShaderModule(this->closestHitShader);
}

void VulkanRayTracingPipeline::createRayTracingPipeline() {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    VkPipelineShaderStageCreateInfo rayGenStageInfo{};
    rayGenStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rayGenStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    rayGenStageInfo.module = this->rayGenShader;
    rayGenStageInfo.pName = "main";
    shaderStages.push_back(rayGenStageInfo);

    VkPipelineShaderStageCreateInfo missStageInfo{};
    missStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    missStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missStageInfo.module = this->missShader;
    missStageInfo.pName = "main";
    shaderStages.push_back(missStageInfo);

    VkPipelineShaderStageCreateInfo hitStageInfo{};
    hitStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    hitStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    hitStageInfo.module = this->closestHitShader;
    hitStageInfo.pName = "main";
    shaderStages.push_back(hitStageInfo);

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.layout = this->pipelineLayout;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
    VkRayTracingShaderGroupCreateInfoKHR raygenGroup{};
    raygenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    raygenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygenGroup.generalShader = 0;
    raygenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(raygenGroup);

    VkRayTracingShaderGroupCreateInfoKHR missGroup{};
    missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missGroup.generalShader = 1;
    missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(missGroup);

    VkRayTracingShaderGroupCreateInfoKHR hitGroup{};
    hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    hitGroup.closestHitShader = 2;
    hitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(hitGroup);

    pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
    pipelineInfo.pGroups = shaderGroups.data();

    if (this->context->vkCreateRayTracingPipelinesKHR(this->device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline");
    }
}

void VulkanRayTracingPipeline::bind(VkCommandBuffer cmdBuffer) {
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, this->pipeline);
}