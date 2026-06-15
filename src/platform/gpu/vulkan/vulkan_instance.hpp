#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

namespace RAGE {
    struct VulkanInstanceCreateInfo {
        std::string appName;
        bool enableValidation = true;
        std::vector<const char *> requiredExtensions;
    };

    class VulkanInstance {
    public:
        VulkanInstance() = delete;
        explicit VulkanInstance(const VulkanInstanceCreateInfo &info);
        ~VulkanInstance();

        VulkanInstance(const VulkanInstance &) = delete;
        VulkanInstance &operator=(const VulkanInstance &) = delete;
        VulkanInstance(VulkanInstance &&) = delete;
        VulkanInstance &operator=(VulkanInstance &&) = delete;

        VkInstance handle() const { return instance_; }

    private:
        VkInstance instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    };
}
