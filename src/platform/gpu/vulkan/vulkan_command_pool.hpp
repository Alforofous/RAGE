#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.h>
#include "shared/logger.hpp"
#include "shared/small_vector.hpp"
#include "gpu_command_pool.hpp"
#include "gpu_queue_kind.hpp"
#include "gpu_types.hpp"
#include "vulkan_buffer.hpp"
#include "vulkan_command_buffer.hpp"
#include "vulkan_image.hpp"
#include "vulkan_type_map.hpp"

namespace RAGE {
    template <QueueKind K> class VulkanCommandPool {
    public:
        using Kind = K;
        using CommandBuffer = VulkanCommandBuffer<K>;

        VulkanCommandPool() = delete;

        VulkanCommandPool(VkDevice device, uint32_t queueFamily)
            : device_(device) {
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = queueFamily;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            if (vkCreateCommandPool(device_, &poolInfo, nullptr, &pool_) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create command pool");
            }
        }

        ~VulkanCommandPool() {
            if (outstandingSubmissions_ != 0) {
                Core::log(Core::LogLevel::Warn,
                          "VulkanCommandPool destroyed with outstanding submissions; draining device");
                vkDeviceWaitIdle(device_);
            }
            if (pool_ != VK_NULL_HANDLE) {
                vkDestroyCommandPool(device_, pool_, nullptr);
            }
        }

        VulkanCommandPool(const VulkanCommandPool &) = delete;
        VulkanCommandPool &operator=(const VulkanCommandPool &) = delete;
        VulkanCommandPool(VulkanCommandPool &&) = delete;
        VulkanCommandPool &operator=(VulkanCommandPool &&) = delete;

        VulkanCommandBuffer<K> allocate() {
            size_t slotIndex = 0;

            if (!free_.empty()) {
                slotIndex = free_.back();
                free_.pop_back();
            } else {
                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = pool_;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;

                VkCommandBuffer handle = VK_NULL_HANDLE;
                if (vkAllocateCommandBuffers(device_, &allocInfo, &handle) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to allocate command buffer");
                }

                slots_.push_back({ handle });
                slotIndex = slots_.size() - 1;
            }

            return VulkanCommandBuffer<K>(this, slotIndex, generation_);
        }

        bool isIdle() const noexcept { return outstandingSubmissions_ == 0; }

        void reset() {
            if (outstandingSubmissions_ != 0) {
                Core::log(Core::LogLevel::Error,
                          "VulkanCommandPool::reset() called with outstanding submissions; draining device");
                assert(false);
                vkDeviceWaitIdle(device_);
                outstandingSubmissions_ = 0;
            }

            if (vkResetCommandPool(device_, pool_, 0) != VK_SUCCESS) {
                throw std::runtime_error("Failed to reset command pool");
            }

            generation_++;
            free_.clear();
            free_.reserve(slots_.size());
            for (size_t i = 0; i < slots_.size(); ++i) {
                free_.push_back(i);
            }
        }

    private:
        friend class VulkanCommandBuffer<K>;
        friend class VulkanRecorder<K>;
        friend class VulkanPendingSubmission<K>;
        friend class VulkanQueue<K>;

        struct Slot {
            VkCommandBuffer handle = VK_NULL_HANDLE;
        };

        void retireSubmission(size_t slotIndex, uint64_t generation, VulkanFenceHandle &fence) noexcept {
            const bool stale = generation_ != generation;
            bool waitFailed = false;

            try {
                if (!stale) {
                    if (fence.isValid()) {
                        fence.wait();
                    }
                    VkCommandBuffer handle = slotHandle(slotIndex, generation);
                    if (vkResetCommandBuffer(handle, 0) != VK_SUCCESS) {
                        Core::log(Core::LogLevel::Error, "Failed to reset command buffer after wait");
                    }
                }
            } catch (const std::exception &e) {
                Core::log(Core::LogLevel::Error, e.what());
                waitFailed = true;
            }

            if (!stale && waitFailed) {
                Core::log(Core::LogLevel::Warn, "Forcing vkDeviceWaitIdle after fence-wait failure");
                vkDeviceWaitIdle(device_);
            }

            if (!stale) {
                free_.push_back(slotIndex);
                outstandingSubmissions_--;
            }
        }

        VkCommandBuffer slotHandle(size_t index, uint64_t generation) const {
            if (index >= slots_.size()) {
                throw std::runtime_error("VulkanCommandPool::slotHandle out-of-range slot index");
            }
            if (generation != generation_) {
                throw std::runtime_error("VulkanCommandPool::slotHandle stale generation (pool was reset)");
            }

            return slots_[index].handle;
        }

        VkDevice device_ = VK_NULL_HANDLE;
        VkCommandPool pool_ = VK_NULL_HANDLE;
        std::vector<Slot> slots_;
        std::vector<size_t> free_;
        uint64_t generation_ = 0;
        uint32_t outstandingSubmissions_ = 0;
    };

    template <QueueKind K> VulkanRecorder<K> VulkanCommandBuffer<K>::begin() && {
        VkCommandBuffer handle = pool_->slotHandle(slotIndex_, generation_);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;

        if (vkBeginCommandBuffer(handle, &beginInfo) != VK_SUCCESS) {
            pool_->free_.push_back(slotIndex_);
            pool_ = nullptr;
            throw std::runtime_error("Failed to begin command buffer");
        }

        VulkanRecorder<K> rec(pool_, slotIndex_, generation_);
        pool_ = nullptr;

        return rec;
    }

    template <QueueKind K> VkCommandBuffer VulkanRecorder<K>::handle_() const {
        return pool_->slotHandle(slotIndex_, generation_);
    }

    template <QueueKind K> void VulkanRecorder<K>::bindPipeline(PipelineBindPoint bindPoint, VkPipeline pipeline) {
        vkCmdBindPipeline(handle_(), toVkPipelineBindPoint(bindPoint), pipeline);
    }

    template <QueueKind K>
    void VulkanRecorder<K>::bindDescriptorSets(PipelineBindPoint bindPoint, VkPipelineLayout layout, uint32_t firstSet,
                                               std::span<const VkDescriptorSet> sets) {
        vkCmdBindDescriptorSets(handle_(), toVkPipelineBindPoint(bindPoint), layout, firstSet,
                                static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
    }

    template <QueueKind K>
    void VulkanRecorder<K>::pushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, uint32_t offset,
                                          std::span<const std::byte> data) {
        if (data.empty()) {
            return;
        }
        vkCmdPushConstants(handle_(), layout, stages, offset, static_cast<uint32_t>(data.size()), data.data());
    }

    template <QueueKind K>
    void VulkanRecorder<K>::blitImage(VkImage src, ImageLayout srcLayout, uint32_t srcWidth, uint32_t srcHeight,
                                      VkImage dst, ImageLayout dstLayout, uint32_t dstWidth, uint32_t dstHeight) {
        VkImageBlit region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[1] = { .x = static_cast<int32_t>(srcWidth), .y = static_cast<int32_t>(srcHeight), .z = 1 };
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[1] = { .x = static_cast<int32_t>(dstWidth), .y = static_cast<int32_t>(dstHeight), .z = 1 };

        vkCmdBlitImage(handle_(), src, toVkImageLayout(srcLayout), dst, toVkImageLayout(dstLayout), 1, &region,
                       VK_FILTER_LINEAR);
    }

    template <QueueKind K>
    void VulkanRecorder<K>::clearColorImage(VkImage image, ImageLayout layout, float r, float g, float b, float a) {
        VkClearColorValue color{};
        color.float32[0] = r;
        color.float32[1] = g;
        color.float32[2] = b;
        color.float32[3] = a;

        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        vkCmdClearColorImage(handle_(), image, toVkImageLayout(layout), &color, 1, &range);
    }

    template <QueueKind K>
    void VulkanRecorder<K>::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
        requires Supports<K, queue_kind::Compute>
    {
        vkCmdDispatch(handle_(), groupsX, groupsY, groupsZ);
    }

    template <QueueKind K> void VulkanRecorder<K>::pipelineBarrier(std::span<const VulkanImageBarrier> imageBarriers) {
        if (imageBarriers.empty()) {
            return;
        }

        Core::SmallVector<VkImageMemoryBarrier, 4> vkBarriers;
        vkBarriers.reserve(imageBarriers.size());

        VkPipelineStageFlags srcStageMask = 0;
        VkPipelineStageFlags dstStageMask = 0;

        for (const VulkanImageBarrier &b : imageBarriers) {
            if (b.image == VK_NULL_HANDLE) {
                throw std::runtime_error("VulkanRecorder::pipelineBarrier: null image in barrier");
            }

            VkImageMemoryBarrier vb{};
            vb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            vb.srcAccessMask = toVkAccessFlags(b.srcAccess);
            vb.dstAccessMask = toVkAccessFlags(b.dstAccess);
            vb.oldLayout = toVkImageLayout(b.oldLayout);
            vb.newLayout = toVkImageLayout(b.newLayout);
            vb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            vb.image = b.image;
            vb.subresourceRange.aspectMask = b.aspectMask;
            vb.subresourceRange.baseMipLevel = b.baseMipLevel;
            vb.subresourceRange.levelCount = b.mipCount;
            vb.subresourceRange.baseArrayLayer = b.baseArrayLayer;
            vb.subresourceRange.layerCount = b.layerCount;

            vkBarriers.push_back(vb);
            srcStageMask |= toVkPipelineStageFlags(b.srcStage);
            dstStageMask |= toVkPipelineStageFlags(b.dstStage);
        }

        if (srcStageMask == 0) {
            srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        if (dstStageMask == 0) {
            dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(handle_(), srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr,
                             static_cast<uint32_t>(vkBarriers.size()), vkBarriers.data());
    }

    template <QueueKind K>
    void VulkanRecorder<K>::copyBuffer(const VulkanBuffer &src, VulkanBuffer &dst,
                                       std::span<const BufferCopy> regions) {
        if (regions.empty()) {
            return;
        }

        Core::SmallVector<VkBufferCopy, 4> vkRegions;
        vkRegions.reserve(regions.size());
        for (const BufferCopy &r : regions) {
            vkRegions.push_back({ .srcOffset = r.srcOffset, .dstOffset = r.dstOffset, .size = r.size });
        }

        vkCmdCopyBuffer(handle_(), src.core_.buffer_, dst.core_.buffer_, static_cast<uint32_t>(vkRegions.size()),
                        vkRegions.data());
    }

    template <QueueKind K>
    void VulkanRecorder<K>::copyBufferToImage(const VulkanBuffer &src, VulkanImage &dst, ImageLayout dstLayout,
                                              std::span<const BufferImageCopy> regions) {
        if (regions.empty()) {
            return;
        }

        Core::SmallVector<VkBufferImageCopy, 4> vkRegions;
        vkRegions.reserve(regions.size());

        const VkImageAspectFlags aspect = aspectFlagsForFormat(dst.info_.format);

        for (const BufferImageCopy &r : regions) {
            VkBufferImageCopy vr{};
            vr.bufferOffset = r.bufferOffset;
            vr.bufferRowLength = r.bufferRowLength;
            vr.bufferImageHeight = r.bufferImageHeight;
            vr.imageSubresource.aspectMask = aspect;
            vr.imageSubresource.mipLevel = r.mipLevel;
            vr.imageSubresource.baseArrayLayer = r.baseArrayLayer;
            vr.imageSubresource.layerCount = r.layerCount;
            vr.imageOffset = { .x = r.imageOffsetX, .y = r.imageOffsetY, .z = r.imageOffsetZ };
            vr.imageExtent = { .width = r.imageWidth, .height = r.imageHeight, .depth = r.imageDepth };
            vkRegions.push_back(vr);
        }

        vkCmdCopyBufferToImage(handle_(), src.core_.buffer_, dst.image_, toVkImageLayout(dstLayout),
                               static_cast<uint32_t>(vkRegions.size()), vkRegions.data());
    }

    template <QueueKind K>
    void VulkanRecorder<K>::copyImageToBuffer(VkImage src, ImageLayout srcLayout, VulkanBuffer &dst,
                                              std::span<const BufferImageCopy> regions) {
        if (regions.empty()) {
            return;
        }

        Core::SmallVector<VkBufferImageCopy, 4> vkRegions;
        vkRegions.reserve(regions.size());

        for (const BufferImageCopy &r : regions) {
            VkBufferImageCopy vr{};
            vr.bufferOffset = r.bufferOffset;
            vr.bufferRowLength = r.bufferRowLength;
            vr.bufferImageHeight = r.bufferImageHeight;
            vr.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            vr.imageSubresource.mipLevel = r.mipLevel;
            vr.imageSubresource.baseArrayLayer = r.baseArrayLayer;
            vr.imageSubresource.layerCount = r.layerCount;
            vr.imageOffset = { .x = r.imageOffsetX, .y = r.imageOffsetY, .z = r.imageOffsetZ };
            vr.imageExtent = { .width = r.imageWidth, .height = r.imageHeight, .depth = r.imageDepth };
            vkRegions.push_back(vr);
        }

        vkCmdCopyImageToBuffer(handle_(), src, toVkImageLayout(srcLayout), dst.core_.buffer_,
                               static_cast<uint32_t>(vkRegions.size()), vkRegions.data());
    }

    template <QueueKind K> VulkanExecutable<K> VulkanRecorder<K>::end() && {
        VkCommandBuffer handle = pool_->slotHandle(slotIndex_, generation_);

        if (vkEndCommandBuffer(handle) != VK_SUCCESS) {
            vkResetCommandBuffer(handle, 0);
            pool_->free_.push_back(slotIndex_);
            pool_ = nullptr;
            throw std::runtime_error("Failed to end command buffer");
        }

        VulkanExecutable<K> exe(pool_, slotIndex_, generation_);
        pool_ = nullptr;

        return exe;
    }

    template <QueueKind K> VulkanCommandBuffer<K> VulkanPendingSubmission<K>::wait() && {
        if (pool_ == nullptr) {
            throw std::runtime_error("VulkanPendingSubmission::wait() on empty submission");
        }
        if (pool_->generation_ != generation_) {
            pool_ = nullptr;
            throw std::runtime_error("VulkanPendingSubmission::wait() on stale submission (pool was reset)");
        }

        VulkanCommandPool<K> *const pool = pool_;
        const size_t slotIndex = slotIndex_;
        const uint64_t generation = generation_;

        drainAndReset();

        return VulkanCommandBuffer<K>(pool, slotIndex, generation);
    }

    static_assert(GpuCommandPool<VulkanCommandPool<queue_kind::Graphics>>);

    template <QueueKind K> void VulkanPendingSubmission<K>::drainAndReset() noexcept {
        if (pool_ == nullptr) {
            return;
        }
        pool_->retireSubmission(slotIndex_, generation_, fence_);
        pool_ = nullptr;
    }
}
