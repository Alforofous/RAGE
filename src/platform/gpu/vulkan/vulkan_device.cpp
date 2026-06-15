#include "vulkan_device.hpp"
#include <array>
#include <stdexcept>

namespace {
    VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t queueFamily) {
        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        const VkPhysicalDeviceFeatures deviceFeatures{};

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
        bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

        const std::array<const char *, 1> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &bufferDeviceAddressFeatures;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device = VK_NULL_HANDLE;
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }

        return device;
    }

    VkQueue fetchQueue(VkDevice device, uint32_t queueFamily) {
        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, queueFamily, 0, &queue);

        return queue;
    }
}

namespace RAGE {
    VulkanLogicalDevice::VulkanLogicalDevice(VkPhysicalDevice physicalDevice, uint32_t graphicsQueueFamily)
        : device_(createLogicalDevice(physicalDevice, graphicsQueueFamily)) {}

    VulkanLogicalDevice::~VulkanLogicalDevice() {
        if (device_ != VK_NULL_HANDLE) {
            vkDestroyDevice(device_, nullptr);
        }
    }

    VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice, uint32_t graphicsQueueFamily)
        : device_(physicalDevice, graphicsQueueFamily)
        , semaphores_(device_.handle())
        , fences_(device_.handle())
        , graphicsQueue_(fetchQueue(device_.handle(), graphicsQueueFamily), graphicsQueueFamily, fences_) {}
}
