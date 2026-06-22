#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include "engine/rendering/frame_context.hpp"
#include "engine/scene/renderable_node3d.hpp"
#include "gpu/vulkan/vulkan_allocator.hpp"
#include "gpu/vulkan/vulkan_buffer.hpp"
#include "gpu/vulkan/vulkan_descriptor_writer.hpp"
#include "gpu/vulkan/vulkan_shader_module.hpp"
#include "math/color.hpp"
#include "math/ivec.hpp"
#include "math/mat.hpp"
#include "math/vec.hpp"

namespace RAGE {
    /**
     * Push-constant block consumed by the voxel raycast compute shader.
     *
     * Layout matches GLSL std430 (vec3+float pack via Vec4-with-scalar-in-w). Total 128 bytes —
     * the Vulkan spec-minimum push-constant size, so this struct is portable across all
     * conformant hardware.
     */
    struct VoxelPushConstants {
        Mat4 invModel;
        Vec4 cameraPos_fovY;
        Vec4 cameraForward_aspect;
        Vec4 cameraUp_voxelSize;
        int32_t dimsX = 0;
        int32_t dimsY = 0;
        int32_t dimsZ = 0;
        int32_t _pad = 0;
    };
    static_assert(sizeof(VoxelPushConstants) == 128, "VoxelPushConstants must fit Vulkan's 128-byte push-constant min");

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

        void resize(IVec3 newDims);

        IVec3 dimensions() const { return dims_; }
        float voxelSize() const { return voxelSize_; }

        void prepareFrame(VulkanDescriptorWriter &writer, VkDescriptorSet set, PushConstantBuilder &pc,
                          const FrameContext &frame) override;

    private:
        void writeVoxelCpu(IVec3 c, Color rgba);
        void uploadDirty();

        void rebuildBuffer();
        bool inBounds(IVec3 c) const;
        size_t coordToIndex(IVec3 c) const;
        uint32_t *mappedVoxels();
        const uint32_t *mappedVoxels() const;

        VulkanAllocator *allocator_ = nullptr;
        IVec3 dims_{};
        float voxelSize_ = 1.0f;
        std::optional<VulkanBuffer> buffer_;
        bool dirty_ = false;
    };
}
