#include "vulkan_render_target.hpp"
#include "vulkan/vulkan_core.h"
#include <cstdint>
#include <stdexcept>

VulkanRenderTarget::VulkanRenderTarget(
    const VulkanContext* context,
    uint32_t width,
    uint32_t height
)
    : context(context),
    image(VK_NULL_HANDLE),
    memory(VK_NULL_HANDLE),
    imageView(VK_NULL_HANDLE),
    format(VK_FORMAT_R8G8B8A8_UNORM),
    extent{width, height}
{
    this->createResources();
}

VulkanRenderTarget::~VulkanRenderTarget() {
    this->dispose();
}

void VulkanRenderTarget::resize(uint32_t width, uint32_t height) {
    if (width == this->extent.width && height == this->extent.height) {
        return;
    }
    this->dispose();
    this->extent.width = width;
    this->extent.height = height;
    this->createResources();
}

void VulkanRenderTarget::createResources() {
    this->createImage();
    this->createImageView();
}

void VulkanRenderTarget::createImage() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = this->format;
    imageInfo.extent.width = this->extent.width;
    imageInfo.extent.height = this->extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    if (vkCreateImage(this->context->device, &imageInfo, nullptr, &this->image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render target image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(this->context->device, this->image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = this->findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    if (vkAllocateMemory(this->context->device, &allocInfo, nullptr, &this->memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate render target memory");
    }

    vkBindImageMemory(this->context->device, this->image, this->memory, 0);
}

void VulkanRenderTarget::createImageView() {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = this->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = this->format;
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;
    viewInfo.subresourceRange = {
        aspectMask,
        baseMipLevel,
        levelCount,
        baseArrayLayer,
        layerCount
    };

    if (vkCreateImageView(this->context->device, &viewInfo, nullptr, &this->imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render target image view");
    }
}

void VulkanRenderTarget::dispose() {
    if (this->imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(this->context->device, this->imageView, nullptr);
        this->imageView = VK_NULL_HANDLE;
    }
    if (this->image != VK_NULL_HANDLE) {
        vkDestroyImage(this->context->device, this->image, nullptr);
        this->image = VK_NULL_HANDLE;
    }
    if (this->memory != VK_NULL_HANDLE) {
        vkFreeMemory(this->context->device, this->memory, nullptr);
        this->memory = VK_NULL_HANDLE;
    }
}

void VulkanRenderTarget::transitionLayout(
    VkCommandBuffer cmdBuffer,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask
) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = this->image;
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    uint32_t baseMipLevel = 0;
    uint32_t levelCount = 1;
    uint32_t baseArrayLayer = 0;
    uint32_t layerCount = 1;
    barrier.subresourceRange = {
        aspectMask,
        baseMipLevel,
        levelCount,
        baseArrayLayer,
        layerCount
    };
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        cmdBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

uint32_t VulkanRenderTarget::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(this->context->physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (((typeFilter & (1 << i)) != 0) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type");
}