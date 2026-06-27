#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>
#include <memory>
#include "engine/materials/pipeline_cache.hpp"
#include "engine/rendering/ambient_light.hpp"
#include "engine/rendering/frame_context.hpp"
#include "engine/rendering/pixel_debug.hpp"
#include "engine/rendering/world_brick_grid.hpp"
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/renderable_node3d.hpp"
#include "gpu/vulkan/vulkan_buffer.hpp"
#include "gpu/gpu_queue_kind.hpp"
#include "gpu/vulkan/vulkan_allocator.hpp"
#include "gpu/vulkan/vulkan_command_buffer.hpp"
#include "gpu/vulkan/vulkan_command_pool.hpp"
#include "gpu/vulkan/vulkan_context.hpp"
#include "gpu/vulkan/vulkan_descriptor_pool.hpp"
#include "gpu/vulkan/vulkan_render_target.hpp"
#include "gpu/vulkan/vulkan_semaphore_pool.hpp"
#include "gpu/vulkan/vulkan_shader_module.hpp"
#include "gpu/vulkan/vulkan_swapchain.hpp"

namespace RAGE {
    class Camera;
    class DirectionalLight;
    class Node3D;
    class Voxel3D;

    struct FrameExtent {
        uint32_t width = 0;
        uint32_t height = 0;
        bool operator==(const FrameExtent &) const = default;
    };

    /**
     * A single rendering pass.
     *
     * For now: identifies a render target to dispatch into. A future field will narrow which
     * renderables participate (the "scene filter" from the architecture doc) — added when
     * filtering has a real consumer (depth pass, shadow pass, …).
     *
     * Pass never knows how a renderable executes — that lives on Material via PipelineCache.
     *
     * @note `target == nullptr` resolves to the renderer's internal main target at render time.
     *       This makes user-added passes survive swapchain recreation without re-pointering.
     */
    struct Pass {
        VulkanRenderTarget *target = nullptr;
    };

    /**
     * Minimal frame renderer.
     *
     * Owns the per-frame infrastructure that drives one compute pass per visible
     * RenderableNode3D against a shared render target, then blits the target onto the swapchain
     * image and presents. Lifts the bookkeeping previously sprawled across src/app/main.cpp into a
     * single class with a `render(root, camera, extent)` entry point.
     *
     * Ownership:
     * - One graphics command pool, one descriptor pool, one PipelineCache.
     * - One shared `VulkanRenderTarget` sized to the current frame extent.
     * - Per-swapchain-image renderDone semaphores (per the canonical Vulkan-validated pattern).
     * - One frame-in-flight (single `VulkanPendingSubmission`) — sufficient for the smoke path.
     *
     * Resize handling is internal: the renderer detects `outOfDate` on acquire or present, or a
     * mismatch between the requested extent and the last one, and recreates the swapchain plus
     * its dependent resources on the next render() call. Callers never invoke onResize().
     *
     * Vulkan-coupled by design — this is the Layer-4 inflection point where backend choice
     * manifests. A future Metal renderer would mirror this class with its own backend types.
     *
     * @note Camera is currently accepted but unused in the smoke path; it becomes load-bearing
     *       once a real raycaster material lands (push-constant matrices).
     */
    class Renderer {
    public:
        Renderer() = delete;
        Renderer(VulkanContext &ctx, VulkanAllocator &allocator, VulkanSwapchain &swapchain);
        ~Renderer();

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) = delete;
        Renderer &operator=(Renderer &&) = delete;

        void render(Node3D &root, const Camera &camera, FrameExtent extent);

        void addPass(Pass pass) { passes_.push_back(pass); }
        void clearPasses() { passes_.clear(); }
        std::span<const Pass> passes() const { return passes_; }

        void addLight(std::shared_ptr<DirectionalLight> light);
        void clearLights() { directionalLights_.clear(); }
        std::span<const std::shared_ptr<DirectionalLight>> lights() const { return directionalLights_; }

        void setAmbientLight(AmbientLight ambient) { ambient_ = ambient; }
        AmbientLight ambientLight() const { return ambient_; }

        // Debug toggles surfaced through FrameUniforms. App-side UI flips them at runtime;
        // engine and shader read them via the existing uniform binding. No new descriptors.
        void setMipSkipEnabled(bool enabled) { mipSkipEnabled_ = enabled; }
        bool mipSkipEnabled() const { return mipSkipEnabled_; }
        void setHeatmapMode(int32_t mode) { heatmapMode_ = mode; }
        int32_t heatmapMode() const { return heatmapMode_; }
        void setHeatmapMaxSteps(int32_t maxSteps) { heatmapMaxSteps_ = maxSteps; }
        int32_t heatmapMaxSteps() const { return heatmapMaxSteps_; }

        /**
         * Shared brick pool that will back every `Voxel3D`'s storage once M3-C lands.
         * Lives for the renderer's lifetime; stable address.
         */
        BrickPool &brickPool() { return brickPool_; }
        const BrickPool &brickPool() const { return brickPool_; }

        /**
         * Top-level sparse grid mapping world brick coords to handles into the brick pool.
         * Rebuilt each frame from the scene's `VoxelData` placements.
         */
        const WorldBrickGrid &worldBrickGrid() const { return worldBrickGrid_; }

        // Debug-only: pick a single pixel for shader-side introspection. setPickTarget queues
        // the request for the next render(); tryReadPick consumes the result after that frame
        // completes. See engine/rendering/pixel_debug.hpp.
        void setPickTarget(int32_t pixelX, int32_t pixelY);
        bool tryReadPick(PixelDebug &out);

        // Observer hooks fired during render(). Each is a single-slot std::function; pass a
        // null/empty value to clear. The engine has no knowledge of what listens — see
        // src/app/profiler.hpp for the canonical subscriber.
        using FrameHook = std::function<void()>;
        using GpuPassHook = std::function<void(const char *passName, VkCommandBuffer cmd)>;
        using FrameImageHook =
            std::function<void(const void *rgbaBytes, uint16_t width, uint16_t height)>;
        using PhaseHook = std::function<void(const char *name)>;

        struct SwapchainInfo {
            VkDevice device = VK_NULL_HANDLE;
            VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
            VkInstance instance = VK_NULL_HANDLE;
            VkQueue graphicsQueue = VK_NULL_HANDLE;
            uint32_t graphicsQueueFamily = 0;
            VkFormat colorFormat = VK_FORMAT_UNDEFINED;
            uint32_t imageCount = 0;
            const VkImage *images = nullptr;
            uint32_t width = 0;
            uint32_t height = 0;
        };

        struct UiRenderContext {
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            VkImage swapImage = VK_NULL_HANDLE;
            uint32_t swapImageIndex = 0;
            uint32_t width = 0;
            uint32_t height = 0;
        };

        using SwapchainRebuiltHook = std::function<void(const SwapchainInfo &)>;
        using UiRenderHook = std::function<void(const UiRenderContext &)>;

        void onFrameEnd(FrameHook h) { frameEnd_ = std::move(h); }
        void onBeforeGpuPass(GpuPassHook h) { beforeGpuPass_ = std::move(h); }
        void onAfterGpuPass(GpuPassHook h) { afterGpuPass_ = std::move(h); }
        void onFrameImage(FrameImageHook h) { frameImage_ = std::move(h); }
        void onPhaseBegin(PhaseHook h) { phaseBegin_ = std::move(h); }
        void onPhaseEnd(PhaseHook h) { phaseEnd_ = std::move(h); }
        void onSwapchainRebuilt(SwapchainRebuiltHook h);
        void onUiRender(UiRenderHook h) { uiRender_ = std::move(h); }

    private:
        using Renderable = RenderableNode3D<VulkanShaderModule>;

        void rebuildFrameResources(FrameExtent extent);
        void collectVisible(Node3D &node, std::vector<Renderable *> &out);
        void collectShadowCasters(Node3D &node);
        void recordFrame(VulkanRecorder<queue_kind::Graphics> &rec, VkImage swapImage, uint32_t swapImageIndex,
                         uint32_t swapW, uint32_t swapH, std::span<Renderable *const> renderables,
                         const FrameContext &frame);
        void recordPass(VulkanRecorder<queue_kind::Graphics> &rec, VulkanRenderTarget &target,
                        std::span<Renderable *const> renderables, const FrameContext &frame, bool isFirstPass,
                        bool isLastPass);
        void drainInFlight();
        VulkanRenderTarget &resolveTarget(const Pass &pass);

        VulkanContext &ctx_;
        VulkanAllocator &allocator_;
        VulkanSwapchain &swapchain_;

        VulkanCommandPool<queue_kind::Graphics> cmdPool_;
        VulkanDescriptorPool descPool_;
        PipelineCache pipelineCache_;

        std::optional<VulkanRenderTarget> renderTarget_;
        std::optional<VulkanRenderTarget> thumbnailTarget_;
        std::optional<VulkanBuffer> frameUniformBuffer_;
        std::optional<VulkanBuffer> brickPoolBuffer_;
        std::optional<VulkanBuffer> worldBrickGridHandlesBuffer_;
        std::optional<VulkanBuffer> worldBrickGridParamsBuffer_;
        std::optional<VulkanBuffer> pixelDebugBuffer_;
        std::optional<VulkanBuffer> thumbnailStaging_;
        bool thumbnailQueued_ = false;
        int32_t pendingPickX_ = -1;
        int32_t pendingPickY_ = -1;
        bool pickResultReady_ = false;
        PixelDebug pickResult_{};
        std::vector<VulkanSemaphoreHandle> renderDoneByImage_;
        std::vector<VkImage> swapchainImageCache_;
        std::optional<VulkanPendingSubmission<queue_kind::Graphics>> inFlight_;
        std::vector<Pass> passes_;
        std::vector<std::shared_ptr<DirectionalLight>> directionalLights_;
        AmbientLight ambient_{};
        bool mipSkipEnabled_ = true;
        int32_t heatmapMode_ = 0;
        int32_t heatmapMaxSteps_ = 1024;
        std::vector<Voxel3D *> shadowCasters_;
        BrickPool brickPool_;
        WorldBrickGrid worldBrickGrid_;
        std::vector<VoxelDataWorldPlacement> brickPlacementsScratch_;

        FrameHook frameEnd_;
        GpuPassHook beforeGpuPass_;
        GpuPassHook afterGpuPass_;
        FrameImageHook frameImage_;
        PhaseHook phaseBegin_;
        PhaseHook phaseEnd_;
        SwapchainRebuiltHook swapchainRebuilt_;
        UiRenderHook uiRender_;

        FrameExtent lastExtent_{};
        bool needsRecreate_ = true;
    };
}
