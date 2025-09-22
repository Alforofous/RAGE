#pragma once
#include <vulkan/vulkan.h>
#include <functional>

// Forward declarations
class Scene;
class VulkanRenderTarget;
class VulkanContext;
class Renderer;

/**
 * CommandBufferRecorder handles the recording of Vulkan command buffers for rendering.
 *
 * This component separates the complex command buffer recording logic from the Renderer,
 * organizing it into distinct phases for better maintainability and testability.
 */
class CommandBufferRecorder {
public:
    CommandBufferRecorder(const VulkanContext *context, VulkanRenderTarget *renderTarget);
    ~CommandBufferRecorder() = default;

    // Main recording interface
    void recordFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                     const std::function<void(VkCommandBuffer)> &renderSceneCallback);

private:
    // === Command Buffer Management ===
    static void beginRecording(VkCommandBuffer commandBuffer);
    static void endRecording(VkCommandBuffer commandBuffer);

    // === Rendering Phases ===
    void prepareRenderTarget(VkCommandBuffer commandBuffer);
    void finalizeRendering(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // === Image Transition Utilities ===
    void transitionRenderTargetForRendering(VkCommandBuffer commandBuffer);
    void transitionRenderTargetForCopy(VkCommandBuffer commandBuffer);
    void transitionSwapchainForCopy(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void transitionSwapchainForPresentation(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void copyRenderTargetToSwapchain(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // === Dependencies ===
    const VulkanContext *context;
    VulkanRenderTarget *renderTarget;
};