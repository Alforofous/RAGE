#pragma once
#include <vulkan/vulkan.h>
#include "vulkan_utils.hpp"

class VulkanRenderTarget {
public:
    VulkanRenderTarget(const VulkanContext* context, uint32_t width, uint32_t height);
    ~VulkanRenderTarget();

    void resize(uint32_t width, uint32_t height);
    void dispose();

    VkImage getImage() const { return image; }
    VkImageView getImageView() const { return imageView; }
    VkFormat getFormat() const { return format; }
    VkExtent2D getExtent() const { return extent; }

    void transitionLayout(VkCommandBuffer cmdBuffer, 
                         VkImageLayout oldLayout,
                         VkImageLayout newLayout,
                         VkPipelineStageFlags srcStageMask,
                         VkPipelineStageFlags dstStageMask,
                         VkAccessFlags srcAccessMask,
                         VkAccessFlags dstAccessMask);

private:
    const VulkanContext* context;
    
    VkImage image;
    VkDeviceMemory memory;
    VkImageView imageView;
    VkFormat format;
    VkExtent2D extent;

    void createResources();
    void createImage();
    void createImageView();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};
