#pragma once

#include <cstdint>
#include <type_traits>

namespace RAGE {
    template <typename E> inline constexpr bool is_bitmask_enum = false;

    template <typename E>
        requires is_bitmask_enum<E>
    constexpr E operator|(E a, E b) {
        using U = std::underlying_type_t<E>;
        return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));
    }

    template <typename E>
        requires is_bitmask_enum<E>
    constexpr E operator&(E a, E b) {
        using U = std::underlying_type_t<E>;
        return static_cast<E>(static_cast<U>(a) & static_cast<U>(b));
    }

    template <typename E>
        requires is_bitmask_enum<E>
    constexpr E operator~(E a) {
        using U = std::underlying_type_t<E>;
        return static_cast<E>(~static_cast<U>(a));
    }

    template <typename E>
        requires is_bitmask_enum<E>
    constexpr E &operator|=(E &a, E b) {
        a = a | b;

        return a;
    }

    template <typename E>
        requires is_bitmask_enum<E>
    constexpr E &operator&=(E &a, E b) {
        a = a & b;

        return a;
    }

    template <typename E>
        requires is_bitmask_enum<E>
    constexpr bool hasFlag(E flags, E flag) {
        using U = std::underlying_type_t<E>;
        return (static_cast<U>(flags) & static_cast<U>(flag)) == static_cast<U>(flag);
    }

    enum class BufferUsage : uint32_t {
        Uniform = 1 << 0,
        Storage = 1 << 1,
        Vertex = 1 << 2,
        Index = 1 << 3,
        TransferSrc = 1 << 4,
        TransferDst = 1 << 5,
        ShaderDeviceAddress = 1 << 6,
    };
    template <> inline constexpr bool is_bitmask_enum<BufferUsage> = true;

    enum class ImageUsage : uint32_t {
        ColorAttachment = 1 << 0,
        DepthStencil = 1 << 1,
        Sampled = 1 << 2,
        Storage = 1 << 3,
        TransferSrc = 1 << 4,
        TransferDst = 1 << 5,
    };
    template <> inline constexpr bool is_bitmask_enum<ImageUsage> = true;

    enum class PipelineStage : uint32_t {
        None = 0,
        TopOfPipe = 1u << 0,
        DrawIndirect = 1u << 1,
        VertexInput = 1u << 2,
        VertexShader = 1u << 3,
        FragmentShader = 1u << 4,
        EarlyFragmentTests = 1u << 5,
        LateFragmentTests = 1u << 6,
        ColorAttachmentOutput = 1u << 7,
        ComputeShader = 1u << 8,
        Transfer = 1u << 9,
        BottomOfPipe = 1u << 10,
        AllGraphics = 1u << 11,
        AllCommands = 1u << 12,
    };
    template <> inline constexpr bool is_bitmask_enum<PipelineStage> = true;

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

    enum class SampleCount : uint8_t {
        Count1 = 1,
        Count2 = 2,
        Count4 = 4,
        Count8 = 8,
        Count16 = 16,
        Count32 = 32,
        Count64 = 64,
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
        SampleCount sampleCount = SampleCount::Count1;
    };

    enum class ImageViewType : uint8_t {
        Auto,
        Type1D,
        Type2D,
        Type3D,
        Cube,
        Type1DArray,
        Type2DArray,
        CubeArray,
    };

    struct ImageViewCreateInfo {
        ImageViewType viewType = ImageViewType::Auto;
        uint32_t baseMipLevel = 0;
        uint32_t mipCount = 1;
        uint32_t baseArrayLayer = 0;
        uint32_t layerCount = 1;
    };

    enum class ImageLayout : uint8_t {
        Undefined,
        General,
        ColorAttachmentOptimal,
        DepthStencilAttachmentOptimal,
        DepthStencilReadOnlyOptimal,
        ShaderReadOnlyOptimal,
        TransferSrcOptimal,
        TransferDstOptimal,
        PresentSrc,
    };

    enum class AccessFlags : uint32_t {
        None = 0,
        ShaderRead = 1u << 0,
        ShaderWrite = 1u << 1,
        ColorAttachmentRead = 1u << 2,
        ColorAttachmentWrite = 1u << 3,
        DepthStencilAttachmentRead = 1u << 4,
        DepthStencilAttachmentWrite = 1u << 5,
        TransferRead = 1u << 6,
        TransferWrite = 1u << 7,
        HostRead = 1u << 8,
        HostWrite = 1u << 9,
        MemoryRead = 1u << 10,
        MemoryWrite = 1u << 11,
    };
    template <> inline constexpr bool is_bitmask_enum<AccessFlags> = true;

    enum class PipelineBindPoint : uint8_t {
        Graphics,
        Compute,
        RayTracing,
    };

    enum class ShaderStage : uint8_t {
        Vertex,
        Fragment,
        Compute,
        RayGeneration,
        Miss,
        ClosestHit,
        AnyHit,
        Intersection,
        Callable,
    };

    struct BufferCopy {
        uint64_t srcOffset = 0;
        uint64_t dstOffset = 0;
        uint64_t size = 0;
    };

    struct BufferImageCopy {
        uint64_t bufferOffset = 0;
        uint32_t bufferRowLength = 0;
        uint32_t bufferImageHeight = 0;
        uint32_t mipLevel = 0;
        uint32_t baseArrayLayer = 0;
        uint32_t layerCount = 1;
        int32_t imageOffsetX = 0;
        int32_t imageOffsetY = 0;
        int32_t imageOffsetZ = 0;
        uint32_t imageWidth = 0;
        uint32_t imageHeight = 0;
        uint32_t imageDepth = 1;
    };
}