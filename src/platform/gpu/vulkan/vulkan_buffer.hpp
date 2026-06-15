#pragma once

#include <cstdint>
#include <utility>
#include <vk_mem_alloc.h>
#include "gpu_queue_kind.hpp"
#include "gpu_types.hpp"

namespace RAGE {
    class VulkanAllocator;
    template <QueueKind K> class VulkanRecorder;

    class VulkanBufferCore {
    public:
        VulkanBufferCore() = default;
        ~VulkanBufferCore() { destroy(); }

        VulkanBufferCore(const VulkanBufferCore &) = delete;
        VulkanBufferCore &operator=(const VulkanBufferCore &) = delete;

        VulkanBufferCore(VulkanBufferCore &&other) noexcept { swap(other); }
        VulkanBufferCore &operator=(VulkanBufferCore &&other) noexcept {
            if (this != &other) {
                destroy();
                swap(other);
            }

            return *this;
        }

        uint64_t size() const { return size_; }
        BufferUsage usage() const { return usage_; }
        void *mappedData() const { return mappedPtr_; }

    private:
        friend class VulkanAllocator;
        friend class VulkanBuffer;
        friend class VulkanDescriptorWriter;
        template <QueueKind K> friend class VulkanRecorder;

        VulkanBufferCore(VmaAllocator allocator, BufferCreateInfo info, VkBuffer buffer, VmaAllocation allocation,
                         void *mapped)
            : allocator_(allocator)
            , buffer_(buffer)
            , allocation_(allocation)
            , mappedPtr_(mapped)
            , size_(info.size)
            , usage_(info.usage) {}

        void destroy() {
            if (buffer_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
                vmaDestroyBuffer(allocator_, buffer_, allocation_);
                buffer_ = VK_NULL_HANDLE;
                allocation_ = VK_NULL_HANDLE;
                allocator_ = VK_NULL_HANDLE;
            }
        }

        void swap(VulkanBufferCore &other) noexcept {
            std::swap(allocator_, other.allocator_);
            std::swap(buffer_, other.buffer_);
            std::swap(allocation_, other.allocation_);
            std::swap(mappedPtr_, other.mappedPtr_);
            std::swap(size_, other.size_);
            std::swap(usage_, other.usage_);
        }

        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        void *mappedPtr_ = nullptr;
        uint64_t size_ = 0;
        BufferUsage usage_ = BufferUsage::Uniform;
    };

    class VulkanBuffer {
    public:
        VulkanBuffer() = delete;

        VulkanBuffer(const VulkanBuffer &) = delete;
        VulkanBuffer &operator=(const VulkanBuffer &) = delete;
        VulkanBuffer(VulkanBuffer &&) noexcept = default;
        VulkanBuffer &operator=(VulkanBuffer &&) noexcept = default;

        uint64_t size() const { return core_.size(); }
        BufferUsage usage() const { return core_.usage(); }
        void *mappedData() const { return core_.mappedData(); }
        uint64_t deviceAddress() const { return deviceAddr_; }

    private:
        friend class VulkanAllocator;
        friend class VulkanDescriptorWriter;
        template <QueueKind K> friend class VulkanRecorder;

        VulkanBuffer(VmaAllocator allocator, BufferCreateInfo info, VkBuffer buffer, VmaAllocation allocation,
                     void *mapped, uint64_t deviceAddr)
            : core_(allocator, info, buffer, allocation, mapped)
            , deviceAddr_(deviceAddr) {}

        VulkanBufferCore core_;
        uint64_t deviceAddr_ = 0;
    };
}
