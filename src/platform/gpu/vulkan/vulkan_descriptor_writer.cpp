#include "vulkan_descriptor_writer.hpp"
#include <stdexcept>
#include <vector>
#include "vulkan_buffer.hpp"
#include "vulkan_image.hpp"
#include "vulkan_render_target.hpp"
#include "vulkan_type_map.hpp"

namespace RAGE {
    VulkanDescriptorWriter::VulkanDescriptorWriter(VkDevice device)
        : device_(device) {
        if (device_ == VK_NULL_HANDLE) {
            throw std::runtime_error("VulkanDescriptorWriter: null device");
        }
    }

    VulkanDescriptorWriter &VulkanDescriptorWriter::writeStorageImage(VkDescriptorSet set, uint32_t binding,
                                                                      const VulkanImageView &view, ImageLayout layout) {
        VkDescriptorImageInfo info{};
        info.imageView = view.view_;
        info.imageLayout = toVkImageLayout(layout);

        imageInfos_.push_back(info);
        pending_.push_back({ .set = set,
                             .binding = binding,
                             .arrayElement = 0,
                             .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                             .kind = InfoKind::Image,
                             .infoIndex = static_cast<uint32_t>(imageInfos_.size() - 1) });

        return *this;
    }

    VulkanDescriptorWriter &VulkanDescriptorWriter::writeStorageImage(VkDescriptorSet set, uint32_t binding,
                                                                      const VulkanRenderTarget &target,
                                                                      ImageLayout layout) {
        return writeStorageImage(set, binding, target.view(), layout);
    }

    VulkanDescriptorWriter &VulkanDescriptorWriter::pushBufferWrite(VkDescriptorSet set, uint32_t binding,
                                                                    uint32_t arrayElement, VkDescriptorType type,
                                                                    VkBuffer buffer, uint64_t offset, uint64_t range) {
        VkDescriptorBufferInfo info{};
        info.buffer = buffer;
        info.offset = offset;
        info.range = (range == 0) ? VK_WHOLE_SIZE : range;

        bufferInfos_.push_back(info);
        pending_.push_back({ .set = set,
                             .binding = binding,
                             .arrayElement = arrayElement,
                             .type = type,
                             .kind = InfoKind::Buffer,
                             .infoIndex = static_cast<uint32_t>(bufferInfos_.size() - 1) });

        return *this;
    }

    VulkanDescriptorWriter &VulkanDescriptorWriter::writeUniformBuffer(VkDescriptorSet set, uint32_t binding,
                                                                       const VulkanBuffer &buffer, uint64_t offset,
                                                                       uint64_t range) {
        return pushBufferWrite(set, binding, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffer.core_.buffer_, offset, range);
    }

    VulkanDescriptorWriter &VulkanDescriptorWriter::writeStorageBuffer(VkDescriptorSet set, uint32_t binding,
                                                                       const VulkanBuffer &buffer, uint64_t offset,
                                                                       uint64_t range) {
        return pushBufferWrite(set, binding, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffer.core_.buffer_, offset, range);
    }

    VulkanDescriptorWriter &VulkanDescriptorWriter::writeStorageBufferArray(VkDescriptorSet set, uint32_t binding,
                                                                            uint32_t arrayElement,
                                                                            const VulkanBuffer &buffer, uint64_t offset,
                                                                            uint64_t range) {
        return pushBufferWrite(set, binding, arrayElement, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffer.core_.buffer_,
                               offset, range);
    }

    void VulkanDescriptorWriter::commit() {
        if (pending_.empty()) {
            return;
        }

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(pending_.size());

        for (const Pending &p : pending_) {
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = p.set;
            w.dstBinding = p.binding;
            w.dstArrayElement = p.arrayElement;
            w.descriptorCount = 1;
            w.descriptorType = p.type;
            if (p.kind == InfoKind::Image) {
                w.pImageInfo = &imageInfos_[p.infoIndex];
            } else {
                w.pBufferInfo = &bufferInfos_[p.infoIndex];
            }
            writes.push_back(w);
        }

        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        pending_.clear();
        imageInfos_.clear();
        bufferInfos_.clear();
    }
}
