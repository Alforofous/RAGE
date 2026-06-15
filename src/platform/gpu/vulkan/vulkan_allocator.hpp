#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "gpu_types.hpp"
#include "gpu_concepts.hpp"
#include "vulkan_buffer.hpp"
#include "vulkan_image.hpp"

namespace RAGE {
    class VulkanContext;

    class VulkanAllocator {
    public:
        using Buffer = VulkanBuffer;
        using Image = VulkanImage;

        VulkanAllocator() = delete;
        VulkanAllocator(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
        ~VulkanAllocator();

        VulkanAllocator(const VulkanAllocator &) = delete;
        VulkanAllocator &operator=(const VulkanAllocator &) = delete;
        VulkanAllocator(VulkanAllocator &&) = delete;
        VulkanAllocator &operator=(VulkanAllocator &&) = delete;

        VulkanBuffer createBuffer(BufferCreateInfo info);
        VulkanImage createImage(ImageCreateInfo info);

        struct Stats {
            uint64_t usedBytes = 0;
            uint32_t allocationCount = 0;
            uint32_t blockCount = 0;
        };
        Stats stats() const;

    private:
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
    };

    static_assert(GpuAllocator<VulkanAllocator>, "VulkanAllocator must satisfy GpuAllocator concept");
}