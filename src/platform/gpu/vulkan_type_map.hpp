#pragma once

#include <vulkan/vulkan.h>
#include "gpu_types.hpp"
#include "../../shared/logger.hpp"

namespace RAGE {
    inline VkBufferUsageFlags toVkBufferUsage(BufferUsage usage) {
        VkBufferUsageFlags flags = 0;
        if (hasFlag(usage, BufferUsage::Uniform)) {
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        if (hasFlag(usage, BufferUsage::Storage)) {
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
        if (hasFlag(usage, BufferUsage::Vertex)) {
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }
        if (hasFlag(usage, BufferUsage::Index)) {
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        if (hasFlag(usage, BufferUsage::DeviceAddress)) {
            flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        }
        if (hasFlag(usage, BufferUsage::TransferSrc)) {
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        if (hasFlag(usage, BufferUsage::TransferDst)) {
            flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        return flags;
    }

    inline VkImageUsageFlags toVkImageUsage(ImageUsage usage) {
        VkImageUsageFlags flags = 0;
        if (hasFlag(usage, ImageUsage::ColorAttachment)) {
            flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (hasFlag(usage, ImageUsage::DepthStencil)) {
            flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        if (hasFlag(usage, ImageUsage::Sampled)) {
            flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        if (hasFlag(usage, ImageUsage::Storage)) {
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;
        }
        if (hasFlag(usage, ImageUsage::TransferSrc)) {
            flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        if (hasFlag(usage, ImageUsage::TransferDst)) {
            flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }

        return flags;
    }

    inline VkFormat toVkFormat(ImageFormat format) {
        switch (format) {
            case ImageFormat::RGBA8_UNORM:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case ImageFormat::BGRA8_UNORM:
                return VK_FORMAT_B8G8R8A8_UNORM;
            case ImageFormat::RGBA8_SRGB:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case ImageFormat::BGRA8_SRGB:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case ImageFormat::R8_UNORM:
                return VK_FORMAT_R8_UNORM;
            case ImageFormat::RG8_UNORM:
                return VK_FORMAT_R8G8_UNORM;
            case ImageFormat::RGBA16_SFLOAT:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case ImageFormat::RGBA32_SFLOAT:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case ImageFormat::R32_SFLOAT:
                return VK_FORMAT_R32_SFLOAT;
            case ImageFormat::R32_UINT:
                return VK_FORMAT_R32_UINT;
            case ImageFormat::D32_SFLOAT:
                return VK_FORMAT_D32_SFLOAT;
            case ImageFormat::D24_UNORM_S8_UINT:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case ImageFormat::D32_SFLOAT_S8_UINT:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            default:
                log(LogLevel::Error, "Unhandled ImageFormat in toVkFormat");

                return VK_FORMAT_UNDEFINED;
        }
    }

    inline VkImageAspectFlags aspectFlagsForFormat(ImageFormat format) {
        switch (format) {
            case ImageFormat::D32_SFLOAT:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
            case ImageFormat::D24_UNORM_S8_UINT:
            case ImageFormat::D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }
}