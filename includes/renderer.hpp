#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <map>
#include <string>
#include "scene.hpp"
#include "camera.hpp"
#include "vulkan_utils.hpp"
#include "pipelines/vulkan_pipeline.hpp"
#include "vulkan_render_target.hpp"
#include "vulkan_swapchain_manager.hpp"
#include "materials/material.hpp"
#include "matrix4.hpp"
#include "vector3.hpp"

// Uniform buffer data structures
struct CameraData {
    Matrix4 viewInverse;
    Matrix4 projInverse;
    Vector3 cameraPos;
    float padding;  // Align to 16 bytes
};

struct CubeData {
    Vector3 position;
    float padding1;
    Vector3 size;
    float padding2;
    Vector3 color;
    float padding3;
};

class Renderer {
public:
    Renderer(const VulkanContext *context, Scene *scene, Camera *camera);
    ~Renderer();

    // Core rendering interface
    void renderFrame();
    VulkanPipeline *getPipelineForMaterial(const Material *material);  // Get or create pipeline for material

private:
    // Core structures (moved to use VulkanUtils buffer creation)

    struct CameraProperties {
        glm::mat4 viewInverse;
        glm::mat4 projInverse;
    };

    // Initialization functions
    void initializeStorageImage();
    void initializeUniformBuffers();

    // Rendering functions
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // Utility functions
    void bindStorageImageDescriptorSet(VkCommandBuffer cmdBuffer, VulkanPipeline *pipeline);
    void updateCameraBuffer(VkBuffer buffer, VkDeviceMemory memory);
    void updateCubeBuffer(VkBuffer buffer, VkDeviceMemory memory);
    void cleanupStaticResources();

    // Core resources
    const VulkanContext *context;
    Scene *scene;
    Camera *camera;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager;

    // Render resources
    std::unique_ptr<VulkanRenderTarget> renderTarget;  // Main render target
    VkBuffer cameraBuffer;
    VkDeviceMemory cameraMemory;

    // Pipeline management (simplified)
    struct DescriptorSetManager {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> sets;
    };
    DescriptorSetManager descriptorSets;                           // For storage image binding

    // Pipeline caching
    std::map<std::string, std::unique_ptr<VulkanPipeline> > pipelineCache;  // Cache pipelines by shader hash
    std::string generateShaderHash(const Material *material) const;         // Generate hash from material shaders

    // Cached descriptor set resources (to avoid recreating them every frame)
    VkDescriptorSet cachedDescriptorSet = VK_NULL_HANDLE;
    VkBuffer cachedCameraBuffer = VK_NULL_HANDLE;
    VkDeviceMemory cachedCameraMemory = VK_NULL_HANDLE;
    VkBuffer cachedCubeBuffer = VK_NULL_HANDLE;
    VkDeviceMemory cachedCubeMemory = VK_NULL_HANDLE;
};