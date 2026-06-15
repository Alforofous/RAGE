#include "vulkan_semaphore_pool.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "shared/logger.hpp"

namespace RAGE {
    void VulkanSemaphoreHandle::retain() noexcept {
        if (pool_ != nullptr) {
            pool_->retainSlot(slotIndex_);
        }
    }

    void VulkanSemaphoreHandle::release() noexcept {
        if (pool_ != nullptr) {
            pool_->releaseSlot(slotIndex_);
            pool_ = nullptr;
        }
    }

    VkSemaphore VulkanSemaphoreHandle::vulkanHandle() const {
        assert(pool_ != nullptr);
        return pool_->slotHandle(slotIndex_, generation_);
    }

    VulkanSemaphorePool::VulkanSemaphorePool(VkDevice device)
        : device_(device) {}

    VulkanSemaphorePool::~VulkanSemaphorePool() {
        uint32_t outstanding = 0;
        for (const Slot &slot : slots_) {
            if (slot.refcount > 0) {
                outstanding++;
            }
        }
        if (outstanding > 0) {
            char msg[160];
            std::snprintf(msg, sizeof(msg),
                          "VulkanSemaphorePool destroyed with %u outstanding handles. "
                          "Aborting before handle destructors hit freed memory.",
                          outstanding);
            Core::log(Core::LogLevel::Error, msg);
            vkDeviceWaitIdle(device_);
            std::abort();
        }
        for (const Slot &slot : slots_) {
            if (slot.handle != VK_NULL_HANDLE) {
                vkDestroySemaphore(device_, slot.handle, nullptr);
            }
        }
    }

    VulkanSemaphoreHandle VulkanSemaphorePool::acquire() {
        uint32_t slotIndex = 0;

        if (freeHead_ != kInvalidSlot) {
            slotIndex = freeHead_;
            freeHead_ = slots_[slotIndex].nextFreeIdx;
        } else {
            VkSemaphoreCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkSemaphore semaphore = VK_NULL_HANDLE;
            if (vkCreateSemaphore(device_, &info, nullptr, &semaphore) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create semaphore");
            }

            slots_.push_back({ .handle = semaphore, .generation = 0, .refcount = 0, .nextFreeIdx = kInvalidSlot });
            slotIndex = static_cast<uint32_t>(slots_.size() - 1);
        }

        slots_[slotIndex].refcount = 1;

        return { this, slotIndex, slots_[slotIndex].generation };
    }

    void VulkanSemaphorePool::retainSlot(uint32_t slotIndex) noexcept {
        assert(slotIndex < slots_.size());
        assert(slots_[slotIndex].refcount > 0);
        slots_[slotIndex].refcount++;
    }

    void VulkanSemaphorePool::releaseSlot(uint32_t slotIndex) noexcept {
        assert(slotIndex < slots_.size());
        assert(slots_[slotIndex].refcount > 0);

        if (--slots_[slotIndex].refcount == 0) {
            slots_[slotIndex].generation++;
            slots_[slotIndex].nextFreeIdx = freeHead_;
            freeHead_ = slotIndex;
        }
    }

    VkSemaphore VulkanSemaphorePool::slotHandle(uint32_t slotIndex, uint64_t generation) const {
        if (slotIndex >= slots_.size()) {
            throw std::runtime_error("VulkanSemaphorePool::slotHandle out-of-range slot index");
        }
        if (generation != slots_[slotIndex].generation) {
            throw std::runtime_error("VulkanSemaphorePool::slotHandle stale generation");
        }

        return slots_[slotIndex].handle;
    }
}
