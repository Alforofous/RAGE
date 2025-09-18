#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

namespace BufferUtils {
    // === Memory Type Finding ===
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // === Buffer Creation ===
    VkBuffer createBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkDeviceMemory &bufferMemory
    );

    VkBuffer createDeviceAddressBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkDeviceMemory &bufferMemory
    );

    // === Buffer Destruction ===
    void destroyBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory);

    // === Memory Mapping ===
    void *mapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize size);
    void unmapMemory(VkDevice device, VkDeviceMemory memory);

    // === Data Transfer ===
    void copyToDeviceMemory(VkDevice device, VkDeviceMemory memory, const void *data, VkDeviceSize size);
    void copyToBuffer(VkDevice device, VkDeviceMemory memory, const void *data, uint32_t dataSize, uint32_t bufferSize);

    // === Device Address ===
    VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer);
}