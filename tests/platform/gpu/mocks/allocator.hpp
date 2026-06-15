#pragma once

#include <tuple>
#include <vector>
#include "gpu/gpu_concepts.hpp"
#include "gpu/gpu_types.hpp"

namespace RAGE::Mocks {
    class MockImageView {
    public:
        MockImageView() = default;
        MockImageView(ImageViewCreateInfo info)
            : info_(info)
            , valid_(true) {}

        MockImageView(const MockImageView &) = delete;
        MockImageView &operator=(const MockImageView &) = delete;
        MockImageView(MockImageView &&) = default;
        MockImageView &operator=(MockImageView &&) = default;

        const ImageViewCreateInfo &info() const { return info_; }
        bool isValid() const { return valid_; }

    private:
        ImageViewCreateInfo info_{};
        bool valid_ = false;
    };

    class MockImage {
    public:
        using ViewType = MockImageView;

        MockImage() = default;
        MockImage(ImageCreateInfo info)
            : info_(info) {}

        MockImage(const MockImage &) = delete;
        MockImage &operator=(const MockImage &) = delete;
        MockImage(MockImage &&) = default;
        MockImage &operator=(MockImage &&) = default;

        ImageFormat format() const { return info_.format; }
        std::tuple<uint32_t, uint32_t, uint32_t> extent() const { return { info_.width, info_.height, info_.depth }; }
        uint32_t mipLevels() const { return info_.mipLevels; }
        uint32_t arrayLayers() const { return info_.arrayLayers; }
        SampleCount sampleCount() const { return info_.sampleCount; }

        MockImageView createView(ImageViewCreateInfo info) { return MockImageView(info); }

    private:
        ImageCreateInfo info_{};
    };

    class MockBuffer {
    public:
        MockBuffer() = default;
        MockBuffer(BufferCreateInfo info)
            : data_(info.size, 0)
            , usage_(info.usage)
            , memory_(info.memory) {}

        MockBuffer(const MockBuffer &) = delete;
        MockBuffer &operator=(const MockBuffer &) = delete;
        MockBuffer(MockBuffer &&) = default;
        MockBuffer &operator=(MockBuffer &&) = default;

        uint64_t size() const { return data_.size(); }
        BufferUsage usage() const { return usage_; }

        void *mappedData() const {
            if (memory_ == MemoryLocation::GpuOnly) {
                return nullptr;
            }

            return const_cast<uint8_t *>(data_.data());
        }

        uint64_t deviceAddress() const { return hasFlag(usage_, BufferUsage::ShaderDeviceAddress) ? 0xDEADBEEFu : 0u; }

    private:
        std::vector<uint8_t> data_;
        BufferUsage usage_ = BufferUsage::Uniform;
        MemoryLocation memory_ = MemoryLocation::GpuOnly;
    };

    class MockAllocator {
    public:
        using Buffer = MockBuffer;
        using Image = MockImage;

        MockBuffer createBuffer(BufferCreateInfo info) { return MockBuffer(info); }
        MockImage createImage(ImageCreateInfo info) { return MockImage(info); }
    };

    static_assert(GpuBuffer<MockBuffer>, "MockBuffer must satisfy GpuBuffer concept");
    static_assert(GpuImage<MockImage>, "MockImage must satisfy GpuImage concept");
    static_assert(GpuAllocator<MockAllocator>, "MockAllocator must satisfy GpuAllocator concept");
}
