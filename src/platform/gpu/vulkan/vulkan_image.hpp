#pragma once

#include <cstdint>
#include <tuple>
#include <utility>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "gpu_queue_kind.hpp"
#include "gpu_types.hpp"

namespace RAGE {
    class VulkanAllocator;
    class VulkanImage;
    template <QueueKind K> class VulkanRecorder;

    class VulkanImageView {
    public:
        VulkanImageView() = default;
        ~VulkanImageView() { destroy(); }

        bool isValid() const { return view_ != VK_NULL_HANDLE; }

        VulkanImageView(const VulkanImageView &) = delete;
        VulkanImageView &operator=(const VulkanImageView &) = delete;

        VulkanImageView(VulkanImageView &&other) noexcept { swap(other); }
        VulkanImageView &operator=(VulkanImageView &&other) noexcept {
            if (this != &other) {
                destroy();
                swap(other);
            }

            return *this;
        }

        const ImageViewCreateInfo &info() const { return info_; }

    private:
        friend class VulkanImage;
        friend class VulkanDescriptorWriter;

        VulkanImageView(VkDevice device, VkImageView view, ImageViewCreateInfo info)
            : device_(device)
            , view_(view)
            , info_(info) {}

        void destroy() {
            if (view_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
                vkDestroyImageView(device_, view_, nullptr);
                view_ = VK_NULL_HANDLE;
            }
        }

        void swap(VulkanImageView &other) noexcept {
            std::swap(device_, other.device_);
            std::swap(view_, other.view_);
            std::swap(info_, other.info_);
        }

        VkDevice device_ = VK_NULL_HANDLE;
        VkImageView view_ = VK_NULL_HANDLE;
        ImageViewCreateInfo info_{};
    };

    class VulkanImage {
    public:
        using ViewType = VulkanImageView;

        VulkanImage() = delete;
        ~VulkanImage() { destroy(); }

        VulkanImage(const VulkanImage &) = delete;
        VulkanImage &operator=(const VulkanImage &) = delete;

        VulkanImage(VulkanImage &&other) noexcept { swap(other); }
        VulkanImage &operator=(VulkanImage &&other) noexcept {
            if (this != &other) {
                destroy();
                swap(other);
            }

            return *this;
        }

        ImageFormat format() const { return info_.format; }
        VkImage handle() const { return image_; }
        std::tuple<uint32_t, uint32_t, uint32_t> extent() const { return { info_.width, info_.height, info_.depth }; }
        uint32_t mipLevels() const { return info_.mipLevels; }
        uint32_t arrayLayers() const { return info_.arrayLayers; }
        SampleCount sampleCount() const { return info_.sampleCount; }

        VulkanImageView createView(ImageViewCreateInfo viewInfo);

    private:
        friend class VulkanAllocator;
        template <QueueKind K> friend class VulkanRecorder;

        VulkanImage(VkDevice device, VmaAllocator allocator, VkImage image, VmaAllocation allocation,
                    ImageCreateInfo info);

        void destroy() {
            if (image_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
                vmaDestroyImage(allocator_, image_, allocation_);
                image_ = VK_NULL_HANDLE;
                allocation_ = VK_NULL_HANDLE;
            }
        }

        void swap(VulkanImage &other) noexcept {
            std::swap(device_, other.device_);
            std::swap(allocator_, other.allocator_);
            std::swap(image_, other.image_);
            std::swap(allocation_, other.allocation_);
            std::swap(info_, other.info_);
        }

        VkDevice device_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        ImageCreateInfo info_{};
    };
}