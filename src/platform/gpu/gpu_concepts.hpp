#pragma once

#include <concepts>
#include <cstdint>
#include "gpu_types.hpp"

namespace RAGE {
    template <typename T>
    concept GpuBuffer = requires(const T buf) {
        { buf.size() } -> std::convertible_to<uint64_t>;
        { buf.usage() } -> std::convertible_to<BufferUsage>;
        { buf.mappedData() } -> std::convertible_to<void *>;
        { buf.deviceAddress() } -> std::convertible_to<uint64_t>;
    } && std::movable<T> && !std::copyable<T>;

    template <typename T>
    concept GpuImageView = std::movable<T> && !std::copyable<T>;

    template <typename T>
    concept GpuImage = requires(const T img, ImageViewCreateInfo viewInfo) {
        { img.format() } -> std::convertible_to<ImageFormat>;
        { img.extent() } -> std::convertible_to<Extent3D>;
        { img.mipLevels() } -> std::convertible_to<uint32_t>;
        { img.arrayLayers() } -> std::convertible_to<uint32_t>;
        { img.sampleCount() } -> std::convertible_to<uint32_t>;
        { img.view() };
        requires GpuImageView<std::remove_cvref_t<decltype(std::declval<const T>().view())>>;
    } && requires(T img, ImageViewCreateInfo viewInfo) {
        { img.createView(viewInfo) };
        requires GpuImageView<decltype(img.createView(viewInfo))>;
    } && std::movable<T> && !std::copyable<T>;

    template <typename T>
    concept GpuAllocator = requires(T alloc, BufferCreateInfo bufInfo, ImageCreateInfo imgInfo) {
        typename T::Buffer;
        typename T::Image;
        typename T::ImageView;
        { alloc.createBuffer(bufInfo) } -> std::same_as<typename T::Buffer>;
        { alloc.createImage(imgInfo) } -> std::same_as<typename T::Image>;
    } && requires {
        requires GpuBuffer<typename T::Buffer>;
        requires GpuImage<typename T::Image>;
        requires GpuImageView<typename T::ImageView>;
    };
}
