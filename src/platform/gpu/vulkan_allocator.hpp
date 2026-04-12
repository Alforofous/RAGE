#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "gpu_types.hpp"
#include "gpu_concepts.hpp"
#include "vulkan_buffer.hpp"
#include "vulkan_image.hpp"

namespace RAGE {
    struct VulkanAllocatorCreateInfo {
        VkInstance instance;
        VkPhysicalDevice physicalDevice;
        VkDevice device;
    };

    class VulkanAllocator {
public:
        using Buffer = VulkanBuffer;
        using Image = VulkanImage;
        using ImageView = VulkanImageView;

        VulkanAllocator() = default;
        explicit VulkanAllocator(VulkanAllocatorCreateInfo info);
        ~VulkanAllocator();

        VulkanAllocator(const VulkanAllocator &) = delete;
        VulkanAllocator& operator=(const VulkanAllocator &) = delete;
        VulkanAllocator(VulkanAllocator &&other) noexcept;
        VulkanAllocator& operator=(VulkanAllocator &&other) noexcept;

        VulkanBuffer createBuffer(BufferCreateInfo info);
        VulkanImage createImage(ImageCreateInfo info);

        VmaAllocator vmaHandle() const { return allocator_; }
        VkDevice device() const { return device_; }

private:
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
    };

    static_assert(GpuAllocator<VulkanAllocator>, "VulkanAllocator must satisfy GpuAllocator concept");
}