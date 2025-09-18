#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32_t graphicsQueueFamily;
    uint32_t presentQueueFamily;
    // Ray tracing properties and functions
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties;
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;

    // Acceleration structure function pointers
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;

    // Swapchain
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainFormat;
    VkExtent2D swapchainExtent;
};

VulkanContext createVulkanGLFWSurface(GLFWwindow *window);
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
VkBuffer createBuffer(const VulkanContext *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory &bufferMemory);
VkBuffer createDeviceAddressBuffer(const VulkanContext *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory &bufferMemory);
void destroyBuffer(const VulkanContext *context, VkBuffer buffer, VkDeviceMemory memory);