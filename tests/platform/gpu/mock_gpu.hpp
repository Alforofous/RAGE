#pragma once

#include <vector>
#include <cstring>
#include "gpu/gpu_types.hpp"
#include "gpu/gpu_concepts.hpp"
#include "logger.hpp"

namespace RAGE {

class MockImageView {
public:
    MockImageView() = default;
    MockImageView(ImageViewCreateInfo info) : info_(info) {}

    MockImageView(const MockImageView&) = delete;
    MockImageView& operator=(const MockImageView&) = delete;
    MockImageView(MockImageView&&) = default;
    MockImageView& operator=(MockImageView&&) = default;

    const ImageViewCreateInfo& info() const { return info_; }

private:
    ImageViewCreateInfo info_{};
};

class MockImage {
public:
    using ViewType = MockImageView;

    MockImage() = default;
    MockImage(ImageCreateInfo info)
        : format_(info.format)
        , extent_{info.width, info.height, info.depth}
        , mipLevels_(info.mipLevels)
        , arrayLayers_(info.arrayLayers)
        , sampleCount_(info.sampleCount) {}

    MockImage(const MockImage&) = delete;
    MockImage& operator=(const MockImage&) = delete;
    MockImage(MockImage&&) = default;
    MockImage& operator=(MockImage&&) = default;

    ImageFormat format() const { return format_; }
    Extent3D extent() const { return extent_; }
    uint32_t mipLevels() const { return mipLevels_; }
    uint32_t arrayLayers() const { return arrayLayers_; }
    uint32_t sampleCount() const { return sampleCount_; }

    const MockImageView& view() const { return defaultView_; }

    MockImageView createView(ImageViewCreateInfo info) {
        return MockImageView(info);
    }

private:
    ImageFormat format_ = ImageFormat::RGBA8_UNORM;
    Extent3D extent_{};
    uint32_t mipLevels_ = 1;
    uint32_t arrayLayers_ = 1;
    uint32_t sampleCount_ = 1;
    MockImageView defaultView_{};
};

class MockBuffer {
public:
    MockBuffer() = default;
    MockBuffer(BufferCreateInfo info)
        : data_(info.size, 0)
        , usage_(info.usage)
        , memory_(info.memory)
        , hasDeviceAddress_(hasFlag(info.usage, BufferUsage::DeviceAddress)) {}

    MockBuffer(const MockBuffer&) = delete;
    MockBuffer& operator=(const MockBuffer&) = delete;
    MockBuffer(MockBuffer&&) = default;
    MockBuffer& operator=(MockBuffer&&) = default;

    uint64_t size() const { return data_.size(); }
    BufferUsage usage() const { return usage_; }

    void* mappedData() const {
        if (memory_ == MemoryLocation::GpuOnly) return nullptr;
        return const_cast<uint8_t*>(data_.data());
    }

    uint64_t deviceAddress() const {
        if (!hasDeviceAddress_) {
            log(LogLevel::Error, "deviceAddress() called on buffer created without Usage::DeviceAddress");
            return 0;
        }
        return fakeAddress_;
    }

private:
    std::vector<uint8_t> data_;
    BufferUsage usage_ = BufferUsage::Uniform;
    MemoryLocation memory_ = MemoryLocation::GpuOnly;
    bool hasDeviceAddress_ = false;
    uint64_t fakeAddress_ = 0xDEADBEEF;
};

class MockAllocator {
public:
    using Buffer = MockBuffer;
    using Image = MockImage;
    using ImageView = MockImageView;

    MockBuffer createBuffer(BufferCreateInfo info) {
        return MockBuffer(info);
    }

    MockImage createImage(ImageCreateInfo info) {
        return MockImage(info);
    }
};

static_assert(GpuBuffer<MockBuffer>, "MockBuffer must satisfy GpuBuffer concept");
static_assert(GpuImage<MockImage>, "MockImage must satisfy GpuImage concept");
static_assert(GpuAllocator<MockAllocator>, "MockAllocator must satisfy GpuAllocator concept");

}
