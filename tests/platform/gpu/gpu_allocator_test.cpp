#include <gtest/gtest.h>
#include "mock_gpu.hpp"

using namespace RAGE;

// --- Concept static assertions ---

TEST(GpuConcepts, MockTypesSatisfyConcepts) {
    SUCCEED() << "static_assert in mock_gpu.hpp verifies concept satisfaction at compile time";
}

// --- MockBuffer tests ---

TEST(MockBuffer, CreatesWithCorrectSize) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 512, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu});
    EXPECT_EQ(buf.size(), 512);
}

TEST(MockBuffer, ReportsUsageFlags) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 64, .usage = BufferUsage::Storage | BufferUsage::TransferDst});
    EXPECT_TRUE(hasFlag(buf.usage(), BufferUsage::Storage));
    EXPECT_TRUE(hasFlag(buf.usage(), BufferUsage::TransferDst));
    EXPECT_FALSE(hasFlag(buf.usage(), BufferUsage::Vertex));
}

TEST(MockBuffer, MappedDataReturnsNullForGpuOnly) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 256, .usage = BufferUsage::Storage, .memory = MemoryLocation::GpuOnly});
    EXPECT_EQ(buf.mappedData(), nullptr);
}

TEST(MockBuffer, MappedDataReturnsPointerForCpuToGpu) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 256, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu});
    EXPECT_NE(buf.mappedData(), nullptr);
}

TEST(MockBuffer, MappedDataReturnsPointerForGpuToCpu) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 128, .usage = BufferUsage::Storage, .memory = MemoryLocation::GpuToCpu});
    EXPECT_NE(buf.mappedData(), nullptr);
}

TEST(MockBuffer, CanWriteToMappedData) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = sizeof(float) * 4, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu});

    float data[] = {1.0f, 2.0f, 3.0f, 4.0f};
    std::memcpy(buf.mappedData(), data, sizeof(data));

    auto* readBack = static_cast<float*>(buf.mappedData());
    EXPECT_FLOAT_EQ(readBack[0], 1.0f);
    EXPECT_FLOAT_EQ(readBack[3], 4.0f);
}

TEST(MockBuffer, DeviceAddressReturnsNonZeroWhenFlagSet) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 64, .usage = BufferUsage::Storage | BufferUsage::DeviceAddress});
    EXPECT_NE(buf.deviceAddress(), 0u);
}

TEST(MockBuffer, DeviceAddressReturnsZeroWithoutFlag) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 64, .usage = BufferUsage::Storage});
    EXPECT_EQ(buf.deviceAddress(), 0u);
}

TEST(MockBuffer, IsMoveOnly) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 128, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu});

    auto buf2 = std::move(buf);
    EXPECT_EQ(buf2.size(), 128);
    EXPECT_NE(buf2.mappedData(), nullptr);
}

// --- MockImage tests ---

TEST(MockImage, CreatesWithCorrectProperties) {
    MockAllocator alloc;
    auto img = alloc.createImage({
        .width = 800, .height = 600, .depth = 1,
        .format = ImageFormat::RGBA8_UNORM,
        .usage = ImageUsage::Storage,
        .mipLevels = 4, .arrayLayers = 2, .sampleCount = 1
    });

    EXPECT_EQ(img.format(), ImageFormat::RGBA8_UNORM);
    EXPECT_EQ(img.extent().width, 800);
    EXPECT_EQ(img.extent().height, 600);
    EXPECT_EQ(img.extent().depth, 1);
    EXPECT_EQ(img.mipLevels(), 4);
    EXPECT_EQ(img.arrayLayers(), 2);
    EXPECT_EQ(img.sampleCount(), 1);
}

TEST(MockImage, HasDefaultView) {
    MockAllocator alloc;
    auto img = alloc.createImage({.width = 64, .height = 64, .format = ImageFormat::RGBA8_UNORM});
    // Default view exists — just verify it's accessible without crashing
    [[maybe_unused]] const auto& v = img.view();
    SUCCEED();
}

TEST(MockImage, CreateViewReturnsStandaloneHandle) {
    MockAllocator alloc;
    auto img = alloc.createImage({.width = 256, .height = 256, .format = ImageFormat::RGBA8_UNORM, .mipLevels = 5});

    auto mipView = img.createView({.baseMipLevel = 3, .mipCount = 1});
    EXPECT_EQ(mipView.info().baseMipLevel, 3);
    EXPECT_EQ(mipView.info().mipCount, 1);
}

TEST(MockImage, IsMoveOnly) {
    MockAllocator alloc;
    auto img = alloc.createImage({.width = 128, .height = 128, .format = ImageFormat::D32_SFLOAT});

    auto img2 = std::move(img);
    EXPECT_EQ(img2.format(), ImageFormat::D32_SFLOAT);
    EXPECT_EQ(img2.extent().width, 128);
}

// --- Generic function using concept ---

template<GpuAllocator Alloc>
typename Alloc::Buffer createUniformBuffer(Alloc& alloc, uint64_t size) {
    return alloc.createBuffer({.size = size, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu});
}

TEST(GpuAllocatorConcept, GenericFunctionWorksWithMock) {
    MockAllocator alloc;
    auto buf = createUniformBuffer(alloc, 1024);
    EXPECT_EQ(buf.size(), 1024);
    EXPECT_NE(buf.mappedData(), nullptr);
}

// --- Zero-size buffer ---

TEST(MockBuffer, ZeroSizeBuffer) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 0, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu});
    EXPECT_EQ(buf.size(), 0);
}

// --- Moved-from state safety ---

TEST(MockBuffer, MovedFromBufferIsSafe) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({.size = 256, .usage = BufferUsage::Storage, .memory = MemoryLocation::CpuToGpu});
    auto buf2 = std::move(buf);

    // Moved-from buffer should be in a valid empty state
    EXPECT_EQ(buf.size(), 0);
    EXPECT_EQ(buf.mappedData(), nullptr);
}

TEST(MockImage, MovedFromImageIsSafe) {
    MockAllocator alloc;
    auto img = alloc.createImage({.width = 64, .height = 64, .format = ImageFormat::RGBA8_UNORM});
    auto img2 = std::move(img);

    // Moved-to object has correct values
    EXPECT_EQ(img2.extent().width, 64);
    EXPECT_EQ(img2.format(), ImageFormat::RGBA8_UNORM);
    // Moved-from object destructor runs safely (no double-free, no crash)
}

// --- Type map pure function tests ---

#include "gpu/vulkan_type_map.hpp"

TEST(VulkanTypeMap, BufferUsageMapping) {
    EXPECT_EQ(toVkBufferUsage(BufferUsage::Uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::Storage), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::Index), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::DeviceAddress), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::TransferSrc), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::TransferDst), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

TEST(VulkanTypeMap, BufferUsageCombined) {
    auto flags = toVkBufferUsage(BufferUsage::Storage | BufferUsage::DeviceAddress);
    EXPECT_TRUE(flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    EXPECT_TRUE(flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    EXPECT_FALSE(flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
}

TEST(VulkanTypeMap, ImageUsageMapping) {
    EXPECT_EQ(toVkImageUsage(ImageUsage::ColorAttachment), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    EXPECT_EQ(toVkImageUsage(ImageUsage::DepthStencil), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    EXPECT_EQ(toVkImageUsage(ImageUsage::Sampled), VK_IMAGE_USAGE_SAMPLED_BIT);
    EXPECT_EQ(toVkImageUsage(ImageUsage::Storage), VK_IMAGE_USAGE_STORAGE_BIT);
    EXPECT_EQ(toVkImageUsage(ImageUsage::TransferSrc), VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    EXPECT_EQ(toVkImageUsage(ImageUsage::TransferDst), VK_IMAGE_USAGE_TRANSFER_DST_BIT);
}

TEST(VulkanTypeMap, FormatMapping) {
    EXPECT_EQ(toVkFormat(ImageFormat::RGBA8_UNORM), VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(toVkFormat(ImageFormat::D32_SFLOAT), VK_FORMAT_D32_SFLOAT);
    EXPECT_EQ(toVkFormat(ImageFormat::BGRA8_SRGB), VK_FORMAT_B8G8R8A8_SRGB);
    EXPECT_EQ(toVkFormat(ImageFormat::R32_UINT), VK_FORMAT_R32_UINT);
}

TEST(VulkanTypeMap, AspectFlagsForFormat) {
    EXPECT_EQ(aspectFlagsForFormat(ImageFormat::RGBA8_UNORM), VK_IMAGE_ASPECT_COLOR_BIT);
    EXPECT_EQ(aspectFlagsForFormat(ImageFormat::D32_SFLOAT), VK_IMAGE_ASPECT_DEPTH_BIT);
    EXPECT_EQ(aspectFlagsForFormat(ImageFormat::D24_UNORM_S8_UINT),
              VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}
