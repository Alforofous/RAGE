#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>
#include "gpu_types.hpp"

namespace RAGE {
    class VulkanBuffer;
    class VulkanImageView;
    class VulkanRenderTarget;

    /**
     * A fluent batcher for vkUpdateDescriptorSets.
     *
     * Accumulates per-binding writes — storage images, uniform buffers, storage buffers — and
     * flushes them as a single vkUpdateDescriptorSets call on commit(). Buffer / image info
     * structures referenced by each write are stored inside the writer so they remain
     * address-stable until commit() runs; never call commit() on a moved-from or destroyed
     * writer.
     *
     * After commit() the writer is empty and may be reused for the next batch.
     *
     * Single-threaded by contract: each VulkanDescriptorWriter belongs to the thread that
     * records descriptors for one frame. Move-only.
     */
    class VulkanDescriptorWriter {
    public:
        VulkanDescriptorWriter() = delete;
        explicit VulkanDescriptorWriter(VkDevice device);

        VulkanDescriptorWriter(const VulkanDescriptorWriter &) = delete;
        VulkanDescriptorWriter &operator=(const VulkanDescriptorWriter &) = delete;
        VulkanDescriptorWriter(VulkanDescriptorWriter &&) noexcept = default;
        VulkanDescriptorWriter &operator=(VulkanDescriptorWriter &&) noexcept = default;

        VulkanDescriptorWriter &writeStorageImage(VkDescriptorSet set, uint32_t binding, const VulkanImageView &view,
                                                  ImageLayout layout);
        VulkanDescriptorWriter &writeStorageImage(VkDescriptorSet set, uint32_t binding,
                                                  const VulkanRenderTarget &target, ImageLayout layout);
        VulkanDescriptorWriter &writeUniformBuffer(VkDescriptorSet set, uint32_t binding, const VulkanBuffer &buffer,
                                                   uint64_t offset = 0, uint64_t range = 0);
        VulkanDescriptorWriter &writeStorageBuffer(VkDescriptorSet set, uint32_t binding, const VulkanBuffer &buffer,
                                                   uint64_t offset = 0, uint64_t range = 0);

        void commit();

        size_t pendingCount() const { return pending_.size(); }

    private:
        enum class InfoKind : uint8_t {
            Image,
            Buffer,
        };

        struct Pending {
            VkDescriptorSet set;
            uint32_t binding;
            VkDescriptorType type;
            InfoKind kind;
            uint32_t infoIndex;
        };

        VulkanDescriptorWriter &pushBufferWrite(VkDescriptorSet set, uint32_t binding, VkDescriptorType type,
                                                VkBuffer buffer, uint64_t offset, uint64_t range);

        VkDevice device_ = VK_NULL_HANDLE;
        std::vector<VkDescriptorImageInfo> imageInfos_;
        std::vector<VkDescriptorBufferInfo> bufferInfos_;
        std::vector<Pending> pending_;
    };
}
