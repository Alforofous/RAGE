#include "vulkan_swapchain_manager.hpp"
#include <stdexcept>
#include <array>

VulkanSwapchainManager::VulkanSwapchainManager(const VulkanContext *context, uint32_t maxFramesInFlight)
    : context(context)
    , maxFramesInFlight(maxFramesInFlight)
    , commandPool(VK_NULL_HANDLE)
    , currentFrame(0)
    , hasSubmittedAnyWork(false) {
    this->createCommandPool();
    this->createCommandBuffers();
    this->createSynchronizationObjects();
}

VulkanSwapchainManager::~VulkanSwapchainManager() {
    this->dispose();
}

void VulkanSwapchainManager::waitForCurrentFrame() {
    if (this->hasSubmittedAnyWork) {
        vkWaitForFences(this->context->device, 1, &this->inFlightFences[this->currentFrame], VK_TRUE, UINT64_MAX);
    }
}

uint32_t VulkanSwapchainManager::acquireNextImage() {
    // Reset fence and command buffer for this frame
    vkResetFences(this->context->device, 1, &this->inFlightFences[this->currentFrame]);
    this->resetCurrentCommandBuffer();

    // Acquire next swapchain image
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        this->context->device,
        this->context->swapchain,
        UINT64_MAX,
        this->imageAvailableSemaphores[this->currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swapchain image: " + std::to_string(result));
    }

    return imageIndex;
}

void VulkanSwapchainManager::submitAndPresent(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &this->renderFinishedSemaphores[this->currentFrame];
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &this->imageAvailableSemaphores[this->currentFrame];

    std::array<VkPipelineStageFlags, 1> waitStages = { VK_PIPELINE_STAGE_TRANSFER_BIT };
    submitInfo.pWaitDstStageMask = waitStages.data();

    if (vkQueueSubmit(this->context->graphicsQueue, 1, &submitInfo, this->inFlightFences[this->currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer");
    }
    this->hasSubmittedAnyWork = true;

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &this->renderFinishedSemaphores[this->currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &this->context->swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(this->context->presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // TODO: Add swapchain recreation
        return;
    }
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }
}

void VulkanSwapchainManager::advanceFrame() {
    this->currentFrame = (this->currentFrame + 1) % this->maxFramesInFlight;
}

VkCommandBuffer VulkanSwapchainManager::getCurrentCommandBuffer() const {
    return this->commandBuffers[this->currentFrame];
}

void VulkanSwapchainManager::resetCurrentCommandBuffer() {
    vkResetCommandBuffer(this->commandBuffers[this->currentFrame], 0);
}

void VulkanSwapchainManager::createSynchronizationObjects() {
    this->imageAvailableSemaphores.resize(this->maxFramesInFlight);
    this->renderFinishedSemaphores.resize(this->maxFramesInFlight);
    this->inFlightFences.resize(this->maxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < this->maxFramesInFlight; i++) {
        if (vkCreateSemaphore(this->context->device, &semaphoreInfo, nullptr, &this->imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(this->context->device, &semaphoreInfo, nullptr, &this->renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(this->context->device, &fenceInfo, nullptr, &this->inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects");
        }
    }
}

void VulkanSwapchainManager::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = this->context->graphicsQueueFamily;

    if (vkCreateCommandPool(this->context->device, &poolInfo, nullptr, &this->commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

void VulkanSwapchainManager::createCommandBuffers() {
    this->commandBuffers.resize(this->maxFramesInFlight);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = this->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = this->maxFramesInFlight;

    if (vkAllocateCommandBuffers(this->context->device, &allocInfo, this->commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }
}

void VulkanSwapchainManager::dispose() {
    // Wait for all operations to complete
    vkDeviceWaitIdle(this->context->device);

    // Clean up synchronization objects
    for (auto *semaphore : this->imageAvailableSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(this->context->device, semaphore, nullptr);
        }
    }
    this->imageAvailableSemaphores.clear();

    for (auto *semaphore : this->renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(this->context->device, semaphore, nullptr);
        }
    }
    this->renderFinishedSemaphores.clear();

    for (auto *fence : this->inFlightFences) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(this->context->device, fence, nullptr);
        }
    }
    this->inFlightFences.clear();

    // Clean up command pool (this also frees all allocated command buffers)
    if (this->commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(this->context->device, this->commandPool, nullptr);
        this->commandPool = VK_NULL_HANDLE;
    }
}