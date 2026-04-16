#define VMA_IMPLEMENTATION
#include "vulkan_allocator.hpp"
#include "vulkan_type_map.hpp"
#include <stdexcept>

using RAGE::MemoryLocation;

namespace {
    VmaMemoryUsage toVmaMemoryUsage(MemoryLocation loc) {
        switch (loc) {
            case MemoryLocation::GpuOnly:
                return VMA_MEMORY_USAGE_GPU_ONLY;
            case MemoryLocation::CpuToGpu:
                return VMA_MEMORY_USAGE_CPU_TO_GPU;
            case MemoryLocation::GpuToCpu:
                return VMA_MEMORY_USAGE_GPU_TO_CPU;
        }

        return VMA_MEMORY_USAGE_AUTO;
    }
}

namespace RAGE {
    VulkanAllocator::VulkanAllocator(VulkanAllocatorCreateInfo info)
        : device_(info.device) {
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.instance = info.instance;
        allocatorInfo.physicalDevice = info.physicalDevice;
        allocatorInfo.device = info.device;
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

    VulkanAllocator::VulkanAllocator(VulkanAllocator &&other) noexcept
        : allocator_(other.allocator_)
        , device_(other.device_) {
        other.allocator_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
    }

    VulkanAllocator &VulkanAllocator::operator=(VulkanAllocator &&other) noexcept {
        if (this != &other) {
            if (allocator_ != VK_NULL_HANDLE) {
                vmaDestroyAllocator(allocator_);
            }
            allocator_ = other.allocator_;
            device_ = other.device_;
            other.allocator_ = VK_NULL_HANDLE;
            other.device_ = VK_NULL_HANDLE;
        }

        return *this;
    }

    VulkanBuffer VulkanAllocator::createBuffer(BufferCreateInfo info) {
        VkBufferCreateInfo bufferCI{};
        bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCI.size = info.size;
        bufferCI.usage = toVkBufferUsage(info.usage);
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = toVmaMemoryUsage(info.memory);

        if (info.memory != MemoryLocation::GpuOnly) {
            allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo allocInfo{};

        if (vmaCreateBuffer(allocator_, &bufferCI, &allocCI, &buffer, &allocation, &allocInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create VMA buffer");
        }

        void *mapped = (info.memory != MemoryLocation::GpuOnly) ? allocInfo.pMappedData : nullptr;

        uint64_t deviceAddr = 0;
        if (hasFlag(info.usage, BufferUsage::DeviceAddress)) {
            VkBufferDeviceAddressInfo addrInfo{};
            addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            addrInfo.buffer = buffer;
            deviceAddr = vkGetBufferDeviceAddress(device_, &addrInfo);
        }

        return VulkanBuffer(allocator_, info, buffer, allocation, mapped, deviceAddr);
    }

    VulkanImage VulkanAllocator::createImage(ImageCreateInfo info) {
        VkImageCreateInfo imageCI{};
        imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCI.imageType = (info.depth > 1) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
        imageCI.format = toVkFormat(info.format);
        imageCI.extent = { .width = info.width, .height = info.height, .depth = info.depth };
        imageCI.mipLevels = info.mipLevels;
        imageCI.arrayLayers = info.arrayLayers;
        imageCI.samples = static_cast<VkSampleCountFlagBits>(info.sampleCount);
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = toVkImageUsage(info.usage);
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocCI{};
        allocCI.usage = toVmaMemoryUsage(info.memory);

        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        if (vmaCreateImage(allocator_, &imageCI, &allocCI, &image, &allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create VMA image");
        }

        return VulkanImage(device_, allocator_, image, allocation, info);
    }
}