#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <stdexcept>
#include <map>
#include <string>
#include "scene.hpp"
#include "camera.hpp"
#include "vulkan_utils.hpp"
#include "pipelines/vulkan_pipeline.hpp"
#include "pipelines/vulkan_ray_tracing_pipeline.hpp"
#include "vulkan_render_target.hpp"
#include "materials/material.hpp"

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

    // Push constant management
    template<typename T>
    void pushConstants(VkCommandBuffer cmdBuffer, const VulkanPipeline *pipeline, const T &data) {
        const auto &range = VulkanPipeline::getPushConstantRange();
        if (sizeof(T) > range.size) {
            throw std::runtime_error("Push constant data too large");
        }
        vkCmdPushConstants(cmdBuffer, pipeline->getLayout(), range.stageFlags, range.offset, sizeof(T), &data);
    }

    Renderer(const VulkanContext *context, Scene *scene, Camera *camera);

    void usePipeline(PipelineType type);
    VulkanPipeline *getPipelineForMaterial(const Material *material);  // Get or create pipeline for material
    ~Renderer();

    void renderFrame();
    void drawBindedMaterial();  // Called by RenderableNode3D to render with current material properties

private:
    // Core structures
    struct AllocatedBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceAddress deviceAddress = 0;
    };

    struct CameraProperties {
        glm::mat4 viewInverse;
        glm::mat4 projInverse;
    };

    // Initialization functions
    void initializeCommandPool();
    void initializeCommandBuffers();
    void initializeStorageImage();
    void initializeUniformBuffers();

    // Rendering functions
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void submitAndPresent(uint32_t imageIndex);

    // Utility functions
    AllocatedBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void copyToDeviceMemory(VkDeviceMemory memory, const void *data, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void submitCommandBuffer();

    // Core resources
    const VulkanContext *context;
    Scene *scene;
    Camera *camera;

    // Vulkan resources
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    size_t currentFrame;
    bool hasSubmittedWork;

    // Render resources
    std::unique_ptr<VulkanRenderTarget> renderTarget;  // Main render target
    AllocatedBuffer cameraBuffer;

    // Pipeline management
    std::unique_ptr<VulkanPipeline> rasterPipeline;                // Raster pipeline (future)
    VulkanPipeline *currentPipeline = nullptr;                     // Currently bound pipeline
    DescriptorSetManager descriptorSets;                           // Current descriptor sets

    // Pipeline caching
    std::map<std::string, std::unique_ptr<VulkanPipeline> > pipelineCache;  // Cache pipelines by shader hash
    std::string generateShaderHash(const Material *material) const;         // Generate hash from material shaders
};