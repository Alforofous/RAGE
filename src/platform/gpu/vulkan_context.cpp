#include "vulkan_context.hpp"
#include "../../shared/logger.hpp"
#include <stdexcept>
#include <cstring>

namespace RAGE {
    namespace {
        VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
                                                     void *userData) {
            (void)type;
            (void)userData;

            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                log(LogLevel::Warn, callbackData->pMessage);
            }

            return VK_FALSE;
        }

        bool checkValidationLayerSupport(const char *layerName) {
            uint32_t layerCount = 0;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> layers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

            for (const auto &layer : layers) {
                if (strcmp(layer.layerName, layerName) == 0) {
                    return true;
                }
            }

            return false;
        }

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
    }

    VulkanContext::VulkanContext(const VulkanContextCreateInfo &info) {
        // --- Instance ---
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = info.appName.c_str();
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = info.appName.c_str();
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        const char *validationLayer = "VK_LAYER_KHRONOS_validation";
        std::vector<const char *> extensions = info.requiredExtensions;

        if (info.enableValidation) {
            if (checkValidationLayerSupport(validationLayer)) {
                createInfo.enabledLayerCount = 1;
                createInfo.ppEnabledLayerNames = &validationLayer;
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            } else {
                log(LogLevel::Warn, "Validation layers requested but not available");
            }
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        // --- Debug messenger ---
        if (info.enableValidation && createInfo.enabledLayerCount > 0) {
            VkDebugUtilsMessengerCreateInfoEXT messengerInfo{};
            messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            messengerInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            messengerInfo.pfnUserCallback = debugCallback;

            auto createFunc = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
            if (createFunc != nullptr) {
                createFunc(instance_, &messengerInfo, nullptr, &debugMessenger_);
            }
        }

        // --- Physical device ---
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("No Vulkan-capable GPU found");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

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

        physicalDevice_ = bestDevice;
        graphicsQueueFamily_ = static_cast<uint32_t>(bestQueueFamily);

        // --- Logical device ---
        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamily_;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        const VkPhysicalDeviceFeatures deviceFeatures{};

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
        bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &bufferDeviceAddressFeatures;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

        if (vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }

        vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    }

    VulkanContext::~VulkanContext() {
        if (device_ != VK_NULL_HANDLE) {
            vkDestroyDevice(device_, nullptr);
        }

        if (debugMessenger_ != VK_NULL_HANDLE) {
            auto destroyFunc = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyFunc != nullptr) {
                destroyFunc(instance_, debugMessenger_, nullptr);
            }
        }

        if (instance_ != VK_NULL_HANDLE) {
            vkDestroyInstance(instance_, nullptr);
        }
    }

    VulkanContext::VulkanContext(VulkanContext &&other) noexcept
        : instance_(other.instance_)
        , physicalDevice_(other.physicalDevice_)
        , device_(other.device_)
        , graphicsQueue_(other.graphicsQueue_)
        , graphicsQueueFamily_(other.graphicsQueueFamily_)
        , debugMessenger_(other.debugMessenger_) {
        other.instance_ = VK_NULL_HANDLE;
        other.physicalDevice_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
        other.graphicsQueue_ = VK_NULL_HANDLE;
        other.graphicsQueueFamily_ = 0;
        other.debugMessenger_ = VK_NULL_HANDLE;
    }

    VulkanContext &VulkanContext::operator=(VulkanContext &&other) noexcept {
        if (this != &other) {
            // Destroy current
            if (device_ != VK_NULL_HANDLE) {
                vkDestroyDevice(device_, nullptr);
            }
            if (debugMessenger_ != VK_NULL_HANDLE) {
                auto destroyFunc = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
                if (destroyFunc != nullptr) {
                    destroyFunc(instance_, debugMessenger_, nullptr);
                }
            }
            if (instance_ != VK_NULL_HANDLE) {
                vkDestroyInstance(instance_, nullptr);
            }

            // Transfer
            instance_ = other.instance_;
            physicalDevice_ = other.physicalDevice_;
            device_ = other.device_;
            graphicsQueue_ = other.graphicsQueue_;
            graphicsQueueFamily_ = other.graphicsQueueFamily_;
            debugMessenger_ = other.debugMessenger_;

            other.instance_ = VK_NULL_HANDLE;
            other.physicalDevice_ = VK_NULL_HANDLE;
            other.device_ = VK_NULL_HANDLE;
            other.graphicsQueue_ = VK_NULL_HANDLE;
            other.graphicsQueueFamily_ = 0;
            other.debugMessenger_ = VK_NULL_HANDLE;
        }

        return *this;
    }
}