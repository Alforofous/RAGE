#pragma once

#include <cstdint>
#include <utility>
#include <vk_mem_alloc.h>
#include "gpu_types.hpp"
#include "../../shared/logger.hpp"

namespace RAGE {
    class VulkanBuffer {
public:
        VulkanBuffer() = default;

        VulkanBuffer(VmaAllocator allocator, BufferCreateInfo info, VkBuffer buffer, VmaAllocation allocation, void *mapped, uint64_t deviceAddr)
            : allocator_(allocator)
            , buffer_(buffer)
            , allocation_(allocation)
            , mappedPtr_(mapped)
            , size_(info.size)
            , usage_(info.usage)
            , deviceAddr_(deviceAddr) {}

        ~VulkanBuffer() { destroy(); }

        VulkanBuffer(const VulkanBuffer &) = delete;
        VulkanBuffer& operator=(const VulkanBuffer &) = delete;

        VulkanBuffer(VulkanBuffer &&other) noexcept { swap(other); }
        VulkanBuffer& operator=(VulkanBuffer &&other) noexcept {
            if (this != &other) {
                destroy();
                swap(other);
            }

            return *this;
        }

        uint64_t size() const { return size_; }
        BufferUsage usage() const { return usage_; }

        void *mappedData() const { return mappedPtr_; }

        uint64_t deviceAddress() const {
            if (!hasFlag(usage_, BufferUsage::DeviceAddress)) {
                log(LogLevel::Error, "deviceAddress() called on buffer created without Usage::DeviceAddress");

                return 0;
            }

            return deviceAddr_;
        }

        VkBuffer vulkanHandle() const { return buffer_; }

private:
        void destroy() {
            if (buffer_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
                vmaDestroyBuffer(allocator_, buffer_, allocation_);
                buffer_ = VK_NULL_HANDLE;
                allocation_ = VK_NULL_HANDLE;
                allocator_ = VK_NULL_HANDLE;
            }
        }

        void swap(VulkanBuffer &other) noexcept {
            std::swap(allocator_, other.allocator_);
            std::swap(buffer_, other.buffer_);
            std::swap(allocation_, other.allocation_);
            std::swap(mappedPtr_, other.mappedPtr_);
            std::swap(size_, other.size_);
            std::swap(usage_, other.usage_);
            std::swap(deviceAddr_, other.deviceAddr_);
        }

        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        void *mappedPtr_ = nullptr;
        uint64_t size_ = 0;
        BufferUsage usage_ = BufferUsage::Uniform;
        uint64_t deviceAddr_ = 0;
    };
}