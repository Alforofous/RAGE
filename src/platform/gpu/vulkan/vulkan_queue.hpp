#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <vulkan/vulkan.h>
#include "gpu_queue.hpp"
#include "gpu_queue_kind.hpp"
#include "gpu_types.hpp"
#include "shared/small_vector.hpp"
#include "vulkan_command_pool.hpp"
#include "vulkan_fence_pool.hpp"
#include "vulkan_semaphore_pool.hpp"
#include "vulkan_type_map.hpp"

namespace RAGE {
    struct VulkanSemaphoreWait {
        VulkanSemaphoreHandle semaphore;
        PipelineStage stage = PipelineStage::None;
    };

    struct VulkanSubmitInfo {
        std::span<const VulkanSemaphoreWait> wait = {};
        std::span<const VulkanSemaphoreHandle> signal = {};
    };

    template <QueueKind K> class VulkanQueue {
    public:
        using Kind = K;
        using CommandBuffer = VulkanCommandBuffer<K>;
        using Executable = VulkanExecutable<K>;
        using Semaphore = VulkanSemaphoreHandle;
        using SubmitInfo = VulkanSubmitInfo;
        using PendingSubmission = VulkanPendingSubmission<K>;

        VulkanQueue() = delete;

        VulkanQueue(VkQueue queue, uint32_t queueFamily, VulkanFencePool &fencePool)
            : queue_(queue)
            , queueFamily_(queueFamily)
            , fencePool_(&fencePool) {}

        VulkanQueue(const VulkanQueue &) = delete;
        VulkanQueue &operator=(const VulkanQueue &) = delete;
        VulkanQueue(VulkanQueue &&) = delete;
        VulkanQueue &operator=(VulkanQueue &&) = delete;

        VulkanPendingSubmission<K> submit(VulkanExecutable<K> exe, VulkanSubmitInfo info = {}) {
            Core::SmallVector<VkSemaphore, 4> waitHandles;
            Core::SmallVector<VkPipelineStageFlags, 4> waitStages;
            waitHandles.reserve(info.wait.size());
            waitStages.reserve(info.wait.size());
            for (const VulkanSemaphoreWait &w : info.wait) {
                waitHandles.push_back(w.semaphore.vulkanHandle());
                waitStages.push_back(toVkPipelineStageFlags(w.stage));
            }

            Core::SmallVector<VkSemaphore, 4> signalHandles;
            signalHandles.reserve(info.signal.size());
            for (const VulkanSemaphoreHandle &s : info.signal) {
                signalHandles.push_back(s.vulkanHandle());
            }

            Core::SmallVector<VulkanSemaphoreHandle, 4> retained;
            retained.reserve(info.wait.size() + info.signal.size());
            for (const VulkanSemaphoreWait &w : info.wait) {
                retained.push_back(w.semaphore);
            }
            for (const VulkanSemaphoreHandle &s : info.signal) {
                retained.push_back(s);
            }

            VkCommandBuffer cmdHandle = exe.pool_->slotHandle(exe.slotIndex_, exe.generation_);

            VulkanFenceHandle fence = fencePool_->acquire();

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdHandle;
            submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitHandles.size());
            submitInfo.pWaitSemaphores = waitHandles.data();
            submitInfo.pWaitDstStageMask = waitStages.data();
            submitInfo.signalSemaphoreCount = static_cast<uint32_t>(signalHandles.size());
            submitInfo.pSignalSemaphores = signalHandles.data();

            if (vkQueueSubmit(queue_, 1, &submitInfo, fence.vulkanHandle()) != VK_SUCCESS) {
                exe.pool_->free_.push_back(exe.slotIndex_);
                exe.pool_ = nullptr;
                throw std::runtime_error("Failed to submit queue");
            }

            exe.pool_->outstandingSubmissions_++;
            VulkanPendingSubmission<K> pending(exe.pool_, exe.slotIndex_, exe.generation_, std::move(fence),
                                               std::move(retained));
            exe.pool_ = nullptr;

            return pending;
        }

        void waitIdle() const {
            if (vkQueueWaitIdle(queue_) != VK_SUCCESS) {
                throw std::runtime_error("Failed to wait for queue idle");
            }
        }

        uint32_t queueFamily() const { return queueFamily_; }

    private:
        friend class VulkanSwapchain;

        VkQueue vkQueue() const { return queue_; }

        VkQueue queue_ = VK_NULL_HANDLE;
        uint32_t queueFamily_ = 0;
        VulkanFencePool *fencePool_ = nullptr;
    };

    static_assert(GpuQueue<VulkanQueue<queue_kind::Graphics>>, "VulkanQueue<Graphics> must satisfy GpuQueue concept");
}
