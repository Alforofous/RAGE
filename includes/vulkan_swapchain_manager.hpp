#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_utils.hpp"

class VulkanSwapchainManager {
public:
    VulkanSwapchainManager(const VulkanContext *context, uint32_t maxFramesInFlight = 3);
    ~VulkanSwapchainManager();

    // === Frame Management ===
    void waitForCurrentFrame();
    uint32_t acquireNextImage();
    void submitAndPresent(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void advanceFrame();

    // === State Queries ===
    size_t getCurrentFrameIndex() const { return this->currentFrame; }
    bool hasSubmittedWork() const { return this->hasSubmittedAnyWork; }
    uint32_t getMaxFramesInFlight() const { return this->maxFramesInFlight; }

    // === Command Buffer Access ===
    VkCommandBuffer getCurrentCommandBuffer() const;
    void resetCurrentCommandBuffer();

private:
    const VulkanContext *context;
    const uint32_t maxFramesInFlight;

    // === Frame Synchronization ===
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    // === Command Resources ===
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // === State ===
    size_t currentFrame;
    bool hasSubmittedAnyWork;

    // === Initialization ===
    void createSynchronizationObjects();
    void createCommandPool();
    void createCommandBuffers();

    // === Cleanup ===
    void dispose();
};