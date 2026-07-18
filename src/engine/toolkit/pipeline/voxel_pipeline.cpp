#include "voxel_pipeline.hpp"

#include <stdexcept>
#include <utility>
#include "engine/rendering/renderer.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/node3d.hpp"
#include "gpu/vulkan/vulkan_queue.hpp"

namespace RAGE::Toolkit {
    namespace {
        VulkanContextCreateInfo contextInfo(const VoxelPipelineSettings &settings,
                                            const WindowSurfaceSource &surface) {
            VulkanContextCreateInfo info;
            info.appName = settings.appName;
            info.enableValidation = settings.enableValidation;
            info.requiredExtensions = surface.instanceExtensions;
            return info;
        }

        VkSurfaceKHR createSurfaceOrThrow(const WindowSurfaceSource &surface, VkInstance instance) {
            if (!surface.createSurface) {
                throw std::invalid_argument("VoxelPipeline: WindowSurfaceSource.createSurface is null");
            }
            return surface.createSurface(instance);
        }

        VulkanSwapchainCreateInfo swapchainInfo(VulkanContext &ctx, VkSurfaceKHR surface,
                                                std::pair<uint32_t, uint32_t> extent, bool vsync) {
            return VulkanSwapchainCreateInfo{
                .physicalDevice = ctx.physicalDevice(),
                .device = ctx.vkDevice(),
                .surface = surface,
                .graphicsQueueFamily = ctx.graphicsQueue().queueFamily(),
                .width = extent.first,
                .height = extent.second,
                .vsync = vsync,
            };
        }
    }

    VoxelPipeline::SurfaceGuard::~SurfaceGuard() {
        if (surface_ != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
        }
    }

    VoxelPipeline::VoxelPipeline(WindowSurfaceSource surface, VoxelPipelineSettings settings)
        : ctx_(contextInfo(settings, surface))
        , allocator_(ctx_.vkInstance(), ctx_.physicalDevice(), ctx_.vkDevice())
        , surface_(ctx_.vkInstance(), createSurfaceOrThrow(surface, ctx_.vkInstance()))
        , swapchain_(swapchainInfo(ctx_, surface_.handle(), surface.framebufferExtent(),
                                   settings.vsync))
        , renderer_(ctx_, allocator_, swapchain_, settings.limits)
        , framebufferExtent_(std::move(surface.framebufferExtent)) {
        auto shader = std::make_shared<const VulkanShaderModule>(VulkanShaderModule::fromFile(
            ctx_.vkDevice(), settings.shaderDir / "voxel_raycast.comp.spv"));
        voxelMaterial_ = std::make_shared<Material<VulkanShaderModule>>(std::move(shader));
    }

    void VoxelPipeline::render(Node3D &scene, const Camera &camera) {
        const auto [width, height] = framebufferExtent_();
        if (width == 0 || height == 0) {
            return;
        }
        renderer_.render(scene, camera, FrameExtent{ .width = width, .height = height });
    }
}
