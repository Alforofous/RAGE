#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include "vulkan_utils.hpp"
#include <stdexcept>

VulkanRayTracingPipeline::VulkanRayTracingPipeline(
    const VulkanContext *context,
    const GLSLShader &rayGenShader,
    const GLSLShader &missShader,
    const GLSLShader &closestHitShader
)
    : VulkanPipelineBase<VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR>(context->device, std::vector<GLSLShader>{ rayGenShader, missShader, closestHitShader }),
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

    // Create the pipeline using base class device accessor and protected pipeline member
    if (this->context->vkCreateRayTracingPipelinesKHR(this->getDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->pipeline) != VK_SUCCESS) {
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

    // Create raygen SBT buffer (using existing working method)
    this->createSBTBuffer(alignedHandleSize, baseAlignment,
                          &shaderHandleStorage[0 * handleSize], handleSize,
                          this->raygenSBTBuffer, this->raygenSBTMemory, this->raygenSBT);

    // Create miss SBT buffer (using existing working method)
    this->createSBTBuffer(alignedHandleSize, baseAlignment,
                          &shaderHandleStorage[1 * handleSize], handleSize,
                          this->missSBTBuffer, this->missSBTMemory, this->missSBT);

    // Create hit SBT buffer (using existing working method)
    this->createSBTBuffer(alignedHandleSize, baseAlignment,
                          &shaderHandleStorage[2 * handleSize], handleSize,
                          this->hitSBTBuffer, this->hitSBTMemory, this->hitSBT);

    this->callableSBT.deviceAddress = 0;
    this->callableSBT.stride = 0;
    this->callableSBT.size = 0;
}

void VulkanRayTracingPipeline::createSBTBuffer(uint32_t size, uint32_t alignment, const void *handleData, uint32_t handleSize,
                                               VkBuffer &buffer, VkDeviceMemory &memory, VkStridedDeviceAddressRegionKHR &sbt) {
    const uint32_t alignedSize = (size + alignment - 1) & ~(alignment - 1);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = alignedSize;
    bufferInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(this->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create SBT buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(this->device, buffer, &memRequirements);

    uint32_t memoryTypeIndex = findMemoryType(this->context->physicalDevice, memRequirements.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    VkMemoryAllocateFlagsInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    allocInfo.pNext = &flagsInfo;

    if (vkAllocateMemory(this->device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        vkDestroyBuffer(this->device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate SBT buffer memory");
    }

    if (vkBindBufferMemory(this->device, buffer, memory, 0) != VK_SUCCESS) {
        vkFreeMemory(this->device, memory, nullptr);
        vkDestroyBuffer(this->device, buffer, nullptr);
        throw std::runtime_error("Failed to bind SBT buffer memory");
    }

    void *mappedMemory = nullptr;
    if (vkMapMemory(this->device, memory, 0, alignedSize, 0, &mappedMemory) != VK_SUCCESS) {
        vkFreeMemory(this->device, memory, nullptr);
        vkDestroyBuffer(this->device, buffer, nullptr);
        throw std::runtime_error("Failed to map SBT buffer memory");
    }

    memcpy(mappedMemory, handleData, handleSize);
    vkUnmapMemory(this->device, memory);

    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;

    VkDeviceAddress deviceAddress = vkGetBufferDeviceAddress(this->device, &addressInfo);

    sbt.deviceAddress = deviceAddress;
    sbt.stride = alignedSize;
    sbt.size = alignedSize;
}