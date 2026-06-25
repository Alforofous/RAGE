#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vulkan/vulkan.h>
#include "gpu_queue.hpp"
#include "gpu_queue_kind.hpp"
#include "gpu_types.hpp"
#include "shared/logger.hpp"
#include "shared/small_vector.hpp"
#include "vulkan_buffer.hpp"
#include "vulkan_fence_pool.hpp"
#include "vulkan_image.hpp"
#include "vulkan_semaphore_pool.hpp"

namespace RAGE {
    template <QueueKind K> class VulkanCommandPool;
    template <QueueKind K> class VulkanQueue;
    template <QueueKind K> class VulkanRecorder;
    template <QueueKind K> class VulkanExecutable;
    template <QueueKind K> class VulkanPendingSubmission;

    struct VulkanImageBarrier {
        VkImage image = VK_NULL_HANDLE;
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageLayout oldLayout = ImageLayout::Undefined;
        ImageLayout newLayout = ImageLayout::Undefined;
        PipelineStage srcStage = PipelineStage::TopOfPipe;
        PipelineStage dstStage = PipelineStage::BottomOfPipe;
        AccessFlags srcAccess = AccessFlags::None;
        AccessFlags dstAccess = AccessFlags::None;
        uint32_t baseMipLevel = 0;
        uint32_t mipCount = 1;
        uint32_t baseArrayLayer = 0;
        uint32_t layerCount = 1;
    };

    inline VulkanImageBarrier imageBarrierFor(VkImage image, VkImageAspectFlags aspectMask) {
        return { .image = image, .aspectMask = aspectMask };
    }

    inline VulkanImageBarrier imageBarrierFor(const VulkanImage &image) {
        VulkanImageBarrier b{};
        b.image = image.handle();
        switch (image.format()) {
            case ImageFormat::D32_SFLOAT:
                b.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                break;
            case ImageFormat::D24_UNORM_S8_UINT:
            case ImageFormat::D32_SFLOAT_S8_UINT:
                b.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
                break;
            default:
                b.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                break;
        }

        return b;
    }

    template <QueueKind K> class VulkanCommandBuffer {
    public:
        using Kind = K;

        VulkanCommandBuffer() = delete;

        VulkanCommandBuffer(const VulkanCommandBuffer &) = delete;
        VulkanCommandBuffer &operator=(const VulkanCommandBuffer &) = delete;

        VulkanCommandBuffer(VulkanCommandBuffer &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {}

        VulkanCommandBuffer &operator=(VulkanCommandBuffer &&other) noexcept {
            if (this != &other) {
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
            }

            return *this;
        }

        VulkanRecorder<K> begin() &&;

    private:
        friend class VulkanCommandPool<K>;
        friend class VulkanPendingSubmission<K>;

        VulkanCommandBuffer(VulkanCommandPool<K> *pool, size_t slotIndex, uint64_t generation)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation) {}

        VulkanCommandPool<K> *pool_ = nullptr;
        size_t slotIndex_ = 0;
        uint64_t generation_ = 0;
    };

    template <QueueKind K> class VulkanRecorder {
    public:
        using Kind = K;

        VulkanRecorder() = delete;

        VulkanRecorder(const VulkanRecorder &) = delete;
        VulkanRecorder &operator=(const VulkanRecorder &) = delete;

        VulkanRecorder(VulkanRecorder &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {}

        VulkanRecorder &operator=(VulkanRecorder &&other) noexcept {
            if (this != &other) {
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
            }

            return *this;
        }

        VulkanExecutable<K> end() &&;

        void bindPipeline(PipelineBindPoint bindPoint, VkPipeline pipeline);
        void bindDescriptorSets(PipelineBindPoint bindPoint, VkPipelineLayout layout, uint32_t firstSet,
                                std::span<const VkDescriptorSet> sets);
        void pushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, uint32_t offset,
                           std::span<const std::byte> data);
        void clearColorImage(VkImage image, ImageLayout layout, float r, float g, float b, float a);
        void blitImage(VkImage src, ImageLayout srcLayout, uint32_t srcWidth, uint32_t srcHeight, VkImage dst,
                       ImageLayout dstLayout, uint32_t dstWidth, uint32_t dstHeight);
        void dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
            requires Supports<K, queue_kind::Compute>;
        void pipelineBarrier(std::span<const VulkanImageBarrier> imageBarriers);
        void copyBuffer(const VulkanBuffer &src, VulkanBuffer &dst, std::span<const BufferCopy> regions);
        void copyBufferToImage(const VulkanBuffer &src, VulkanImage &dst, ImageLayout dstLayout,
                               std::span<const BufferImageCopy> regions);
        void copyImageToBuffer(VkImage src, ImageLayout srcLayout, VulkanBuffer &dst,
                               std::span<const BufferImageCopy> regions);

        VkCommandBuffer rawHandle() const { return handle_(); }

    private:
        friend class VulkanCommandBuffer<K>;

        VulkanRecorder(VulkanCommandPool<K> *pool, size_t slotIndex, uint64_t generation)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation) {}

        VkCommandBuffer handle_() const;

        VulkanCommandPool<K> *pool_ = nullptr;
        size_t slotIndex_ = 0;
        uint64_t generation_ = 0;
    };

    template <QueueKind K> class VulkanExecutable {
    public:
        using Kind = K;

        VulkanExecutable() = delete;

        VulkanExecutable(const VulkanExecutable &) = delete;
        VulkanExecutable &operator=(const VulkanExecutable &) = delete;

        VulkanExecutable(VulkanExecutable &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {}

        VulkanExecutable &operator=(VulkanExecutable &&other) noexcept {
            if (this != &other) {
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
            }

            return *this;
        }

    private:
        friend class VulkanRecorder<K>;
        friend class VulkanQueue<K>;

        VulkanExecutable(VulkanCommandPool<K> *pool, size_t slotIndex, uint64_t generation)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation) {}

        VulkanCommandPool<K> *pool_ = nullptr;
        size_t slotIndex_ = 0;
        uint64_t generation_ = 0;
    };

    template <QueueKind K> class VulkanPendingSubmission {
    public:
        using Kind = K;

        VulkanPendingSubmission() = delete;

        VulkanPendingSubmission(const VulkanPendingSubmission &) = delete;
        VulkanPendingSubmission &operator=(const VulkanPendingSubmission &) = delete;

        VulkanPendingSubmission(VulkanPendingSubmission &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_)
            , fence_(std::move(other.fence_))
            , retainedSemaphores_(std::move(other.retainedSemaphores_)) {}

        VulkanPendingSubmission &operator=(VulkanPendingSubmission &&other) noexcept {
            if (this != &other) {
                assert(pool_ == nullptr);
                drainAndReset();
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
                fence_ = std::move(other.fence_);
                retainedSemaphores_ = std::move(other.retainedSemaphores_);
            }

            return *this;
        }

        ~VulkanPendingSubmission() {
            if (pool_ != nullptr && fence_.isValid() && !fence_.isSignaled()) {
                Core::log(Core::LogLevel::Warn, "VulkanPendingSubmission destroyed before wait(); stalling on fence");
            }
            drainAndReset();
        }

        VulkanCommandBuffer<K> wait() &&;

    private:
        friend class VulkanQueue<K>;

        VulkanPendingSubmission(VulkanCommandPool<K> *pool, size_t slotIndex, uint64_t generation,
                                VulkanFenceHandle fence, Core::SmallVector<VulkanSemaphoreHandle, 4> retained)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation)
            , fence_(std::move(fence))
            , retainedSemaphores_(std::move(retained)) {}

        void drainAndReset() noexcept;

        VulkanCommandPool<K> *pool_ = nullptr;
        size_t slotIndex_ = 0;
        uint64_t generation_ = 0;
        VulkanFenceHandle fence_;
        Core::SmallVector<VulkanSemaphoreHandle, 4> retainedSemaphores_;
    };

    static_assert(GpuCommandBuffer<VulkanCommandBuffer<queue_kind::Graphics>>);
    static_assert(GpuRecorder<VulkanRecorder<queue_kind::Graphics>>);
    static_assert(GpuExecutable<VulkanExecutable<queue_kind::Graphics>>);
    static_assert(GpuPendingSubmission<VulkanPendingSubmission<queue_kind::Graphics>>);
}
