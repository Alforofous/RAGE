#include "vulkan_utils.hpp"
#include <stdexcept>
#include <vector>
#include <set>
#include <iostream>
#include <iostream>

namespace {
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        for (const auto &availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D chooseSwapExtent(GLFWwindow *window, const VkSurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        }
        else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::max(capabilities.minImageExtent.width,
                                          std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(capabilities.minImageExtent.height,
                                           std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    }

    void createSwapchain(GLFWwindow *window, VulkanContext &context) {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physicalDevice, context.surface, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, context.surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, context.surface, &formatCount, formats.data());

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(context.physicalDevice, context.surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(context.physicalDevice, context.surface, &presentModeCount, presentModes.data());

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
        VkExtent2D extent = chooseSwapExtent(window, capabilities);

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = context.surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t queueFamilyIndices[] = { context.graphicsQueueFamily, context.presentQueueFamily };
        if (context.graphicsQueueFamily != context.presentQueueFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(context.device, &createInfo, nullptr, &context.swapchain) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create swap chain");
        }

        // Get swapchain images
        vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, nullptr);
        context.swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, context.swapchainImages.data());

        context.swapchainFormat = surfaceFormat.format;
        context.swapchainExtent = extent;

        // Create image views
        context.swapchainImageViews.resize(context.swapchainImages.size());
        for (size_t i = 0; i < context.swapchainImages.size(); i++) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = context.swapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = context.swapchainFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(context.device, &viewInfo, nullptr, &context.swapchainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create image views");
            }
        }
    }
    VkInstance createVulkanGLFWInstance() {
        std::cout << "Checking Vulkan support..." << std::endl;
        if (!glfwVulkanSupported()) {
            throw std::runtime_error("GLFW reports that Vulkan is not supported");
        }

        std::cout << "Getting required Vulkan extensions..." << std::endl;
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        if (!glfwExtensions) {
            throw std::runtime_error("Failed to get required Vulkan extensions from GLFW");
        }

        std::cout << "Required extensions:" << std::endl;
        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        for (uint32_t i = 0; i < glfwExtensionCount; i++) {
            std::cout << "  " << glfwExtensions[i] << std::endl;
        }

        std::cout << "Adding VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME..." << std::endl;
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        std::cout << "Creating Vulkan application info..." << std::endl;
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "RAGE Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "RAGE";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;  // Let's try 1.2 first

        std::cout << "Creating Vulkan instance info..." << std::endl;
        // Add validation layers in debug builds
        std::vector<const char *> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        // Check validation layer support
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        std::cout << "Available Vulkan layers:" << std::endl;
        for (const auto &layer : availableLayers) {
            std::cout << "  " << layer.layerName << " (spec version "
                      << VK_VERSION_MAJOR(layer.specVersion) << "."
                      << VK_VERSION_MINOR(layer.specVersion) << "."
                      << VK_VERSION_PATCH(layer.specVersion) << ")" << std::endl;
        }

        bool validationLayersAvailable = false;
        for (const char *layerName : validationLayers) {
            bool layerFound = false;
            for (const auto &layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) {
                std::cout << "Validation layer " << layerName << " not available" << std::endl;
                validationLayersAvailable = false;
                break;
            }
            validationLayersAvailable = true;
        }

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (validationLayersAvailable) {
            std::cout << "Enabling validation layers" << std::endl;
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // Add debug extension if validation layers are available
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();
        }
        else {
            std::cout << "Validation layers not available" << std::endl;
            createInfo.enabledLayerCount = 0;
        }

        // Print all extensions we're requesting
        std::cout << "Requesting Vulkan extensions:" << std::endl;
        for (uint32_t i = 0; i < extensions.size(); i++) {
            std::cout << "  " << extensions[i] << std::endl;
        }

        std::cout << "Creating Vulkan instance..." << std::endl;
        VkInstance instance = VK_NULL_HANDLE;
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance: " + std::to_string(result));
        }

        std::cout << "Vulkan instance created successfully" << std::endl;

        return instance;
    }

    VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
                                        uint32_t &graphicsQueueFamily,
                                        uint32_t &presentQueueFamily) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto &device : devices) {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            for (uint32_t i = 0; i < queueFamilyCount; i++) {
                if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                    graphicsQueueFamily = i;
                }

                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
                if (presentSupport != VK_FALSE) {
                    presentQueueFamily = i;
                }

                if (graphicsQueueFamily != UINT32_MAX && presentQueueFamily != UINT32_MAX) {
                    VkPhysicalDeviceProperties2 deviceProperties2{};
                    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

                    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};
                    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
                    deviceProperties2.pNext = &rtProperties;

                    vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

                    if (rtProperties.maxRayRecursionDepth > 0) {
                        return device;
                    }
                }
            }
        }

        throw std::runtime_error("Failed to find a suitable GPU");
    }

    VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice,
                                 uint32_t graphicsQueueFamily,
                                 uint32_t presentQueueFamily,
                                 VkQueue &graphicsQueue,
                                 VkQueue &presentQueue) {
        std::cout << "Creating logical device..." << std::endl;
        std::cout << "Graphics queue family: " << graphicsQueueFamily << std::endl;
        std::cout << "Present queue family: " << presentQueueFamily << std::endl;

        // Use a set to ensure unique queue families
        std::set<uint32_t> uniqueQueueFamilies = { graphicsQueueFamily, presentQueueFamily };
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
            std::cout << "Adding queue create info for family: " << queueFamily << std::endl;
        }

        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
        rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rtPipelineFeatures.rayTracingPipeline = VK_TRUE;
        deviceFeatures2.pNext = &rtPipelineFeatures;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
        asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        asFeatures.accelerationStructure = VK_TRUE;
        asFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
        rtPipelineFeatures.pNext = &asFeatures;

        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
        indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        indexingFeatures.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
        indexingFeatures.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
        indexingFeatures.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
        asFeatures.pNext = &indexingFeatures;

        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
        bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;
        indexingFeatures.pNext = &bufferDeviceAddressFeatures;

        std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
        };

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext = &deviceFeatures2;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        VkDevice device = VK_NULL_HANDLE;
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }

        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);

        return device;
    }
}

VulkanContext createVulkanGLFWSurface(GLFWwindow *window) {
    if (!window) {
        throw std::runtime_error("Invalid window handle provided to createVulkanGLFWSurface");
    }

    VulkanContext context{};
    // Initialize all handles to VK_NULL_HANDLE
    context.instance = VK_NULL_HANDLE;
    context.physicalDevice = VK_NULL_HANDLE;
    context.device = VK_NULL_HANDLE;
    context.surface = VK_NULL_HANDLE;
    context.graphicsQueue = VK_NULL_HANDLE;
    context.presentQueue = VK_NULL_HANDLE;
    context.graphicsQueueFamily = UINT32_MAX;
    context.presentQueueFamily = UINT32_MAX;
    context.swapchain = VK_NULL_HANDLE;

    try {
        context.instance = createVulkanGLFWInstance();

        VkResult result = glfwCreateWindowSurface(context.instance, window, nullptr, &context.surface);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface: " + std::to_string(result));
        }

        context.physicalDevice = pickPhysicalDevice(context.instance, context.surface,
                                                    context.graphicsQueueFamily,
                                                    context.presentQueueFamily);

        context.device = createLogicalDevice(context.physicalDevice,
                                             context.graphicsQueueFamily,
                                             context.presentQueueFamily,
                                             context.graphicsQueue,
                                             context.presentQueue);

        // Initialize ray tracing properties
        VkPhysicalDeviceProperties2 deviceProperties2{};
        deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

        context.rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        deviceProperties2.pNext = &context.rayTracingPipelineProperties;

        context.accelerationStructureProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
        context.rayTracingPipelineProperties.pNext = &context.accelerationStructureProperties;

        vkGetPhysicalDeviceProperties2(context.physicalDevice, &deviceProperties2);

        // Load ray tracing function pointers
        context.vkGetAccelerationStructureBuildSizesKHR =
            reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
                vkGetDeviceProcAddr(context.device, "vkGetAccelerationStructureBuildSizesKHR"));
        if (!context.vkGetAccelerationStructureBuildSizesKHR) {
            throw std::runtime_error("Failed to load vkGetAccelerationStructureBuildSizesKHR");
        }

        context.vkCreateAccelerationStructureKHR =
            reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
                vkGetDeviceProcAddr(context.device, "vkCreateAccelerationStructureKHR"));
        if (!context.vkCreateAccelerationStructureKHR) {
            throw std::runtime_error("Failed to load vkCreateAccelerationStructureKHR");
        }

        context.vkDestroyAccelerationStructureKHR =
            reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
                vkGetDeviceProcAddr(context.device, "vkDestroyAccelerationStructureKHR"));
        if (!context.vkDestroyAccelerationStructureKHR) {
            throw std::runtime_error("Failed to load vkDestroyAccelerationStructureKHR");
        }

        context.vkGetRayTracingShaderGroupHandlesKHR =
            reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
                vkGetDeviceProcAddr(context.device, "vkGetRayTracingShaderGroupHandlesKHR"));
        if (!context.vkGetRayTracingShaderGroupHandlesKHR) {
            throw std::runtime_error("Failed to load vkGetRayTracingShaderGroupHandlesKHR");
        }

        context.vkCreateRayTracingPipelinesKHR =
            reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
                vkGetDeviceProcAddr(context.device, "vkCreateRayTracingPipelinesKHR"));
        if (!context.vkCreateRayTracingPipelinesKHR) {
            throw std::runtime_error("Failed to load vkCreateRayTracingPipelinesKHR");
        }

        context.vkCmdTraceRaysKHR =
            reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
                vkGetDeviceProcAddr(context.device, "vkCmdTraceRaysKHR"));
        if (!context.vkCmdTraceRaysKHR) {
            throw std::runtime_error("Failed to load vkCmdTraceRaysKHR");
        }

        context.vkCmdBuildAccelerationStructuresKHR =
            reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
                vkGetDeviceProcAddr(context.device, "vkCmdBuildAccelerationStructuresKHR"));
        if (!context.vkCmdBuildAccelerationStructuresKHR) {
            throw std::runtime_error("Failed to load vkCmdBuildAccelerationStructuresKHR");
        }

        context.vkGetAccelerationStructureDeviceAddressKHR =
            reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
                vkGetDeviceProcAddr(context.device, "vkGetAccelerationStructureDeviceAddressKHR"));
        if (!context.vkGetAccelerationStructureDeviceAddressKHR) {
            throw std::runtime_error("Failed to load vkGetAccelerationStructureDeviceAddressKHR");
        }

        // Create swapchain
        createSwapchain(window, context);

        return context;
    }
    catch (const std::exception &e) {
        // Clean up in case of error
        if (context.swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(context.device, context.swapchain, nullptr);
        }
        for (auto imageView : context.swapchainImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(context.device, imageView, nullptr);
            }
        }
        if (context.device != VK_NULL_HANDLE) {
            vkDestroyDevice(context.device, nullptr);
        }
        if (context.surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
        }
        if (context.instance != VK_NULL_HANDLE) {
            vkDestroyInstance(context.instance, nullptr);
        }
        throw; // Re-throw the exception after cleanup
    }
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}

VkBuffer createBuffer(const VulkanContext *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory &bufferMemory) {
    VkBuffer buffer = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(context->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context->device, buffer, &memRequirements);

    uint32_t memoryTypeIndex = findMemoryType(context->physicalDevice, memRequirements.memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(context->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(context->device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    if (vkBindBufferMemory(context->device, buffer, bufferMemory, 0) != VK_SUCCESS) {
        vkFreeMemory(context->device, bufferMemory, nullptr);
        vkDestroyBuffer(context->device, buffer, nullptr);
        throw std::runtime_error("Failed to bind buffer memory");
    }

    return buffer;
}

VkBuffer createDeviceAddressBuffer(const VulkanContext *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory &bufferMemory) {
    VkBuffer buffer = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(context->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context->device, buffer, &memRequirements);

    uint32_t memoryTypeIndex = findMemoryType(context->physicalDevice, memRequirements.memoryTypeBits, properties);

    VkMemoryAllocateFlagsInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    allocInfo.pNext = &flagsInfo;

    if (vkAllocateMemory(context->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(context->device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    if (vkBindBufferMemory(context->device, buffer, bufferMemory, 0) != VK_SUCCESS) {
        vkFreeMemory(context->device, bufferMemory, nullptr);
        vkDestroyBuffer(context->device, buffer, nullptr);
        throw std::runtime_error("Failed to bind buffer memory");
    }

    return buffer;
}

void destroyBuffer(const VulkanContext *context, VkBuffer buffer, VkDeviceMemory memory) {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->device, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(context->device, memory, nullptr);
    }
}