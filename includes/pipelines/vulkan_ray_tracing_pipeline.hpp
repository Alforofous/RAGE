#pragma once
#include "vulkan_pipeline.hpp"
#include "vulkan_utils.hpp"

class VulkanRayTracingPipeline : public VulkanPipeline {
public:
    VulkanRayTracingPipeline(
        const VulkanContext *context,
        const GLSLShader &rayGenShader,
        const GLSLShader &missShader,
        const GLSLShader &closestHitShader
    );
    virtual ~VulkanRayTracingPipeline();

    void bind(VkCommandBuffer cmdBuffer) override;
    void dispatch(VkCommandBuffer cmdBuffer, uint32_t width, uint32_t height);

private:
    void createRayTracingPipeline();
    void createShaderBindingTables();
    void createSBTBuffer(uint32_t size, uint32_t alignment, const void *handleData, uint32_t handleSize,
                         VkBuffer &buffer, VkDeviceMemory &memory, VkStridedDeviceAddressRegionKHR &sbt);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    const VulkanContext *context;
    VkShaderModule rayGenShader = VK_NULL_HANDLE;
    VkShaderModule missShader = VK_NULL_HANDLE;
    VkShaderModule closestHitShader = VK_NULL_HANDLE;

    // Shader Binding Tables
    VkStridedDeviceAddressRegionKHR raygenSBT{};
    VkStridedDeviceAddressRegionKHR missSBT{};
    VkStridedDeviceAddressRegionKHR hitSBT{};
    VkStridedDeviceAddressRegionKHR callableSBT{};

    // SBT Buffers
    VkBuffer raygenSBTBuffer = VK_NULL_HANDLE;
    VkDeviceMemory raygenSBTMemory = VK_NULL_HANDLE;
    VkBuffer missSBTBuffer = VK_NULL_HANDLE;
    VkDeviceMemory missSBTMemory = VK_NULL_HANDLE;
    VkBuffer hitSBTBuffer = VK_NULL_HANDLE;
    VkDeviceMemory hitSBTMemory = VK_NULL_HANDLE;
};