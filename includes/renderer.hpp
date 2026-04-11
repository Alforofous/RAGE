#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include <map>
#include <string>
#include "scene.hpp"
#include "camera.hpp"
#include "vulkan_utils.hpp"
#include "pipelines/vulkan_pipeline.hpp"
#include "vulkan_render_target.hpp"
#include "vulkan_swapchain_manager.hpp"
#include "vulkan_descriptor_manager.hpp"
#include "uniform_buffer_manager.hpp"
#include "command_buffer_recorder.hpp"
#include "materials/material.hpp"

/**
 * Minimal, generic renderer that delegates all material-specific logic to materials themselves.
 * The renderer only handles:
 * - Pipeline creation and caching
 * - Command buffer recording
 * - Resource management
 * - Scene traversal
 */
class Renderer {
public:
    Renderer(const VulkanContext *context, Scene *scene, Camera *camera);
    ~Renderer();

    // Core rendering interface
    void renderFrame();
    void setAutoClear(bool autoClear);

private:
    // Pipeline management
    VulkanPipeline *getPipeline(const Material *material);
    static std::string generateShaderHash(const Material *material);
    
    // Scene and material rendering
    void renderScene(VkCommandBuffer commandBuffer, VulkanRenderTarget* renderTarget, uint32_t currentFrame);
    void renderMaterial(VkCommandBuffer commandBuffer, VulkanPipeline *pipeline, 
                       const Material *material, const RenderableNode3D *renderable, VulkanRenderTarget* renderTarget, uint32_t currentFrame);

    // Core resources
    const VulkanContext *context;
    Scene *scene;
    Camera *camera;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager;
    std::unique_ptr<VulkanDescriptorManager> descriptorManager;
    std::unique_ptr<UniformBufferManager> uniformBufferManager;
    std::vector<std::unique_ptr<VulkanRenderTarget>> renderTargets;
    std::vector<std::unique_ptr<CommandBufferRecorder>> commandBufferRecorders;

    // Pipeline caching
    std::map<std::string, std::unique_ptr<VulkanPipeline> > pipelineCache;

    // Auto clear the render target
    bool autoClear;
};