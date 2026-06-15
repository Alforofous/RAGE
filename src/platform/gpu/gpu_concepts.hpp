#pragma once

#include <concepts>
#include <cstdint>
#include <tuple>
#include "gpu_queue.hpp"
#include "gpu_types.hpp"

namespace RAGE {
    template <typename T>
    concept GpuBuffer = requires(const T buf) {
        { buf.size() } -> std::convertible_to<uint64_t>;
        { buf.usage() } -> std::convertible_to<BufferUsage>;
        { buf.mappedData() } -> std::convertible_to<void *>;
        { buf.deviceAddress() } -> std::convertible_to<uint64_t>;
    } && MoveOnly<T>;

    template <typename T>
    concept GpuImageView = MoveOnly<T> && std::default_initializable<T> && requires(const T view) {
        { view.info() } -> std::convertible_to<ImageViewCreateInfo>;
        { view.isValid() } -> std::convertible_to<bool>;
    };

    template <typename T>
    concept GpuImage = requires(const T img) {
        { img.format() } -> std::convertible_to<ImageFormat>;
        { img.extent() } -> std::convertible_to<std::tuple<uint32_t, uint32_t, uint32_t>>;
        { img.mipLevels() } -> std::convertible_to<uint32_t>;
        { img.arrayLayers() } -> std::convertible_to<uint32_t>;
        { img.sampleCount() } -> std::convertible_to<SampleCount>;
    } && requires(T img, ImageViewCreateInfo viewInfo) {
        { img.createView(viewInfo) };
        requires GpuImageView<decltype(img.createView(viewInfo))>;
    } && MoveOnly<T>;

    template <typename T>
    concept GpuAllocator = requires(T alloc, BufferCreateInfo bufInfo, ImageCreateInfo imgInfo) {
        typename T::Buffer;
        typename T::Image;
        { alloc.createBuffer(bufInfo) } -> std::same_as<typename T::Buffer>;
        { alloc.createImage(imgInfo) } -> std::same_as<typename T::Image>;
    } && requires {
        requires GpuBuffer<typename T::Buffer>;
        requires GpuImage<typename T::Image>;
    };
}
