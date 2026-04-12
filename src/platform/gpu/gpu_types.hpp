#pragma once

#include <cstdint>

namespace RAGE {
#define RAGE_DEFINE_FLAGS(EnumType) \
        inline EnumType operator|(EnumType a, EnumType b) { \
            return static_cast<EnumType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); \
        } \
        inline EnumType operator&(EnumType a, EnumType b) { \
            return static_cast<EnumType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); \
        } \
        inline bool hasFlag(EnumType flags, EnumType flag) { \
            return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0; \
        }

    enum class BufferUsage : uint32_t {
        Uniform       = 1 << 0,
        Storage       = 1 << 1,
        Vertex        = 1 << 2,
        Index         = 1 << 3,
        DeviceAddress = 1 << 4,
        TransferSrc   = 1 << 5,
        TransferDst   = 1 << 6,
    };
    RAGE_DEFINE_FLAGS(BufferUsage)

    enum class ImageUsage : uint32_t {
        ColorAttachment = 1 << 0,
        DepthStencil    = 1 << 1,
        Sampled         = 1 << 2,
        Storage         = 1 << 3,
        TransferSrc     = 1 << 4,
        TransferDst     = 1 << 5,
    };
    RAGE_DEFINE_FLAGS(ImageUsage)

    enum class ImageFormat : uint8_t {
        RGBA8_UNORM,
        BGRA8_UNORM,
        RGBA8_SRGB,
        BGRA8_SRGB,
        R8_UNORM,
        RG8_UNORM,
        RGBA16_SFLOAT,
        RGBA32_SFLOAT,
        R32_SFLOAT,
        R32_UINT,
        D32_SFLOAT,
        D24_UNORM_S8_UINT,
        D32_SFLOAT_S8_UINT,
    };

    enum class MemoryLocation : uint8_t {
        GpuOnly,
        CpuToGpu,
        GpuToCpu,
    };

    struct Extent3D {
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
    };

    struct BufferCreateInfo {
        uint64_t size = 0;
        BufferUsage usage = BufferUsage::Uniform;
        MemoryLocation memory = MemoryLocation::GpuOnly;
    };

    struct ImageCreateInfo {
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        ImageFormat format = ImageFormat::RGBA8_UNORM;
        ImageUsage usage = ImageUsage::Storage;
        MemoryLocation memory = MemoryLocation::GpuOnly;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        uint32_t sampleCount = 1;
    };

    struct ImageViewCreateInfo {
        uint32_t baseMipLevel = 0;
        uint32_t mipCount = 1;
        uint32_t baseArrayLayer = 0;
        uint32_t layerCount = 1;
    };
}