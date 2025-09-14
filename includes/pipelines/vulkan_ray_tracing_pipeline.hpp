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
    // === Shader Group Constants ===
    static constexpr uint32_t RAYGEN_GROUP_INDEX = 0;
    static constexpr uint32_t MISS_GROUP_INDEX = 1;
    static constexpr uint32_t HIT_GROUP_INDEX = 2;
    static constexpr uint32_t SHADER_GROUP_COUNT = 3;

    void createShaderBindingTables();
    void createSBTEntry(size_t groupIndex, const std::vector<uint8_t> &handleStorage,
                        size_t handleSize, size_t alignedHandleSize,
                        VkBuffer &buffer, VkDeviceMemory &memory, VkStridedDeviceAddressRegionKHR &sbt);

    const VulkanContext *context;

    // === SBT Resources ===
    struct SBTEntry {
        VkStridedDeviceAddressRegionKHR region{};
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    SBTEntry raygenSBT;
    SBTEntry missSBT;
    SBTEntry hitSBT;
    VkStridedDeviceAddressRegionKHR callableSBT{};
};