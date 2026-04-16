#pragma once

#include <concepts>

namespace RAGE {
    template <typename T>
    concept GpuContext = requires(const T ctx) {
        typename T::Instance;
        typename T::PhysicalDevice;
        typename T::Device;
        { ctx.instance() } -> std::convertible_to<typename T::Instance>;
        { ctx.physicalDevice() } -> std::convertible_to<typename T::PhysicalDevice>;
        { ctx.device() } -> std::convertible_to<typename T::Device>;
    } && std::movable<T> && !std::copyable<T>;
}