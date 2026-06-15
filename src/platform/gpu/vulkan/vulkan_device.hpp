#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include "gpu_queue_kind.hpp"
#include "vulkan_fence_pool.hpp"
#include "vulkan_queue.hpp"
#include "vulkan_semaphore_pool.hpp"

namespace RAGE {
    class VulkanLogicalDevice {
    public:
        VulkanLogicalDevice() = delete;
        VulkanLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t graphicsQueueFamily);
        ~VulkanLogicalDevice();

        VulkanLogicalDevice(const VulkanLogicalDevice &) = delete;
        VulkanLogicalDevice &operator=(const VulkanLogicalDevice &) = delete;
        VulkanLogicalDevice(VulkanLogicalDevice &&) = delete;
        VulkanLogicalDevice &operator=(VulkanLogicalDevice &&) = delete;

        VkDevice handle() const { return device_; }

    private:
        VkDevice device_ = VK_NULL_HANDLE;
    };

    class VulkanDevice {
    public:
        VulkanDevice() = delete;
        VulkanDevice(VkPhysicalDevice physicalDevice, uint32_t graphicsQueueFamily);

        VulkanDevice(const VulkanDevice &) = delete;
        VulkanDevice &operator=(const VulkanDevice &) = delete;
        VulkanDevice(VulkanDevice &&) = delete;
        VulkanDevice &operator=(VulkanDevice &&) = delete;

        VkDevice handle() const { return device_.handle(); }

        const VulkanQueue<queue_kind::Graphics> &graphicsQueue() const { return graphicsQueue_; }
        VulkanQueue<queue_kind::Graphics> &graphicsQueue() { return graphicsQueue_; }
        VulkanSemaphorePool &semaphores() { return semaphores_; }
        VulkanFencePool &fences() { return fences_; }

    private:
        VulkanLogicalDevice device_;
        VulkanSemaphorePool semaphores_;
        VulkanFencePool fences_;
        VulkanQueue<queue_kind::Graphics> graphicsQueue_;
    };
}
