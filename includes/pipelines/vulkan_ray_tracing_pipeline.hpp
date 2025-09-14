#pragma once
#include "vulkan_pipeline.hpp"
#include "vulkan_utils.hpp"

class VulkanRayTracingPipeline : public VulkanPipelineBase<VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR> {
public:
    VulkanRayTracingPipeline(
        const VulkanContext *context,
        const GLSLShader &rayGenShader,
        const GLSLShader &missShader,
        const GLSLShader &closestHitShader
    );
    virtual ~VulkanRayTracingPipeline();
    void dispatch(VkCommandBuffer cmdBuffer, uint32_t width, uint32_t height);

protected:
    void createPipeline() override;

private:
    void createRayTracingPipeline();
    void createShaderBindingTables();
    void createSBTBuffer(uint32_t size, uint32_t alignment, const void *handleData, uint32_t handleSize,
                         VkBuffer &buffer, VkDeviceMemory &memory, VkStridedDeviceAddressRegionKHR &sbt);

    const VulkanContext *context;

    VkStridedDeviceAddressRegionKHR raygenSBT{};
    VkStridedDeviceAddressRegionKHR missSBT{};
    VkStridedDeviceAddressRegionKHR hitSBT{};
    VkStridedDeviceAddressRegionKHR callableSBT{};

    VkBuffer raygenSBTBuffer = VK_NULL_HANDLE;
    VkDeviceMemory raygenSBTMemory = VK_NULL_HANDLE;
    VkBuffer missSBTBuffer = VK_NULL_HANDLE;
    VkDeviceMemory missSBTMemory = VK_NULL_HANDLE;
    VkBuffer hitSBTBuffer = VK_NULL_HANDLE;
    VkDeviceMemory hitSBTMemory = VK_NULL_HANDLE;
};