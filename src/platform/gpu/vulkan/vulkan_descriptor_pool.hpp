#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace RAGE {
    struct VulkanDescriptorPoolSizes {
        uint32_t maxSets = 256;
        uint32_t uniformBuffers = 256;
        uint32_t storageBuffers = 256;
        uint32_t storageImages = 256;
        uint32_t combinedImageSamplers = 256;
        uint32_t sampledImages = 256;
        uint32_t samplers = 64;
    };

    /**
     * A transient descriptor pool sized for one frame-in-flight slot.
     *
     * Designed for the pool-per-frame pattern: create one VulkanDescriptorPool per frame-in-flight,
     * allocate sets as the frame records, then call reset() at the start of the next iteration of
     * that frame slot to invalidate every set allocated from this pool. The pool itself is not
     * destroyed on reset — only its allocations.
     *
     * Maximum capacity per descriptor type is fixed at construction via VulkanDescriptorPoolSizes;
     * allocate() throws if the pool is exhausted. Move-only, single-threaded by contract.
     *
     * @note Allocated VkDescriptorSet handles do not need to be freed individually — they are
     *       reclaimed in bulk by reset() (the pool is created without
     *       VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT).
     */
    class VulkanDescriptorPool {
    public:
        VulkanDescriptorPool() = delete;
        VulkanDescriptorPool(VkDevice device, const VulkanDescriptorPoolSizes &sizes);
        ~VulkanDescriptorPool();

        VulkanDescriptorPool(const VulkanDescriptorPool &) = delete;
        VulkanDescriptorPool &operator=(const VulkanDescriptorPool &) = delete;
        VulkanDescriptorPool(VulkanDescriptorPool &&) = delete;
        VulkanDescriptorPool &operator=(VulkanDescriptorPool &&) = delete;

        VkDescriptorSet allocate(VkDescriptorSetLayout layout);
        void reset();

        uint32_t allocatedCount() const { return allocatedCount_; }

    private:
        VkDevice device_ = VK_NULL_HANDLE;
        VkDescriptorPool pool_ = VK_NULL_HANDLE;
        uint32_t allocatedCount_ = 0;
    };
}
