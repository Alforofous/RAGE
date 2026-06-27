#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include "engine/rendering/frame_context.hpp"
#include "engine/scene/renderable_node3d.hpp"
#include "engine/scene/voxel_occupancy_mip.hpp"
#include "gpu/vulkan/vulkan_allocator.hpp"
#include "gpu/vulkan/vulkan_buffer.hpp"
#include "gpu/vulkan/vulkan_descriptor_writer.hpp"
#include "gpu/vulkan/vulkan_shader_module.hpp"
#include "math/color.hpp"
#include "math/ivec.hpp"

namespace RAGE {
    /**
     * A grid of coloured voxels rendered via a compute raycaster.
     *
     * Owns a dense RGBA8 storage buffer (one uint32 per voxel, linear xyz layout) plus the
     * standard Node3D transform. Use setMaterial() with a Material wrapping the voxel raycast
     * shader; PipelineCache deduplicates the pipeline across Voxel3Ds that share the material.
     *
     * Memory management is fully encapsulated. Users mutate the grid via setVoxel / fill / resize;
     * the GPU buffer is host-visible and host-coherent so CPU writes are immediately visible to
     * the GPU. The internal writeVoxelCpu / uploadDirty pipeline is private today but structurally
     * present — promote it to public when batching or non-coherent backends arrive.
     *
     * @note Voxel coordinates are zero-based corner-origin: the object's local-space AABB runs
     *       from (0,0,0) to (dims * voxelSize). The Node3D transform places that corner.
     */
    class Voxel3D : public RenderableNode3D<VulkanShaderModule> {
    public:
        Voxel3D(VulkanAllocator &allocator, IVec3 dims, float voxelSize);

        void setVoxel(IVec3 c, Color rgba);
        Color voxel(IVec3 c) const;
        void fill(const std::function<Color(IVec3)> &fn);
        void fillSolid(Color rgba);
        void clear();

        /**
         * Bulk-copy packed RGBA8 voxels straight into the buffer. `src` must point to
         * `srcDims.x * srcDims.y * srcDims.z` uint32 values in the same linear layout as the
         * GPU buffer (z-major, y-mid, x-fastest). When `srcDims == dimensions()` this is a
         * single memcpy. Mismatched dims throws — call `resize()` first to match.
         *
         * Use this for content imports (vox loader, etc.) instead of `fill(lambda)` — the
         * lambda path was ~700ns/cell on a 256³ scene (~12s total) due to `std::function`
         * dispatch and per-cell Color round-trips; this path is one memcpy.
         */
        void fillFromPackedRGBA8(const uint32_t *src, IVec3 srcDims);

        void resize(IVec3 newDims);

        IVec3 dimensions() const { return dims_; }
        float voxelSize() const { return voxelSize_; }
        const VulkanBuffer *voxelBuffer() const { return buffer_.has_value() ? &*buffer_ : nullptr; }

        /**
         * Occupancy mip pyramid for empty-space skipping during DDA. Level L is a bit-packed
         * grid of `ceil(dims / 2^(L+1))` cells, where each bit answers "is any voxel in the
         * corresponding 2^(L+1) block occupied?". Levels stop when dims collapse to 1×1×1 or
         * after a hard cap.
         *
         * Built eagerly inside the bulk-fill methods (`fillSolid`, `clear`, `fill`).
         * `setVoxel` leaves the mip silently stale — fine while the renderer's `mipSkipEnabled`
         * is false; callers doing live voxel edits should call `rebuildOccupancyMipNow()`
         * afterwards (typically on a worker thread).
         */
        const VulkanBuffer *occupancyMipBuffer();
        const OccupancyMipLayout &occupancyMipLayout() const { return mipLayout_; }

        /**
         * Force a synchronous mip rebuild. Called automatically by the bulk-fill methods;
         * exposed publicly so callers can drive the build from a worker thread for async
         * loading. The engine knows nothing about threads — the caller owns concurrency.
         *
         * Fires the `MipBuildProgress` callback (if set) at the start, between mip levels,
         * and at completion. Progress values are in [0.0, 1.0]. Callback runs on the calling
         * thread — if called from a worker, the callback fires on the worker too.
         */
        void rebuildOccupancyMipNow();

        using MipBuildProgressHook = std::function<void(float progress)>;
        void onMipBuildProgress(MipBuildProgressHook h) { mipBuildProgress_ = std::move(h); }

        /**
         * Cooperative cancellation. If the callback returns true between mip Z slices, the
         * build aborts and the mip buffer is left partially-filled. Useful for app shutdown
         * so an in-flight mip build doesn't block process exit.
         */
        void onMipBuildShouldCancel(MipBuildCancelFn h) { mipBuildShouldCancel_ = std::move(h); }

        void prepareFrame(VulkanDescriptorWriter &writer, VkDescriptorSet set, PushConstantBuilder &pc,
                          const FrameContext &frame) override;

    private:
        void writeVoxelCpu(IVec3 c, Color rgba);
        void uploadDirty();

        void rebuildBuffer();
        void rebuildOccupancyMipLayout();
        void rebuildOccupancyMip();
        bool inBounds(IVec3 c) const;
        size_t coordToIndex(IVec3 c) const;
        uint32_t *mappedVoxels();
        const uint32_t *mappedVoxels() const;

        VulkanAllocator *allocator_ = nullptr;
        IVec3 dims_{};
        float voxelSize_ = 1.0f;
        std::optional<VulkanBuffer> buffer_;
        std::optional<VulkanBuffer> mipBuffer_;
        OccupancyMipLayout mipLayout_;
        bool dirty_ = false;
        MipBuildProgressHook mipBuildProgress_;
        MipBuildCancelFn mipBuildShouldCancel_;
    };
}
