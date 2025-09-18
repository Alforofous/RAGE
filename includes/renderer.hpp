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

enum class PipelineType : uint8_t {
    RayTracing,
    Raster
};

class Renderer {
public:
    struct StorageBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
    };

    struct DescriptorSetManager {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> sets;
    };

    // Resource management API
    StorageBuffer createStorageBuffer(VkDeviceSize size);
    void updateStorageBuffer(StorageBuffer &buffer, const void *data, VkDeviceSize size, VkDeviceSize offset = 0);
    void destroyStorageBuffer(StorageBuffer &buffer);

    // Descriptor management
    void allocateDescriptorSets(const VulkanPipeline *pipeline);
    void updateDescriptorSet(uint32_t setIndex, uint32_t binding, const StorageBuffer &buffer);
    void bindDescriptorSets(VkCommandBuffer cmdBuffer, const VulkanPipeline *pipeline);
    void bindStorageImageDescriptorSet(VkCommandBuffer cmdBuffer, VulkanPipeline *pipeline);
    void updateCameraBuffer(VkBuffer buffer, VkDeviceMemory memory);
    void updateCubeBuffer(VkBuffer buffer, VkDeviceMemory memory);

    Renderer(const VulkanContext *context, Scene *scene, Camera *camera);

    void usePipeline(PipelineType type);
    VulkanPipeline *getPipelineForMaterial(const Material *material);  // Get or create pipeline for material
    ~Renderer();

    void renderFrame();
    void drawBindedMaterial();  // Called by RenderableNode3D to render with current material properties

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
    void copyToDeviceMemory(VkDeviceMemory memory, const void *data, VkDeviceSize size);
    void submitCommandBuffer();
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

    // Pipeline management
    std::unique_ptr<VulkanPipeline> rasterPipeline;                // Raster pipeline (future)
    VulkanPipeline *currentPipeline = nullptr;                     // Currently bound pipeline
    DescriptorSetManager descriptorSets;                           // Current descriptor sets

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