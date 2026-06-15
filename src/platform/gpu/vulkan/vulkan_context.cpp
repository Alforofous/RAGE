#include "vulkan_context.hpp"
#include <stdexcept>
#include <vector>

namespace {
    uint32_t scorePhysicalDevice(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        uint32_t score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
        }

        return score;
    }

    int32_t findGraphicsQueueFamily(VkPhysicalDevice device) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, families.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                return static_cast<int32_t>(i);
            }
        }

        return -1;
    }

    struct SelectedDevice {
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint32_t queueFamily = 0;
    };

    SelectedDevice selectPhysicalDevice(VkInstance instance) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("No Vulkan-capable GPU found");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
        uint32_t bestScore = 0;
        int32_t bestQueueFamily = -1;

        for (const auto &dev : devices) {
            const int32_t queueFamily = findGraphicsQueueFamily(dev);
            if (queueFamily < 0) {
                continue;
            }

            const uint32_t score = scorePhysicalDevice(dev);
            if (score > bestScore) {
                bestScore = score;
                bestDevice = dev;
                bestQueueFamily = queueFamily;
            }
        }

        if (bestDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("No suitable GPU found");
        }

        return { .physicalDevice = bestDevice, .queueFamily = static_cast<uint32_t>(bestQueueFamily) };
    }
}

namespace RAGE {
    VulkanContext::VulkanContext(const VulkanContextCreateInfo &info)
        : instance_(info)
        , physicalDevice_(selectPhysicalDevice(instance_.handle()).physicalDevice)
        , device_(physicalDevice_, static_cast<uint32_t>(findGraphicsQueueFamily(physicalDevice_))) {}
}
