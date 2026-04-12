#include "vulkan_image.hpp"
#include "vulkan_type_map.hpp"
#include <stdexcept>

namespace RAGE {
    namespace {
        VkImageViewType deduceViewType(uint32_t depth, uint32_t arrayLayers) {
            if (depth > 1) {
                return VK_IMAGE_VIEW_TYPE_3D;
            }
            if (arrayLayers > 1) {
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            }

            return VK_IMAGE_VIEW_TYPE_2D;
        }
    }

    VulkanImage::VulkanImage(VkDevice device, VmaAllocator allocator, VkImage image,
                             VmaAllocation allocation, ImageCreateInfo info)
        : device_(device)
        , allocator_(allocator)
        , image_(image)
        , allocation_(allocation)
        , format_(info.format)
        , extent_{.width = info.width, .height = info.height, .depth = info.depth}
        , mipLevels_(info.mipLevels)
        , arrayLayers_(info.arrayLayers)
        , sampleCount_(info.sampleCount) {
        const ImageViewCreateInfo defaultViewInfo{
            .baseMipLevel = 0,
            .mipCount = info.mipLevels,
            .baseArrayLayer = 0,
            .layerCount = info.arrayLayers,
        };
        defaultView_ = createView(defaultViewInfo);
    }

    VulkanImageView VulkanImage::createView(ImageViewCreateInfo viewInfo) {
        VkImageViewCreateInfo viewCI{};
        viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image = image_;
        viewCI.viewType = deduceViewType(extent_.depth, arrayLayers_);
        viewCI.format = toVkFormat(format_);
        viewCI.subresourceRange.aspectMask = aspectFlagsForFormat(format_);
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