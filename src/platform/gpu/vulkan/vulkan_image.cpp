#include "vulkan_image.hpp"
#include "vulkan_type_map.hpp"
#include <stdexcept>

namespace {
    VkImageViewType autoDeduceViewType(uint32_t height, uint32_t depth, uint32_t arrayLayers) {
        if (depth > 1) {
            return VK_IMAGE_VIEW_TYPE_3D;
        }
        if (height == 1) {
            return arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        }
        if (arrayLayers > 1) {
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }

        return VK_IMAGE_VIEW_TYPE_2D;
    }
}

namespace RAGE {
    VulkanImage::VulkanImage(VkDevice device, VmaAllocator allocator, VkImage image, VmaAllocation allocation,
                             ImageCreateInfo info)
        : device_(device)
        , allocator_(allocator)
        , image_(image)
        , allocation_(allocation)
        , info_(info) {}

    VulkanImageView VulkanImage::createView(ImageViewCreateInfo viewInfo) {
        VkImageViewCreateInfo viewCI{};
        viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image = image_;
        viewCI.viewType = (viewInfo.viewType == ImageViewType::Auto)
                              ? autoDeduceViewType(info_.height, info_.depth, info_.arrayLayers)
                              : toVkImageViewType(viewInfo.viewType);
        viewCI.format = toVkFormat(info_.format);
        viewCI.subresourceRange.aspectMask = aspectFlagsForFormat(info_.format);
        viewCI.subresourceRange.baseMipLevel = viewInfo.baseMipLevel;
        viewCI.subresourceRange.levelCount = viewInfo.mipCount;
        viewCI.subresourceRange.baseArrayLayer = viewInfo.baseArrayLayer;
        viewCI.subresourceRange.layerCount = viewInfo.layerCount;

        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(device_, &viewCI, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }

        return VulkanImageView(device_, view, viewInfo);
    }
}