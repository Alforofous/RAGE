#include <gtest/gtest.h>
#include "mocks/allocator.hpp"
#include "mocks/context.hpp"

using namespace RAGE;
using namespace RAGE::Mocks;

TEST(GpuConcepts, MockTypesSatisfyConcepts) {
    SUCCEED() << "static_assert in mocks/ verifies concept satisfaction at compile time";
}

TEST(MockBuffer, CreatesWithCorrectSize) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 512, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu });
    EXPECT_EQ(buf.size(), 512);
}

TEST(MockBuffer, ReportsUsageFlags) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 64, .usage = BufferUsage::Storage | BufferUsage::TransferDst });
    EXPECT_TRUE(hasFlag(buf.usage(), BufferUsage::Storage));
    EXPECT_TRUE(hasFlag(buf.usage(), BufferUsage::TransferDst));
    EXPECT_FALSE(hasFlag(buf.usage(), BufferUsage::Vertex));
}

TEST(MockBuffer, MappedDataReturnsNullForGpuOnly) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 256, .usage = BufferUsage::Storage, .memory = MemoryLocation::GpuOnly });
    EXPECT_EQ(buf.mappedData(), nullptr);
}

TEST(MockBuffer, MappedDataReturnsPointerForCpuToGpu) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 256, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu });
    EXPECT_NE(buf.mappedData(), nullptr);
}

TEST(MockBuffer, MappedDataReturnsPointerForGpuToCpu) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 128, .usage = BufferUsage::Storage, .memory = MemoryLocation::GpuToCpu });
    EXPECT_NE(buf.mappedData(), nullptr);
}

TEST(MockBuffer, CanWriteToMappedData) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer(
        { .size = sizeof(float) * 4, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu });

    float data[] = { 1.0f, 2.0f, 3.0f, 4.0f };
    std::memcpy(buf.mappedData(), data, sizeof(data));

    auto *readBack = static_cast<float *>(buf.mappedData());
    EXPECT_FLOAT_EQ(readBack[0], 1.0f);
    EXPECT_FLOAT_EQ(readBack[3], 4.0f);
}

TEST(MockBuffer, DeviceAddressNonZeroWhenShaderDeviceAddressUsageSet) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 64, .usage = BufferUsage::Storage | BufferUsage::ShaderDeviceAddress });
    EXPECT_NE(buf.deviceAddress(), 0u);
}

TEST(MockBuffer, DeviceAddressZeroWhenUsageMissing) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 64, .usage = BufferUsage::Storage });
    EXPECT_EQ(buf.deviceAddress(), 0u);
}

TEST(MockBuffer, IsMoveOnly) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 128, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu });

    auto buf2 = std::move(buf);
    EXPECT_EQ(buf2.size(), 128);
    EXPECT_NE(buf2.mappedData(), nullptr);
}

TEST(MockImage, CreatesWithCorrectProperties) {
    MockAllocator alloc;
    auto img = alloc.createImage({ .width = 800,
                                   .height = 600,
                                   .depth = 1,
                                   .format = ImageFormat::RGBA8_UNORM,
                                   .usage = ImageUsage::Storage,
                                   .mipLevels = 4,
                                   .arrayLayers = 2,
                                   .sampleCount = SampleCount::Count1 });

    EXPECT_EQ(img.format(), ImageFormat::RGBA8_UNORM);
    auto [w, h, d] = img.extent();
    EXPECT_EQ(w, 800u);
    EXPECT_EQ(h, 600u);
    EXPECT_EQ(d, 1u);
    EXPECT_EQ(img.mipLevels(), 4);
    EXPECT_EQ(img.arrayLayers(), 2);
    EXPECT_EQ(img.sampleCount(), SampleCount::Count1);
}

TEST(MockImage, CreateViewReturnsStandaloneHandle) {
    MockAllocator alloc;
    auto img = alloc.createImage({ .width = 256, .height = 256, .format = ImageFormat::RGBA8_UNORM, .mipLevels = 5 });

    auto mipView = img.createView({ .baseMipLevel = 3, .mipCount = 1 });
    EXPECT_EQ(mipView.info().baseMipLevel, 3);
    EXPECT_EQ(mipView.info().mipCount, 1);
}

TEST(MockImage, IsMoveOnly) {
    MockAllocator alloc;
    auto img = alloc.createImage({ .width = 128, .height = 128, .format = ImageFormat::D32_SFLOAT });

    auto img2 = std::move(img);
    EXPECT_EQ(img2.format(), ImageFormat::D32_SFLOAT);
    EXPECT_EQ(std::get<0>(img2.extent()), 128u);
}

template <GpuAllocator Alloc> typename Alloc::Buffer createUniformBuffer(Alloc &alloc, uint64_t size) {
    return alloc.createBuffer({ .size = size, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu });
}

TEST(GpuAllocatorConcept, GenericFunctionWorksWithMock) {
    MockAllocator alloc;
    auto buf = createUniformBuffer(alloc, 1024);
    EXPECT_EQ(buf.size(), 1024);
    EXPECT_NE(buf.mappedData(), nullptr);
}

TEST(MockBuffer, ZeroSizeBuffer) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 0, .usage = BufferUsage::Uniform, .memory = MemoryLocation::CpuToGpu });
    EXPECT_EQ(buf.size(), 0);
}

TEST(MockBuffer, MovedFromBufferIsSafe) {
    MockAllocator alloc;
    auto buf = alloc.createBuffer({ .size = 256, .usage = BufferUsage::Storage, .memory = MemoryLocation::CpuToGpu });
    auto buf2 = std::move(buf);

    EXPECT_EQ(buf2.size(), 256);
    EXPECT_NE(buf2.mappedData(), nullptr);
}

TEST(MockImage, MovedFromImageIsSafe) {
    MockAllocator alloc;
    auto img = alloc.createImage({ .width = 64, .height = 64, .format = ImageFormat::RGBA8_UNORM });
    auto img2 = std::move(img);

    EXPECT_EQ(std::get<0>(img2.extent()), 64u);
    EXPECT_EQ(img2.format(), ImageFormat::RGBA8_UNORM);
}

#include "gpu/vulkan/vulkan_type_map.hpp"

TEST(VulkanTypeMap, BufferUsageMapping) {
    EXPECT_EQ(toVkBufferUsage(BufferUsage::Uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::Storage), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::Index), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::TransferSrc), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    EXPECT_EQ(toVkBufferUsage(BufferUsage::TransferDst), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
}

TEST(VulkanTypeMap, BufferUsageCombined) {
    auto flags = toVkBufferUsage(BufferUsage::Storage | BufferUsage::TransferDst);
    EXPECT_TRUE(flags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    EXPECT_TRUE(flags & VK_BUFFER_USAGE_TRANSFER_DST_BIT);
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

TEST(GpuContextConcept, MockSatisfiesConcept) {
    SUCCEED() << "static_assert in mocks/ verifies concept satisfaction at compile time";
}

TEST(MockContext, ReturnsHandles) {
    const MockContext ctx;
    [[maybe_unused]] const auto inst = ctx.instance();
    [[maybe_unused]] const auto phys = ctx.physicalDevice();
    [[maybe_unused]] const auto dev = ctx.device();
    SUCCEED();
}

TEST(MockContext, IsMoveOnly) {
    MockContext ctx;
    auto ctx2 = std::move(ctx);
    [[maybe_unused]] const auto dev = ctx2.device();
    SUCCEED();
}

template <GpuContext Ctx> typename Ctx::Device extractDevice(Ctx &ctx) {
    return ctx.device();
}

TEST(GpuContextConcept, GenericFunctionWorksWithMock) {
    MockContext ctx;
    [[maybe_unused]] const auto dev = extractDevice(ctx);
    SUCCEED();
}