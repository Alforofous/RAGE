#include "vulkan_allocator.hpp"
#include <cstdlib>
#include <stdexcept>
#include "vulkan_type_map.hpp"

using RAGE::MemoryLocation;

namespace {
    struct VmaMemoryConfig {
        VmaMemoryUsage usage;
        VmaAllocationCreateFlags flags;
    };

    VmaMemoryConfig toVmaMemoryConfig(MemoryLocation loc) {
        switch (loc) {
            case MemoryLocation::GpuOnly:
                return { .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, .flags = 0 };
            case MemoryLocation::CpuToGpu:
                return { .usage = VMA_MEMORY_USAGE_AUTO,
                         .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                  VMA_ALLOCATION_CREATE_MAPPED_BIT };
            case MemoryLocation::GpuToCpu:
                return { .usage = VMA_MEMORY_USAGE_AUTO,
                         .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT };
        }
        std::abort();
    }

    struct BufferAllocation {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        void *mapped = nullptr;
    };

    BufferAllocation allocateBuffer(VmaAllocator allocator, const RAGE::BufferCreateInfo &info) {
        VkBufferCreateInfo bufferCI{};
        bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCI.size = info.size;
        bufferCI.usage = RAGE::toVkBufferUsage(info.usage);
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        const VmaMemoryConfig memCfg = toVmaMemoryConfig(info.memory);
        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = memCfg.usage;
        allocCI.flags = memCfg.flags;

        BufferAllocation result;
        VmaAllocationInfo allocInfo{};
        if (vmaCreateBuffer(allocator, &bufferCI, &allocCI, &result.buffer, &result.allocation, &allocInfo) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create VMA buffer");
        }

        result.mapped = (info.memory != MemoryLocation::GpuOnly) ? allocInfo.pMappedData : nullptr;

        return result;
    }
}

namespace RAGE {
    VulkanAllocator::VulkanAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
        : device_(device) {
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.instance = instance;
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = device;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

        if (vmaCreateAllocator(&allocatorInfo, &allocator_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create VMA allocator");
        }
    }

    VulkanAllocator::~VulkanAllocator() {
        if (allocator_ != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator_);
            allocator_ = VK_NULL_HANDLE;
        }
    }

    VulkanBuffer VulkanAllocator::createBuffer(BufferCreateInfo info) {
        const BufferAllocation alloc = allocateBuffer(allocator_, info);

        uint64_t deviceAddr = 0;
        if (hasFlag(info.usage, BufferUsage::ShaderDeviceAddress)) {
            VkBufferDeviceAddressInfo addrInfo{};
            addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addrInfo.buffer = alloc.buffer;
            deviceAddr = vkGetBufferDeviceAddress(device_, &addrInfo);
        }

        return VulkanBuffer(allocator_, info, alloc.buffer, alloc.allocation, alloc.mapped, deviceAddr);
    }

    VulkanAllocator::Stats VulkanAllocator::stats() const {
        VmaTotalStatistics totals{};
        vmaCalculateStatistics(allocator_, &totals);
        return { .usedBytes = totals.total.statistics.allocationBytes,
                 .allocationCount = totals.total.statistics.allocationCount,
                 .blockCount = totals.total.statistics.blockCount };
    }

    VulkanImage VulkanAllocator::createImage(ImageCreateInfo info) {
        VkImageCreateInfo imageCI{};
        imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCI.imageType = (info.depth > 1) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
        imageCI.format = toVkFormat(info.format);
        imageCI.extent = { .width = info.width, .height = info.height, .depth = info.depth };
        imageCI.mipLevels = info.mipLevels;
        imageCI.arrayLayers = info.arrayLayers;
        imageCI.samples = static_cast<VkSampleCountFlagBits>(static_cast<uint32_t>(info.sampleCount));
        imageCI.tiling = (info.memory == MemoryLocation::GpuOnly) ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR;
        imageCI.usage = toVkImageUsage(info.usage);
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        const VmaMemoryConfig memCfg = toVmaMemoryConfig(info.memory);
        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = memCfg.usage;
        allocCI.flags = memCfg.flags;

        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        if (vmaCreateImage(allocator_, &imageCI, &allocCI, &image, &allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create VMA image");
        }

        return VulkanImage(device_, allocator_, image, allocation, info);
    }
}
