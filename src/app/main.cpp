#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "gpu/gpu_queue_kind.hpp"
#include "gpu/gpu_types.hpp"
#include "gpu/vulkan/vulkan_allocator.hpp"
#include "gpu/vulkan/vulkan_command_buffer.hpp"
#include "gpu/vulkan/vulkan_command_pool.hpp"
#include "gpu/vulkan/vulkan_compute_pipeline.hpp"
#include "gpu/vulkan/vulkan_context.hpp"
#include "gpu/vulkan/vulkan_descriptor_layout_cache.hpp"
#include "gpu/vulkan/vulkan_descriptor_pool.hpp"
#include "gpu/vulkan/vulkan_descriptor_writer.hpp"
#include "gpu/vulkan/vulkan_queue.hpp"
#include "gpu/vulkan/vulkan_render_target.hpp"
#include "gpu/vulkan/vulkan_semaphore_pool.hpp"
#include "gpu/vulkan/vulkan_shader_module.hpp"
#include "gpu/vulkan/vulkan_swapchain.hpp"
#include "window.hpp"

namespace {
    using namespace RAGE;

    VulkanContextCreateInfo buildContextInfo() {
        uint32_t glfwExtCount = 0;
        const char **glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);

        VulkanContextCreateInfo info;
        info.appName = "RAGE Smoke";
        info.enableValidation = true;
        info.requiredExtensions.assign(glfwExts, glfwExts + glfwExtCount);

        return info;
    }

    class SurfaceGuard {
    public:
        SurfaceGuard(VkInstance instance, GLFWwindow *window)
            : instance_(instance) {
            if (glfwCreateWindowSurface(instance_, window, nullptr, &surface_) != VK_SUCCESS) {
                throw std::runtime_error("glfwCreateWindowSurface failed");
            }
        }
        ~SurfaceGuard() {
            if (surface_ != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(instance_, surface_, nullptr);
            }
        }
        SurfaceGuard(const SurfaceGuard &) = delete;
        SurfaceGuard &operator=(const SurfaceGuard &) = delete;
        SurfaceGuard(SurfaceGuard &&) = delete;
        SurfaceGuard &operator=(SurfaceGuard &&) = delete;

        VkSurfaceKHR handle() const { return surface_; }

    private:
        VkInstance instance_ = VK_NULL_HANDLE;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    };

    std::filesystem::path executableDir(const char *argv0) {
        std::error_code ec;
        const std::filesystem::path exePath = std::filesystem::weakly_canonical(argv0, ec);
        if (ec || exePath.empty()) {
            return std::filesystem::current_path();
        }

        return exePath.parent_path();
    }
}

int main(int /*argc*/, char **argv) {
    try {
        App::Window window(1280, 720, "RAGE Smoke");

        VulkanContext ctx(buildContextInfo());
        VulkanAllocator allocator(ctx.vkInstance(), ctx.physicalDevice(), ctx.vkDevice());

        const SurfaceGuard surfaceGuard(ctx.vkInstance(), window.glfwHandle());
        VkSurfaceKHR surface = surfaceGuard.handle();

        const std::filesystem::path shaderDir = executableDir(argv[0]) / "shaders";
        const VulkanShaderModule gradientShader =
            VulkanShaderModule::fromFile(ctx.vkDevice(), shaderDir / "gradient.comp.spv");

        VulkanDescriptorSetLayoutCache layoutCache(ctx.vkDevice());
        VulkanComputePipeline pipeline(ctx.vkDevice(), layoutCache, gradientShader);

        {
            const auto [initW, initH] = window.framebufferExtent();

            VulkanSwapchain swapchain({ .physicalDevice = ctx.physicalDevice(),
                                        .device = ctx.vkDevice(),
                                        .surface = surface,
                                        .graphicsQueueFamily = ctx.graphicsQueue().queueFamily(),
                                        .width = initW,
                                        .height = initH,
                                        .vsync = true });

            VulkanCommandPool<queue_kind::Graphics> cmdPool(ctx.vkDevice(), ctx.graphicsQueue().queueFamily());

            VulkanDescriptorPool descPool(ctx.vkDevice(), {});

            std::optional<VulkanRenderTarget> renderTarget;
            VkDescriptorSet descSet = VK_NULL_HANDLE;

            const auto rebuildRenderTarget = [&](uint32_t w, uint32_t h) {
                renderTarget.emplace(
                    allocator, VulkanRenderTargetCreateInfo{ .width = w,
                                                             .height = h,
                                                             .format = ImageFormat::RGBA8_UNORM,
                                                             .usage = ImageUsage::Storage | ImageUsage::TransferSrc });
                descPool.reset();
                descSet = descPool.allocate(pipeline.descriptorSetLayouts()[0]);
                VulkanDescriptorWriter writer(ctx.vkDevice());
                writer.writeStorageImage(descSet, 0, *renderTarget, ImageLayout::General);
                writer.commit();
            };

            std::vector<VulkanSemaphoreHandle> renderDoneByImage;
            const auto rebuildPerImageSemaphores = [&]() {
                renderDoneByImage.clear();
                renderDoneByImage.reserve(swapchain.imageCount());
                for (uint32_t i = 0; i < swapchain.imageCount(); ++i) {
                    renderDoneByImage.push_back(ctx.semaphores().acquire());
                }
            };

            rebuildRenderTarget(initW, initH);
            rebuildPerImageSemaphores();

            std::optional<VulkanPendingSubmission<queue_kind::Graphics>> inFlight;

            while (!window.shouldClose()) {
                window.pollEvents();

                const auto [w, h] = window.framebufferExtent();
                if (w == 0 || h == 0) {
                    continue;
                }

                if (window.wasResized()) {
                    if (inFlight.has_value()) {
                        std::move(*inFlight).wait();
                        inFlight.reset();
                    }
                    vkDeviceWaitIdle(ctx.vkDevice());
                    swapchain.recreate(w, h);
                    rebuildRenderTarget(w, h);
                    rebuildPerImageSemaphores();
                    window.clearResized();
                    continue;
                }

                if (inFlight.has_value()) {
                    const VulkanCommandBuffer<queue_kind::Graphics> recycled = std::move(*inFlight).wait();
                    inFlight.reset();
                    (void)recycled;
                }

                const VulkanSemaphoreHandle imageReady = ctx.semaphores().acquire();

                const VulkanSwapchainAcquire acq = swapchain.acquireNextImage(imageReady);
                if (acq.outOfDate) {
                    vkDeviceWaitIdle(ctx.vkDevice());
                    swapchain.recreate(w, h);
                    rebuildRenderTarget(w, h);
                    rebuildPerImageSemaphores();
                    continue;
                }

                const VulkanSemaphoreHandle &renderDone = renderDoneByImage[acq.imageIndex];

                VulkanCommandBuffer<queue_kind::Graphics> cmd = cmdPool.allocate();
                VulkanRecorder<queue_kind::Graphics> rec = std::move(cmd).begin();

                VulkanRenderTarget &rt = *renderTarget;
                VkImage targetImage = rt.image().handle();
                VkImage swapImage = swapchain.image(acq.imageIndex);
                const uint32_t rtW = rt.width();
                const uint32_t rtH = rt.height();

                const std::array<VulkanImageBarrier, 1> toGeneral{ { { .image = targetImage,
                                                                       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                       .oldLayout = ImageLayout::Undefined,
                                                                       .newLayout = ImageLayout::General,
                                                                       .srcStage = PipelineStage::TopOfPipe,
                                                                       .dstStage = PipelineStage::ComputeShader,
                                                                       .srcAccess = AccessFlags::None,
                                                                       .dstAccess = AccessFlags::ShaderWrite } } };
                rec.pipelineBarrier(toGeneral);

                const std::array<VkDescriptorSet, 1> sets{ descSet };
                const auto [gx, gy, gz] = pipeline.groupCountsFor(rtW, rtH);
                pipeline.execute(
                    rec, PipelineExecuteContext{ .descriptorSets = sets, .groupsX = gx, .groupsY = gy, .groupsZ = gz });

                const std::array<VulkanImageBarrier, 2> toBlit{ { { .image = targetImage,
                                                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .oldLayout = ImageLayout::General,
                                                                    .newLayout = ImageLayout::TransferSrcOptimal,
                                                                    .srcStage = PipelineStage::ComputeShader,
                                                                    .dstStage = PipelineStage::Transfer,
                                                                    .srcAccess = AccessFlags::ShaderWrite,
                                                                    .dstAccess = AccessFlags::TransferRead },
                                                                  { .image = swapImage,
                                                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .oldLayout = ImageLayout::Undefined,
                                                                    .newLayout = ImageLayout::TransferDstOptimal,
                                                                    .srcStage = PipelineStage::TopOfPipe,
                                                                    .dstStage = PipelineStage::Transfer,
                                                                    .srcAccess = AccessFlags::None,
                                                                    .dstAccess = AccessFlags::TransferWrite } } };
                rec.pipelineBarrier(toBlit);

                rec.blitImage(targetImage, ImageLayout::TransferSrcOptimal, rtW, rtH, swapImage,
                              ImageLayout::TransferDstOptimal, w, h);

                const std::array<VulkanImageBarrier, 1> toPresent{ { { .image = swapImage,
                                                                       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                       .oldLayout = ImageLayout::TransferDstOptimal,
                                                                       .newLayout = ImageLayout::PresentSrc,
                                                                       .srcStage = PipelineStage::Transfer,
                                                                       .dstStage = PipelineStage::BottomOfPipe,
                                                                       .srcAccess = AccessFlags::TransferWrite,
                                                                       .dstAccess = AccessFlags::None } } };
                rec.pipelineBarrier(toPresent);

                VulkanExecutable<queue_kind::Graphics> exe = std::move(rec).end();

                const std::array<VulkanSemaphoreWait, 1> waits{ { { .semaphore = imageReady,
                                                                    .stage = PipelineStage::Transfer } } };
                const std::array<VulkanSemaphoreHandle, 1> signals{ renderDone };

                inFlight.emplace(
                    ctx.graphicsQueue().submit(std::move(exe), VulkanSubmitInfo{ .wait = waits, .signal = signals }));

                const bool presentOutOfDate = swapchain.present(ctx.graphicsQueue(), renderDone, acq.imageIndex);
                if (presentOutOfDate) {
                    if (inFlight.has_value()) {
                        std::move(*inFlight).wait();
                        inFlight.reset();
                    }
                    vkDeviceWaitIdle(ctx.vkDevice());
                    swapchain.recreate(w, h);
                    rebuildRenderTarget(w, h);
                    rebuildPerImageSemaphores();
                }
            }

            vkDeviceWaitIdle(ctx.vkDevice());

            if (inFlight.has_value()) {
                std::move(*inFlight).wait();
                inFlight.reset();
            }
        }
    } catch (const std::exception &e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());

        return 1;
    }

    return 0;
}
