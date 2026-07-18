#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include "engine/materials/material.hpp"
#include "engine/rendering/renderer.hpp"
#include "engine/toolkit/pipeline/window_surface_source.hpp"
#include "gpu/vulkan/vulkan_allocator.hpp"
#include "gpu/vulkan/vulkan_context.hpp"
#include "gpu/vulkan/vulkan_shader_module.hpp"
#include "gpu/vulkan/vulkan_swapchain.hpp"

namespace RAGE {
    class Camera;
    class Node3D;
}

namespace RAGE::Toolkit {
    /**
     * @brief Everything the pipeline needs to boot, decided at the top level
     *        (capacity-injection rule: the app derives limits, the engine owns
     *        no capacity constants).
     */
    struct VoxelPipelineSettings {
        std::string appName = "RAGE";
        /// Directory containing compiled SPIR-V shaders (voxel_raycast.comp.spv).
        std::filesystem::path shaderDir;
        bool vsync = true;
        bool enableValidation = true;
        Renderer::WorldLimits limits{};
    };

    /**
     * @brief The one Vulkan-aware entry point for applications: owns the Vulkan
     *        context, allocator, presentation surface, swapchain, renderer, and
     *        the default voxel material. Apps construct it from a
     *        WindowSurfaceSource and call render(scene, camera) per frame —
     *        no Vulkan types appear at the call site.
     *
     * Game-agnostic by design: the pipeline does not know about worlds, chunks,
     * or streaming — it receives a scene graph and raycasts the Voxel3Ds in it.
     *
     * @note Transitional seams (renderer(), context()) exist while the API
     *       north star lands (N4–N9, `.claude/plans/api-north-star.md`); they
     *       shrink milestone by milestone.
     */
    class VoxelPipeline {
    public:
        VoxelPipeline() = delete;
        VoxelPipeline(WindowSurfaceSource surface, VoxelPipelineSettings settings);

        VoxelPipeline(const VoxelPipeline &) = delete;
        VoxelPipeline &operator=(const VoxelPipeline &) = delete;
        VoxelPipeline(VoxelPipeline &&) = delete;
        VoxelPipeline &operator=(VoxelPipeline &&) = delete;

        /**
         * @brief Render one frame: polls the framebuffer extent and drives the
         *        renderer. Skips (no-op) while the window is minimized.
         */
        void render(Node3D &scene, const Camera &camera);

        /// Default material for voxel volumes (compute raycast shader).
        std::shared_ptr<Material<VulkanShaderModule>> defaultVoxelMaterial() const {
            return voxelMaterial_;
        }

        /// Bytes currently allocated on the GPU by the pipeline's allocator.
        size_t gpuMemoryUsedBytes() const { return allocator_.stats().usedBytes; }

        /// Transitional seam: direct renderer access while N4–N9 land.
        Renderer &renderer() { return renderer_; }
        /// Transitional seam: app-side tooling (debug UI) that records Vulkan work.
        VulkanContext &context() { return ctx_; }

    private:
        /// RAII ownership of the presentation surface created via the source callback.
        class SurfaceGuard {
        public:
            SurfaceGuard(VkInstance instance, VkSurfaceKHR surface)
                : instance_(instance)
                , surface_(surface) {}
            ~SurfaceGuard();
            SurfaceGuard(const SurfaceGuard &) = delete;
            SurfaceGuard &operator=(const SurfaceGuard &) = delete;
            SurfaceGuard(SurfaceGuard &&) = delete;
            SurfaceGuard &operator=(SurfaceGuard &&) = delete;

            VkSurfaceKHR handle() const { return surface_; }

        private:
            VkInstance instance_ = VK_NULL_HANDLE;
            VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        };

        // Declaration order is destruction-order-load-bearing: the renderer must die
        // before the swapchain, the swapchain before the surface, everything before
        // the context.
        VulkanContext ctx_;
        VulkanAllocator allocator_;
        SurfaceGuard surface_;
        VulkanSwapchain swapchain_;
        Renderer renderer_;
        std::function<std::pair<uint32_t, uint32_t>()> framebufferExtent_;
        std::shared_ptr<Material<VulkanShaderModule>> voxelMaterial_;
    };
}
