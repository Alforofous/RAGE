#pragma once

#include <vulkan/vulkan.h>
#include "gpu_queue_kind.hpp"
#include "gpu_types.hpp"
#include "shared/logger.hpp"

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
        if (hasFlag(usage, BufferUsage::TransferSrc)) {
            flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        }
        if (hasFlag(usage, BufferUsage::TransferDst)) {
            flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        if (hasFlag(usage, BufferUsage::ShaderDeviceAddress)) {
            flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
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
                Core::log(Core::LogLevel::Error, "Unhandled ImageFormat in toVkFormat");

                return VK_FORMAT_UNDEFINED;
        }
    }

    inline VkImageViewType toVkImageViewType(ImageViewType type) {
        switch (type) {
            case ImageViewType::Type1D:
                return VK_IMAGE_VIEW_TYPE_1D;
            case ImageViewType::Type2D:
                return VK_IMAGE_VIEW_TYPE_2D;
            case ImageViewType::Type3D:
                return VK_IMAGE_VIEW_TYPE_3D;
            case ImageViewType::Cube:
                return VK_IMAGE_VIEW_TYPE_CUBE;
            case ImageViewType::Type1DArray:
                return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            case ImageViewType::Type2DArray:
                return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            case ImageViewType::CubeArray:
                return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            case ImageViewType::Auto:
                break;
        }
        Core::log(Core::LogLevel::Error, "Unhandled ImageViewType in toVkImageViewType");

        return VK_IMAGE_VIEW_TYPE_2D;
    }

    inline VkPipelineStageFlags toVkPipelineStageFlags(PipelineStage stage) {
        VkPipelineStageFlags out = 0;
        if (hasFlag(stage, PipelineStage::TopOfPipe)) {
            out |= VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        if (hasFlag(stage, PipelineStage::DrawIndirect)) {
            out |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        }
        if (hasFlag(stage, PipelineStage::VertexInput)) {
            out |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        }
        if (hasFlag(stage, PipelineStage::VertexShader)) {
            out |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        }
        if (hasFlag(stage, PipelineStage::FragmentShader)) {
            out |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        if (hasFlag(stage, PipelineStage::EarlyFragmentTests)) {
            out |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        if (hasFlag(stage, PipelineStage::LateFragmentTests)) {
            out |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }
        if (hasFlag(stage, PipelineStage::ColorAttachmentOutput)) {
            out |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        if (hasFlag(stage, PipelineStage::ComputeShader)) {
            out |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
        if (hasFlag(stage, PipelineStage::Transfer)) {
            out |= VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        if (hasFlag(stage, PipelineStage::BottomOfPipe)) {
            out |= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
        if (hasFlag(stage, PipelineStage::AllGraphics)) {
            out |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        }
        if (hasFlag(stage, PipelineStage::AllCommands)) {
            out |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        return out;
    }

    inline VkQueueFlags toVkQueueFlags(uint32_t rageCaps) {
        VkQueueFlags out = 0;
        if ((rageCaps & queue_kind::caps::Graphics) != 0) {
            out |= VK_QUEUE_GRAPHICS_BIT;
        }
        if ((rageCaps & queue_kind::caps::Compute) != 0) {
            out |= VK_QUEUE_COMPUTE_BIT;
        }
        if ((rageCaps & queue_kind::caps::Transfer) != 0) {
            out |= VK_QUEUE_TRANSFER_BIT;
        }

        return out;
    }

    inline VkImageLayout toVkImageLayout(ImageLayout layout) {
        switch (layout) {
            case ImageLayout::Undefined:
                return VK_IMAGE_LAYOUT_UNDEFINED;
            case ImageLayout::General:
                return VK_IMAGE_LAYOUT_GENERAL;
            case ImageLayout::ColorAttachmentOptimal:
                return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case ImageLayout::DepthStencilAttachmentOptimal:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            case ImageLayout::DepthStencilReadOnlyOptimal:
                return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            case ImageLayout::ShaderReadOnlyOptimal:
                return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            case ImageLayout::TransferSrcOptimal:
                return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case ImageLayout::TransferDstOptimal:
                return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case ImageLayout::PresentSrc:
                return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }
        Core::log(Core::LogLevel::Error, "Unhandled ImageLayout in toVkImageLayout");

        return VK_IMAGE_LAYOUT_UNDEFINED;
    }

    inline VkAccessFlags toVkAccessFlags(AccessFlags flags) {
        VkAccessFlags out = 0;
        if (hasFlag(flags, AccessFlags::ShaderRead)) {
            out |= VK_ACCESS_SHADER_READ_BIT;
        }
        if (hasFlag(flags, AccessFlags::ShaderWrite)) {
            out |= VK_ACCESS_SHADER_WRITE_BIT;
        }
        if (hasFlag(flags, AccessFlags::ColorAttachmentRead)) {
            out |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        }
        if (hasFlag(flags, AccessFlags::ColorAttachmentWrite)) {
            out |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        }
        if (hasFlag(flags, AccessFlags::DepthStencilAttachmentRead)) {
            out |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
        if (hasFlag(flags, AccessFlags::DepthStencilAttachmentWrite)) {
            out |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        if (hasFlag(flags, AccessFlags::TransferRead)) {
            out |= VK_ACCESS_TRANSFER_READ_BIT;
        }
        if (hasFlag(flags, AccessFlags::TransferWrite)) {
            out |= VK_ACCESS_TRANSFER_WRITE_BIT;
        }
        if (hasFlag(flags, AccessFlags::HostRead)) {
            out |= VK_ACCESS_HOST_READ_BIT;
        }
        if (hasFlag(flags, AccessFlags::HostWrite)) {
            out |= VK_ACCESS_HOST_WRITE_BIT;
        }
        if (hasFlag(flags, AccessFlags::MemoryRead)) {
            out |= VK_ACCESS_MEMORY_READ_BIT;
        }
        if (hasFlag(flags, AccessFlags::MemoryWrite)) {
            out |= VK_ACCESS_MEMORY_WRITE_BIT;
        }

        return out;
    }

    inline VkPipelineBindPoint toVkPipelineBindPoint(PipelineBindPoint bp) {
        switch (bp) {
            case PipelineBindPoint::Graphics:
                return VK_PIPELINE_BIND_POINT_GRAPHICS;
            case PipelineBindPoint::Compute:
                return VK_PIPELINE_BIND_POINT_COMPUTE;
            case PipelineBindPoint::RayTracing:
                return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        }
        Core::log(Core::LogLevel::Error, "Unhandled PipelineBindPoint in toVkPipelineBindPoint");

        return VK_PIPELINE_BIND_POINT_COMPUTE;
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