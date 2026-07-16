#pragma once

#include "engine/scene/world_brick_grid.hpp"
#include "gpu/gpu_queue_kind.hpp"
#include "gpu/vulkan/vulkan_allocator.hpp"
#include "gpu/vulkan/vulkan_buffer.hpp"
#include "gpu/vulkan/vulkan_command_pool.hpp"
#include "gpu/vulkan/vulkan_image.hpp"
#include "gpu/vulkan/vulkan_sampler.hpp"
#include "math/ivec.hpp"

namespace RAGE {
    /**
     * @brief CPU→GPU mirror of the toroidal `WorldBrickGrid` (the `GpuSvdagCache`
     *        model): owns the params UBO, the handle SSBO, and the optional R32_UINT
     *        3D texture with its staging buffer. `upload()` copies the grid into the
     *        mapped buffers; when the texture path is wanted it also fills staging and
     *        arms `recordTextureUpload()` for the next command recording.
     *
     * `textureValid()` reports whether the GPU texture matches the current grid —
     * uploads made with the texture path disabled invalidate it, so the shader flag
     * can stay honest on quiet frames instead of only on re-upload frames.
     */
    class WorldGridGpuSync {
    public:
        WorldGridGpuSync(VulkanAllocator &allocator, VkDevice device, IVec3 gridDims);

        /// Copy params + full handle storage into the mapped buffers. `wantTexture`
        /// additionally fills staging and arms the next `recordTextureUpload`.
        void upload(const WorldBrickGrid &grid, float brickWorldSize, bool wantTexture);

        /**
         * @brief Record the armed staging→image copy (with layout transitions), or the
         *        one-time Undefined→ShaderReadOnly init on the first frame. Returns
         *        true when an armed upload was recorded (for GPU zone bracketing).
         */
        bool recordTextureUpload(VulkanRecorder<queue_kind::Graphics> &rec);

        bool textureUploadArmed() const { return uploadDims_.x > 0; }
        /// True while the GPU texture contents match the current grid.
        bool textureValid() const { return textureValid_; }

        const VulkanBuffer &paramsBuffer() const { return paramsBuffer_; }
        const VulkanBuffer &handlesBuffer() const { return handlesBuffer_; }
        const VulkanImageView &imageView() const { return imageView_; }
        const VulkanSampler &sampler() const { return sampler_; }

    private:
        IVec3 gridDims_{};
        VulkanBuffer paramsBuffer_;
        VulkanBuffer handlesBuffer_;
        VulkanBuffer stagingBuffer_;
        VulkanImage image_;
        VulkanImageView imageView_;
        VulkanSampler sampler_;
        IVec3 uploadDims_{ 0, 0, 0 };
        bool imageInitialized_ = false;
        bool textureValid_ = false;
    };
}
