#include "vulkan_fence_pool.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "shared/logger.hpp"

namespace RAGE {
    void VulkanFenceHandle::wait(uint64_t timeoutNs) {
        if (pool_ == nullptr) {
            throw std::runtime_error("VulkanFenceHandle::wait() on invalid handle");
        }
        VkDevice device = pool_->device_;
        VkFence fence = pool_->slotHandle(slotIndex_, generation_);
        if (vkWaitForFences(device, 1, &fence, VK_TRUE, timeoutNs) != VK_SUCCESS) {
            throw std::runtime_error("Failed to wait for fence");
        }
    }

    bool VulkanFenceHandle::isSignaled() const {
        if (pool_ == nullptr) {
            return false;
        }
        VkDevice device = pool_->device_;
        VkFence fence = pool_->slotHandle(slotIndex_, generation_);

        return vkGetFenceStatus(device, fence) == VK_SUCCESS;
    }

    void VulkanFenceHandle::release() noexcept {
        if (pool_ != nullptr) {
            pool_->releaseSlot(slotIndex_);
            pool_ = nullptr;
        }
    }

    VkFence VulkanFenceHandle::vulkanHandle() const {
        assert(pool_ != nullptr);
        return pool_->slotHandle(slotIndex_, generation_);
    }

    VulkanFencePool::VulkanFencePool(VkDevice device)
        : device_(device) {}

    VulkanFencePool::~VulkanFencePool() {
        uint32_t outstanding = 0;
        for (const Slot &slot : slots_) {
            if (slot.inUse) {
                outstanding++;
            }
        }
        if (outstanding > 0) {
            char msg[160];
            std::snprintf(msg, sizeof(msg),
                          "VulkanFencePool destroyed with %u outstanding handles. "
                          "Aborting before handle destructors hit freed memory.",
                          outstanding);
            Core::log(Core::LogLevel::Error, msg);
            vkDeviceWaitIdle(device_);
            std::abort();
        }
        for (const Slot &slot : slots_) {
            if (slot.handle != VK_NULL_HANDLE) {
                vkDestroyFence(device_, slot.handle, nullptr);
            }
        }
    }

    VulkanFenceHandle VulkanFencePool::acquire() {
        uint32_t slotIndex = 0;

        if (freeHead_ != kInvalidSlot) {
            slotIndex = freeHead_;
            freeHead_ = slots_[slotIndex].nextFreeIdx;
            if (vkResetFences(device_, 1, &slots_[slotIndex].handle) != VK_SUCCESS) {
                throw std::runtime_error("Failed to reset fence");
            }
        } else {
            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = 0;

            VkFence fence = VK_NULL_HANDLE;
            if (vkCreateFence(device_, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create fence");
            }

            slots_.push_back({ .handle = fence, .generation = 0, .nextFreeIdx = kInvalidSlot, .inUse = false });
            slotIndex = static_cast<uint32_t>(slots_.size() - 1);
        }

        slots_[slotIndex].inUse = true;

        return { this, slotIndex, slots_[slotIndex].generation };
    }

    void VulkanFencePool::releaseSlot(uint32_t slotIndex) noexcept {
        assert(slotIndex < slots_.size());
        assert(slots_[slotIndex].inUse);

        slots_[slotIndex].inUse = false;
        slots_[slotIndex].generation++;
        slots_[slotIndex].nextFreeIdx = freeHead_;
        freeHead_ = slotIndex;
    }

    VkFence VulkanFencePool::slotHandle(uint32_t slotIndex, uint64_t generation) const {
        if (slotIndex >= slots_.size()) {
            throw std::runtime_error("VulkanFencePool::slotHandle out-of-range slot index");
        }
        if (generation != slots_[slotIndex].generation) {
            throw std::runtime_error("VulkanFencePool::slotHandle stale generation");
        }

        return slots_[slotIndex].handle;
    }
}
