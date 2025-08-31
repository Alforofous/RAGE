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

private:
    void createRayTracingPipeline();

    const VulkanContext *context;
    VkShaderModule rayGenShader = VK_NULL_HANDLE;
    VkShaderModule missShader = VK_NULL_HANDLE;
    VkShaderModule closestHitShader = VK_NULL_HANDLE;
};