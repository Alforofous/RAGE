#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include "engine/rendering/frame_context.hpp"
#include "engine/scene/renderable_node3d.hpp"
#include "engine/scene/voxel_data.hpp"
#include "gpu/vulkan/vulkan_descriptor_writer.hpp"
#include "gpu/vulkan/vulkan_shader_module.hpp"
#include "math/color.hpp"
#include "math/ivec.hpp"

namespace RAGE {
    class BrickPool;

    /**
     * A grid of coloured voxels rendered via a compute raycaster.
     *
     * As of M3, storage is **sparse**: a `Voxel3D` holds a `VoxelData` that owns a list
     * of brick handles into the renderer's shared `BrickPool`. Empty 8³ regions of the
     * grid consume no voxel memory at all. The shader's two-level DDA traverses the
     * renderer's world brick grid (outer) and individual bricks (inner) — there's no
     * per-`Voxel3D` voxel buffer or AABB-bound caster array anymore.
     *
     * Public API stays voxel-coordinate based; the brick split is internal. Construction
     * requires a reference to the renderer's `BrickPool` (typically
     * `renderer.brickPool()`); the pool must outlive every `Voxel3D` that draws from it.
     *
     * @note Voxel coordinates are zero-based corner-origin. The Node3D transform's
     *       translation places that corner. **Translation-only for M3** — rotated voxel
     *       grids don't tile onto an axis-aligned world brick grid, and rotation in the
     *       transform is ignored at render time.
     */
    class Voxel3D : public RenderableNode3D<VulkanShaderModule> {
    public:
        Voxel3D(BrickPool &pool, IVec3 dims, float voxelSize);

        void setVoxel(IVec3 c, Color rgba);
        Color voxel(IVec3 c) const;
        void fill(const std::function<Color(IVec3)> &fn);
        void fillSolid(Color rgba);
        void clear();

        /**
         * Bulk-import packed RGBA8 voxels matching `dimensions()`. Allocates bricks only
         * for non-empty 8³ regions. Fires the load progress / cancel hooks while
         * iterating brick rows. Mismatched `srcDims` throws.
         */
        void fillFromPackedRGBA8(const uint32_t *src, IVec3 srcDims);

        IVec3 dimensions() const { return dims_; }
        float voxelSize() const { return voxelSize_; }

        /** Storage backend. Renderer reads brick handles + placements through this. */
        const VoxelData *voxelData() const { return data_.get(); }
        VoxelData *voxelData() { return data_.get(); }

        using LoadProgressHook = std::function<void(float progress)>;
        void onLoadProgress(LoadProgressHook h) { loadProgress_ = std::move(h); }

        /**
         * Cooperative cancellation polled between brick rows during
         * `fillFromPackedRGBA8`. Returning true aborts the load with whatever bricks
         * were already allocated (they're released on `Voxel3D` destruction). Used by
         * the app to interrupt a long-running asset load at shutdown.
         */
        using LoadCancelHook = std::function<bool()>;
        void onLoadShouldCancel(LoadCancelHook h) { loadShouldCancel_ = std::move(h); }

        void prepareFrame(VulkanDescriptorWriter &writer, VkDescriptorSet set, PushConstantBuilder &pc,
                          const FrameContext &frame) override;

    private:
        IVec3 dims_{};
        float voxelSize_ = 1.0f;
        std::unique_ptr<VoxelData> data_;
        LoadProgressHook loadProgress_;
        LoadCancelHook loadShouldCancel_;
    };
}
