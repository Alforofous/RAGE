#pragma once

#include <cstdint>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include "gpu_queue_kind.hpp"
#include "gpu_semaphore_pool.hpp"

namespace RAGE {
    class VulkanSemaphorePool;

    class VulkanSemaphoreHandle {
    public:
        VulkanSemaphoreHandle() = default;
        ~VulkanSemaphoreHandle() noexcept { release(); }

        VulkanSemaphoreHandle(const VulkanSemaphoreHandle &other) noexcept
            : pool_(other.pool_)
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {
            retain();
        }

        VulkanSemaphoreHandle(VulkanSemaphoreHandle &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {}

        VulkanSemaphoreHandle &operator=(const VulkanSemaphoreHandle &other) noexcept {
            if (this != &other) {
                release();
                pool_ = other.pool_;
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
                retain();
            }

            return *this;
        }

        VulkanSemaphoreHandle &operator=(VulkanSemaphoreHandle &&other) noexcept {
            if (this != &other) {
                release();
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
            }

            return *this;
        }

        bool isValid() const { return pool_ != nullptr; }

    private:
        friend class VulkanSemaphorePool;
        friend class VulkanSwapchain;
        template <QueueKind K> friend class VulkanQueue;

        VulkanSemaphoreHandle(VulkanSemaphorePool *pool, uint32_t slotIndex, uint64_t generation)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation) {}

        void retain() noexcept;
        void release() noexcept;

        VkSemaphore vulkanHandle() const;

        VulkanSemaphorePool *pool_ = nullptr;
        uint32_t slotIndex_ = 0;
        uint64_t generation_ = 0;
    };

    class VulkanSemaphorePool {
    public:
        using Handle = VulkanSemaphoreHandle;

        VulkanSemaphorePool() = delete;
        explicit VulkanSemaphorePool(VkDevice device);
        ~VulkanSemaphorePool();

        VulkanSemaphorePool(const VulkanSemaphorePool &) = delete;
        VulkanSemaphorePool &operator=(const VulkanSemaphorePool &) = delete;
        VulkanSemaphorePool(VulkanSemaphorePool &&) = delete;
        VulkanSemaphorePool &operator=(VulkanSemaphorePool &&) = delete;

        VulkanSemaphoreHandle acquire();

    private:
        friend class VulkanSemaphoreHandle;

        static constexpr uint32_t kInvalidSlot = ~0u;

        struct Slot {
            VkSemaphore handle = VK_NULL_HANDLE;
            uint64_t generation = 0;
            uint32_t refcount = 0;
            uint32_t nextFreeIdx = kInvalidSlot;
        };

        void retainSlot(uint32_t slotIndex) noexcept;
        void releaseSlot(uint32_t slotIndex) noexcept;
        VkSemaphore slotHandle(uint32_t slotIndex, uint64_t generation) const;

        VkDevice device_ = VK_NULL_HANDLE;
        std::vector<Slot> slots_;
        uint32_t freeHead_ = kInvalidSlot;
    };

    static_assert(GpuSemaphorePool<VulkanSemaphorePool>, "VulkanSemaphorePool must satisfy GpuSemaphorePool concept");
}
