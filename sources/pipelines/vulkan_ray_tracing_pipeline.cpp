#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include "vulkan_utils.hpp"
#include "utils/buffer_utils.hpp"
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
    this->createShaderBindingTables();
}

VulkanRayTracingPipeline::~VulkanRayTracingPipeline() {
    if (this->raygenSBT.buffer != VK_NULL_HANDLE) {
        BufferUtils::destroyBuffer(this->context->device, this->raygenSBT.buffer, this->raygenSBT.memory);
    }
    if (this->missSBT.buffer != VK_NULL_HANDLE) {
        BufferUtils::destroyBuffer(this->context->device, this->missSBT.buffer, this->missSBT.memory);
    }
    if (this->hitSBT.buffer != VK_NULL_HANDLE) {
        BufferUtils::destroyBuffer(this->context->device, this->hitSBT.buffer, this->hitSBT.memory);
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
    raygenGroup.generalShader = RAYGEN_GROUP_INDEX;
    raygenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(raygenGroup);

    VkRayTracingShaderGroupCreateInfoKHR missGroup{};
    missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missGroup.generalShader = MISS_GROUP_INDEX;
    missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(missGroup);

    VkRayTracingShaderGroupCreateInfoKHR hitGroup{};
    hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    hitGroup.closestHitShader = HIT_GROUP_INDEX;
    hitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    shaderGroups.push_back(hitGroup);

    pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
    pipelineInfo.pGroups = shaderGroups.data();

    if (this->context->vkCreateRayTracingPipelinesKHR(this->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ray tracing pipeline");
    }
}

void VulkanRayTracingPipeline::dispatch(VkCommandBuffer cmdBuffer, uint32_t width, uint32_t height) {
    this->context->vkCmdTraceRaysKHR(
        cmdBuffer,
        &this->raygenSBT.region,
        &this->missSBT.region,
        &this->hitSBT.region,
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

    const uint32_t sbtSize = SHADER_GROUP_COUNT * handleSize;

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    VkResult result = this->context->vkGetRayTracingShaderGroupHandlesKHR(
        this->getDevice(), this->getPipeline(), 0, SHADER_GROUP_COUNT, sbtSize, shaderHandleStorage.data());

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to get ray tracing shader group handles");
    }

    this->createSBTEntry(RAYGEN_GROUP_INDEX, shaderHandleStorage, handleSize, alignedHandleSize,
                         this->raygenSBT.buffer, this->raygenSBT.memory, this->raygenSBT.region);

    this->createSBTEntry(MISS_GROUP_INDEX, shaderHandleStorage, handleSize, alignedHandleSize,
                         this->missSBT.buffer, this->missSBT.memory, this->missSBT.region);

    this->createSBTEntry(HIT_GROUP_INDEX, shaderHandleStorage, handleSize, alignedHandleSize,
                         this->hitSBT.buffer, this->hitSBT.memory, this->hitSBT.region);

    this->callableSBT.deviceAddress = 0;
    this->callableSBT.stride = 0;
    this->callableSBT.size = 0;
}

void VulkanRayTracingPipeline::createSBTEntry(
    size_t groupIndex,
    const std::vector<uint8_t> &handleStorage,
    size_t handleSize,
    size_t alignedHandleSize,
    VkBuffer &buffer,
    VkDeviceMemory &memory,
    VkStridedDeviceAddressRegionKHR &sbt
) {
    buffer = BufferUtils::createDeviceAddressBuffer(
        this->context->device,
        this->context->physicalDevice,
        alignedHandleSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        memory
    );

    this->copyToBuffer(
        memory,
        &handleStorage[groupIndex * handleSize],
        handleSize,
        alignedHandleSize
    );

    sbt.deviceAddress = BufferUtils::getBufferDeviceAddress(this->context->device, buffer);
    sbt.stride = alignedHandleSize;
    sbt.size = alignedHandleSize;
}