#pragma once

#include <cstdint>
#include <span>
#include "gpu/vulkan/vulkan_allocator.hpp"
#include "gpu/vulkan/vulkan_buffer.hpp"

namespace RAGE {
    class Voxel3D;

    /**
     * @brief CPU→GPU mirror of the free-standing (full-TRS) Voxel3D set: per-volume
     *        descriptors (transforms, brick dims, handle offset) plus all volumes'
     *        handle grids concatenated into one SSBO, re-uploaded every frame so
     *        animating transforms stays free. Budgets are injected (capacity rule);
     *        exceeding either throws.
     */
    class FreeVolumeGpuSync {
    public:
        FreeVolumeGpuSync(VulkanAllocator &allocator, uint32_t maxVolumes,
                          uint32_t maxHandleCells);

        /// Upload descriptors + handle grids for `volumes`; returns the count.
        uint32_t upload(std::span<Voxel3D *const> volumes);

        const VulkanBuffer &descsBuffer() const { return descsBuffer_; }
        const VulkanBuffer &handlesBuffer() const { return handlesBuffer_; }

    private:
        uint32_t maxVolumes_ = 0;
        uint32_t maxHandleCells_ = 0;
        VulkanBuffer descsBuffer_;
        VulkanBuffer handlesBuffer_;
    };
}
