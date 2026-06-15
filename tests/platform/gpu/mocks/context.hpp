#pragma once

#include "gpu/gpu_context.hpp"

namespace RAGE::Mocks {
    struct MockInstance {};
    struct MockPhysicalDevice {};
    struct MockDevice {};

    class MockContext {
    public:
        using Instance = MockInstance;
        using PhysicalDevice = MockPhysicalDevice;
        using Device = MockDevice;

        MockContext() = default;

        MockContext(const MockContext &) = delete;
        MockContext &operator=(const MockContext &) = delete;
        MockContext(MockContext &&) = default;
        MockContext &operator=(MockContext &&) = default;

        MockInstance instance() const { return instance_; }
        MockPhysicalDevice physicalDevice() const { return physicalDevice_; }
        MockDevice device() const { return device_; }

    private:
        MockInstance instance_{};
        MockPhysicalDevice physicalDevice_{};
        MockDevice device_{};
    };

    static_assert(GpuContext<MockContext>, "MockContext must satisfy GpuContext concept");
}
