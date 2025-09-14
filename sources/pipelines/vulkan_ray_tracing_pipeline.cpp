#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include "vulkan_utils.hpp"
#include <stdexcept>

VulkanRayTracingPipeline::VulkanRayTracingPipeline(
    const VulkanContext *context,
    const GLSLShader &rayGenShader,
    const GLSLShader &missShader,
    const GLSLShader &closestHitShader
)
    : VulkanPipelineBase<VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR>(context->device, context->physicalDevice, std::vector<GLSLShader>{ rayGenShader, missShader, closestHitShader }),
    context(context) {
    this->createPipeline();
}

VulkanRayTracingPipeline::~VulkanRayTracingPipeline() {
    if (this->raygenSBTBuffer != VK_NULL_HANDLE) {
        this->destroyBuffer(this->raygenSBTBuffer, this->raygenSBTMemory);
    }
    if (this->missSBTBuffer != VK_NULL_HANDLE) {
        this->destroyBuffer(this->missSBTBuffer, this->missSBTMemory);
    }
    if (this->hitSBTBuffer != VK_NULL_HANDLE) {
        this->destroyBuffer(this->hitSBTBuffer, this->hitSBTMemory);
    }
}

void VulkanRayTracingPipeline::createPipeline() {
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = this->getShaderStages();

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.layout = this->getLayout();
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

    VkPipeline pipeline = this->getPipeline();
    if (this->context->vkCreateRayTracingPipelinesKHR(this->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline");
    }
}

void VulkanRayTracingPipeline::dispatch(VkCommandBuffer cmdBuffer, uint32_t width, uint32_t height) {
    if (this->raygenSBT.deviceAddress == 0) {
        this->createShaderBindingTables();
    }

    // Dispatch ray tracing with proper SBTs
    this->context->vkCmdTraceRaysKHR(
        cmdBuffer,
        &this->raygenSBT,
        &this->missSBT,
        &this->hitSBT,
        &this->callableSBT,
        width,
        height,
        1  // depth
    );
}

void VulkanRayTracingPipeline::createShaderBindingTables() {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 deviceProperties{};
    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties.pNext = &rtProperties;
    vkGetPhysicalDeviceProperties2(this->context->physicalDevice, &deviceProperties);

    const size_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
    const uint32_t baseAlignment = rtProperties.shaderGroupBaseAlignment;

    const uint32_t alignedHandleSize = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);

    const uint32_t groupCount = 3;
    const uint32_t sbtSize = groupCount * handleSize;

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    VkResult result = this->context->vkGetRayTracingShaderGroupHandlesKHR(
        this->getDevice(), this->getPipeline(), 0, groupCount, sbtSize, shaderHandleStorage.data());

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to get ray tracing shader group handles");
    }

    // Create raygen SBT buffer using specialized device address helper
    this->raygenSBTBuffer = this->createDeviceAddressBuffer(alignedHandleSize,
                                                            VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                            this->raygenSBTMemory);
    this->copyToBuffer(this->raygenSBTMemory,
                       &shaderHandleStorage[0 * handleSize], handleSize, alignedHandleSize);
    this->raygenSBT.deviceAddress = this->getBufferDeviceAddress(this->raygenSBTBuffer);
    this->raygenSBT.stride = alignedHandleSize;
    this->raygenSBT.size = alignedHandleSize;

    // Create miss SBT buffer using specialized device address helper
    this->missSBTBuffer = this->createDeviceAddressBuffer(alignedHandleSize,
                                                          VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                          this->missSBTMemory);
    this->copyToBuffer(this->missSBTMemory,
                       &shaderHandleStorage[1 * handleSize], handleSize, alignedHandleSize);
    this->missSBT.deviceAddress = this->getBufferDeviceAddress(this->missSBTBuffer);
    this->missSBT.stride = alignedHandleSize;
    this->missSBT.size = alignedHandleSize;

    // Create hit SBT buffer using specialized device address helper
    this->hitSBTBuffer = this->createDeviceAddressBuffer(alignedHandleSize,
                                                         VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                         this->hitSBTMemory);
    this->copyToBuffer(this->hitSBTMemory,
                       &shaderHandleStorage[2 * handleSize], handleSize, alignedHandleSize);
    this->hitSBT.deviceAddress = this->getBufferDeviceAddress(this->hitSBTBuffer);
    this->hitSBT.stride = alignedHandleSize;
    this->hitSBT.size = alignedHandleSize;

    this->callableSBT.deviceAddress = 0;
    this->callableSBT.stride = 0;
    this->callableSBT.size = 0;
}