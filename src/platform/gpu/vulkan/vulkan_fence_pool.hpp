#pragma once

#include <cstdint>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include "gpu_fence.hpp"
#include "gpu_queue_kind.hpp"

namespace RAGE {
    class VulkanFencePool;

    class VulkanFenceHandle {
    public:
        VulkanFenceHandle() = default;
        ~VulkanFenceHandle() noexcept { release(); }

        VulkanFenceHandle(const VulkanFenceHandle &) = delete;
        VulkanFenceHandle &operator=(const VulkanFenceHandle &) = delete;

        VulkanFenceHandle(VulkanFenceHandle &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {}

        VulkanFenceHandle &operator=(VulkanFenceHandle &&other) noexcept {
            if (this != &other) {
                release();
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
            }

            return *this;
        }

        void wait(uint64_t timeoutNs = UINT64_MAX);
        bool isSignaled() const;
        bool isValid() const { return pool_ != nullptr; }

    private:
        friend class VulkanFencePool;
        template <QueueKind K> friend class VulkanQueue;

        VulkanFenceHandle(VulkanFencePool *pool, uint32_t slotIndex, uint64_t generation)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation) {}

        void release() noexcept;
        VkFence vulkanHandle() const;

        VulkanFencePool *pool_ = nullptr;
        uint32_t slotIndex_ = 0;
        uint64_t generation_ = 0;
    };

    static_assert(GpuFenceHandle<VulkanFenceHandle>, "VulkanFenceHandle must satisfy GpuFenceHandle concept");

    class VulkanFencePool {
    public:
        using Handle = VulkanFenceHandle;

        VulkanFencePool() = delete;
        explicit VulkanFencePool(VkDevice device);
        ~VulkanFencePool();

        VulkanFencePool(const VulkanFencePool &) = delete;
        VulkanFencePool &operator=(const VulkanFencePool &) = delete;
        VulkanFencePool(VulkanFencePool &&) = delete;
        VulkanFencePool &operator=(VulkanFencePool &&) = delete;

        VulkanFenceHandle acquire();

    private:
        friend class VulkanFenceHandle;

        static constexpr uint32_t kInvalidSlot = ~0u;

        struct Slot {
            VkFence handle = VK_NULL_HANDLE;
            uint64_t generation = 0;
            uint32_t nextFreeIdx = kInvalidSlot;
            bool inUse = false;
        };

        void releaseSlot(uint32_t slotIndex) noexcept;
        VkFence slotHandle(uint32_t slotIndex, uint64_t generation) const;

        VkDevice device_ = VK_NULL_HANDLE;
        std::vector<Slot> slots_;
        uint32_t freeHead_ = kInvalidSlot;
    };

    static_assert(GpuFencePool<VulkanFencePool>, "VulkanFencePool must satisfy GpuFencePool concept");
}
