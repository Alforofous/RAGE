#include "utils/buffer_utils.hpp"
#include <stdexcept>
#include <cstring>

namespace BufferUtils {
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) != 0 && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("Failed to find suitable memory type");
    }

    VkBuffer createBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkDeviceMemory &bufferMemory
    ) {
        VkBuffer buffer = VK_NULL_HANDLE;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        uint32_t memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            vkDestroyBuffer(device, buffer, nullptr);
            throw std::runtime_error("Failed to allocate buffer memory");
        }

        if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS) {
            vkFreeMemory(device, bufferMemory, nullptr);
            vkDestroyBuffer(device, buffer, nullptr);
            throw std::runtime_error("Failed to bind buffer memory");
        }

        return buffer;
    }

    VkBuffer createDeviceAddressBuffer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkDeviceMemory &bufferMemory
    ) {
        VkBuffer buffer = VK_NULL_HANDLE;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create buffer");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        uint32_t memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

        VkMemoryAllocateFlagsInfo flagsInfo{};
        flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;
        allocInfo.pNext = &flagsInfo;

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            vkDestroyBuffer(device, buffer, nullptr);
            throw std::runtime_error("Failed to allocate buffer memory");
        }

        if (vkBindBufferMemory(device, buffer, bufferMemory, 0) != VK_SUCCESS) {
            vkFreeMemory(device, bufferMemory, nullptr);
            vkDestroyBuffer(device, buffer, nullptr);
            throw std::runtime_error("Failed to bind buffer memory");
        }

        return buffer;
    }

    void destroyBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer, nullptr);
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, memory, nullptr);
        }
    }

    void *mapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize size) {
        void *mappedMemory = nullptr;
        if (vkMapMemory(device, memory, 0, size, 0, &mappedMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map buffer memory");
        }

        return mappedMemory;
    }

    void unmapMemory(VkDevice device, VkDeviceMemory memory) {
        vkUnmapMemory(device, memory);
    }

    void copyToDeviceMemory(VkDevice device, VkDeviceMemory memory, const void *data, VkDeviceSize size) {
        void *mapped = mapMemory(device, memory, size);
        memcpy(mapped, data, size);
        unmapMemory(device, memory);
    }

    void copyToBuffer(VkDevice device, VkDeviceMemory memory, const void *data, uint32_t dataSize, uint32_t bufferSize) {
        void *mapped = mapMemory(device, memory, bufferSize);
        memcpy(mapped, data, dataSize);
        unmapMemory(device, memory);
    }

    VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = buffer;

        return vkGetBufferDeviceAddress(device, &addressInfo);
    }
}