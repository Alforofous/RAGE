#pragma once

#include <vulkan/vulkan.h>
#include "gpu_context.hpp"
#include "gpu_queue_kind.hpp"
#include "vulkan_device.hpp"
#include "vulkan_instance.hpp"
#include "vulkan_queue.hpp"
#include "vulkan_semaphore_pool.hpp"

namespace RAGE {
    using VulkanContextCreateInfo = VulkanInstanceCreateInfo;

    class VulkanContext {
    public:
        using Instance = VulkanInstance;
        using PhysicalDevice = VkPhysicalDevice;
        using Device = VulkanDevice;

        VulkanContext() = delete;
        explicit VulkanContext(const VulkanContextCreateInfo &info);

        VulkanContext(const VulkanContext &) = delete;
        VulkanContext &operator=(const VulkanContext &) = delete;
        VulkanContext(VulkanContext &&) = delete;
        VulkanContext &operator=(VulkanContext &&) = delete;

        const VulkanInstance &instance() const { return instance_; }
        VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
        const VulkanDevice &device() const { return device_; }
        VulkanDevice &device() { return device_; }

        VkInstance vkInstance() const { return instance_.handle(); }
        VkDevice vkDevice() const { return device_.handle(); }

        const VulkanQueue<queue_kind::Graphics> &graphicsQueue() const { return device_.graphicsQueue(); }
        VulkanQueue<queue_kind::Graphics> &graphicsQueue() { return device_.graphicsQueue(); }
        VulkanSemaphorePool &semaphores() { return device_.semaphores(); }
        VulkanFencePool &fences() { return device_.fences(); }

    private:
        VulkanInstance instance_;
        VkPhysicalDevice physicalDevice_;
        VulkanDevice device_;
    };

    static_assert(GpuContext<VulkanContext>, "VulkanContext must satisfy GpuContext concept");
}
