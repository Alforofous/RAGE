#include "vulkan_instance.hpp"
#include "shared/logger.hpp"
#include <cstring>
#include <stdexcept>

using RAGE::Core::log;
using RAGE::Core::LogLevel;

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

    VkInstance createInstance(const RAGE::VulkanInstanceCreateInfo &info) {
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

        VkInstance instance = VK_NULL_HANDLE;
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        return instance;
    }

    VkDebugUtilsMessengerEXT createDebugMessenger(VkInstance instance) {
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo{};
        messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        messengerInfo.pfnUserCallback = debugCallback;

        auto createFunc = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (createFunc == nullptr) {
            log(LogLevel::Warn, "Failed to load vkCreateDebugUtilsMessengerEXT");

            return VK_NULL_HANDLE;
        }

        VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
        if (createFunc(instance, &messengerInfo, nullptr, &messenger) != VK_SUCCESS) {
            log(LogLevel::Warn, "Failed to create debug messenger");

            return VK_NULL_HANDLE;
        }

        return messenger;
    }
}

namespace RAGE {
    VulkanInstance::VulkanInstance(const VulkanInstanceCreateInfo &info)
        : instance_(createInstance(info))
        , debugMessenger_(info.enableValidation ? createDebugMessenger(instance_) : VK_NULL_HANDLE) {}

    VulkanInstance::~VulkanInstance() {
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
}
