#pragma once

#include <cstdint>
#include <utility>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "gpu_types.hpp"

namespace RAGE {
    class VulkanImageView {
public:
        VulkanImageView() = default;

        VulkanImageView(VkDevice device, VkImageView view, ImageViewCreateInfo info)
            : device_(device), view_(view), info_(info) {}

        ~VulkanImageView() { destroy(); }

        VulkanImageView(const VulkanImageView &) = delete;
        VulkanImageView& operator=(const VulkanImageView &) = delete;

        VulkanImageView(VulkanImageView &&other) noexcept { swap(other); }
        VulkanImageView& operator=(VulkanImageView &&other) noexcept {
            if (this != &other) {
                destroy();
                swap(other);
            }

            return *this;
        }

        VkImageView vulkanHandle() const { return view_; }
        const ImageViewCreateInfo& info() const { return info_; }

private:
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

        VulkanImage() = default;

        VulkanImage(VkDevice device, VmaAllocator allocator, VkImage image, VmaAllocation allocation,
                    ImageCreateInfo info);

        ~VulkanImage() { destroy(); }

        VulkanImage(const VulkanImage &) = delete;
        VulkanImage& operator=(const VulkanImage &) = delete;

        VulkanImage(VulkanImage &&other) noexcept { swap(other); }
        VulkanImage& operator=(VulkanImage &&other) noexcept {
            if (this != &other) {
                destroy();
                swap(other);
            }

            return *this;
        }

        ImageFormat format() const { return format_; }
        Extent3D extent() const { return extent_; }
        uint32_t mipLevels() const { return mipLevels_; }
        uint32_t arrayLayers() const { return arrayLayers_; }
        uint32_t sampleCount() const { return sampleCount_; }

        const VulkanImageView& view() const { return defaultView_; }

        VulkanImageView createView(ImageViewCreateInfo viewInfo);

        VkImage vulkanHandle() const { return image_; }

private:
        void destroy() {
            if (image_ != VK_NULL_HANDLE && allocator_ != VK_NULL_HANDLE) {
                defaultView_ = VulkanImageView{};
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
            std::swap(defaultView_, other.defaultView_);
            std::swap(format_, other.format_);
            std::swap(extent_, other.extent_);
            std::swap(mipLevels_, other.mipLevels_);
            std::swap(arrayLayers_, other.arrayLayers_);
            std::swap(sampleCount_, other.sampleCount_);
        }

        VkDevice device_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VulkanImageView defaultView_;
        ImageFormat format_ = ImageFormat::RGBA8_UNORM;
        Extent3D extent_{};
        uint32_t mipLevels_ = 1;
        uint32_t arrayLayers_ = 1;
        uint32_t sampleCount_ = 1;
    };
}