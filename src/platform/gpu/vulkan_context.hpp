#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "gpu_context.hpp"

namespace RAGE {
    struct VulkanContextCreateInfo {
        std::string appName;
        bool enableValidation = true;
        std::vector<const char *> requiredExtensions;
    };

    class VulkanContext {
    public:
        using Instance = VkInstance;
        using PhysicalDevice = VkPhysicalDevice;
        using Device = VkDevice;

        VulkanContext() = default;
        explicit VulkanContext(const VulkanContextCreateInfo &info);
        ~VulkanContext();

        VulkanContext(const VulkanContext &) = delete;
        VulkanContext &operator=(const VulkanContext &) = delete;
        VulkanContext(VulkanContext &&other) noexcept;
        VulkanContext &operator=(VulkanContext &&other) noexcept;

        VkInstance instance() const { return instance_; }
        VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
        VkDevice device() const { return device_; }

        VkQueue graphicsQueue() const { return graphicsQueue_; }
        uint32_t graphicsQueueFamily() const { return graphicsQueueFamily_; }

    private:
        VkInstance instance_ = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue graphicsQueue_ = VK_NULL_HANDLE;
        uint32_t graphicsQueueFamily_ = 0;
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    };

    static_assert(GpuContext<VulkanContext>, "VulkanContext must satisfy GpuContext concept");
}