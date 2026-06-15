#pragma once

namespace RAGE {
    template <typename T>
    concept GpuContext = requires(const T &ctx) {
        typename T::Instance;
        typename T::PhysicalDevice;
        typename T::Device;
        { ctx.instance() };
        { ctx.physicalDevice() };
        { ctx.device() };
    };
}
