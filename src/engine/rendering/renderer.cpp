#include "renderer.hpp"
#include <array>
#include <exception>
#include <utility>
#include "engine/rendering/frame_context.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/node3d.hpp"
#include "gpu/gpu_types.hpp"
#include "gpu/vulkan/vulkan_descriptor_writer.hpp"
#include "gpu/vulkan/vulkan_pipeline.hpp"
#include "gpu/vulkan/vulkan_queue.hpp"
#include "shared/logger.hpp"

namespace RAGE {
    Renderer::Renderer(VulkanContext &ctx, VulkanAllocator &allocator, VulkanSwapchain &swapchain)
        : ctx_(ctx)
        , allocator_(allocator)
        , swapchain_(swapchain)
        , cmdPool_(ctx.vkDevice(), ctx.graphicsQueue().queueFamily())
        , descPool_(ctx.vkDevice(), {})
        , pipelineCache_(ctx.vkDevice()) {}

    Renderer::~Renderer() {
        try {
            drainInFlight();
        } catch (const std::exception &e) {
            Core::log(Core::LogLevel::Error, e.what());
        }
        vkDeviceWaitIdle(ctx_.vkDevice());
    }

    void Renderer::drainInFlight() {
        if (inFlight_.has_value()) {
            std::move(*inFlight_).wait();
            inFlight_.reset();
        }
    }

    void Renderer::rebuildFrameResources(FrameExtent extent) {
        renderTarget_.emplace(allocator_,
                              VulkanRenderTargetCreateInfo{ .width = extent.width,
                                                            .height = extent.height,
                                                            .format = ImageFormat::RGBA8_UNORM,
                                                            .usage = ImageUsage::Storage | ImageUsage::TransferSrc });

        renderDoneByImage_.clear();
        renderDoneByImage_.reserve(swapchain_.imageCount());
        for (uint32_t i = 0; i < swapchain_.imageCount(); ++i) {
            renderDoneByImage_.push_back(ctx_.semaphores().acquire());
        }
    }

    void Renderer::collectVisible(Node3D &node, std::vector<Renderable *> &out) {
        auto *r = dynamic_cast<Renderable *>(&node);
        if (r != nullptr && r->visible() && r->hasMaterial()) {
            out.push_back(r);
        }
        for (const auto &child : node.children()) {
            collectVisible(*child, out);
        }
    }

    void Renderer::render(Node3D &root, const Camera &camera, FrameExtent extent) {
        if (extent.width == 0 || extent.height == 0) {
            return;
        }

        const bool extentChanged = (extent != lastExtent_) && (lastExtent_.width != 0);
        if (needsRecreate_ || extentChanged) {
            drainInFlight();
            vkDeviceWaitIdle(ctx_.vkDevice());
            if (lastExtent_.width != 0) {
                swapchain_.recreate(extent.width, extent.height);
            }
            rebuildFrameResources(extent);
            lastExtent_ = extent;
            needsRecreate_ = false;
        }

        drainInFlight();

        descPool_.reset();

        const VulkanSemaphoreHandle imageReady = ctx_.semaphores().acquire();
        const VulkanSwapchainAcquire acq = swapchain_.acquireNextImage(imageReady);
        if (acq.outOfDate) {
            needsRecreate_ = true;
            return;
        }

        const VulkanSemaphoreHandle &renderDone = renderDoneByImage_[acq.imageIndex];

        std::vector<Renderable *> visible;
        collectVisible(root, visible);

        VulkanCommandBuffer<queue_kind::Graphics> cmd = cmdPool_.allocate();
        VulkanRecorder<queue_kind::Graphics> rec = std::move(cmd).begin();
        const auto [swW, swH] = swapchain_.extent();
        const FrameContext frameCtx{ .camera = &camera };
        recordFrame(rec, swapchain_.image(acq.imageIndex), swW, swH, visible, frameCtx);
        VulkanExecutable<queue_kind::Graphics> exe = std::move(rec).end();

        const std::array<VulkanSemaphoreWait, 1> waits{ { { .semaphore = imageReady,
                                                            .stage = PipelineStage::Transfer } } };
        const std::array<VulkanSemaphoreHandle, 1> signals{ renderDone };

        inFlight_.emplace(
            ctx_.graphicsQueue().submit(std::move(exe), VulkanSubmitInfo{ .wait = waits, .signal = signals }));

        const bool presentOutOfDate = swapchain_.present(ctx_.graphicsQueue(), renderDone, acq.imageIndex);
        if (presentOutOfDate) {
            needsRecreate_ = true;
        }
    }

    VulkanRenderTarget &Renderer::resolveTarget(const Pass &pass) {
        return (pass.target != nullptr) ? *pass.target : *renderTarget_;
    }

    void Renderer::recordPass(VulkanRecorder<queue_kind::Graphics> &rec, VulkanRenderTarget &target,
                              std::span<Renderable *const> renderables, const FrameContext &frame, bool /*isFirstPass*/,
                              bool /*isLastPass*/) {
        VkImage targetImage = target.image().handle();
        const uint32_t tgtW = target.width();
        const uint32_t tgtH = target.height();

        const std::array<VulkanImageBarrier, 1> toGeneral{ { { .image = targetImage,
                                                               .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                               .oldLayout = ImageLayout::Undefined,
                                                               .newLayout = ImageLayout::General,
                                                               .srcStage = PipelineStage::TopOfPipe,
                                                               .dstStage = PipelineStage::ComputeShader,
                                                               .srcAccess = AccessFlags::None,
                                                               .dstAccess = AccessFlags::ShaderWrite } } };
        rec.pipelineBarrier(toGeneral);

        for (size_t i = 0; i < renderables.size(); ++i) {
            Renderable *r = renderables[i];
            VulkanPipeline &pipeline = pipelineCache_.getOrCreate(*r->material());

            VkDescriptorSet set = descPool_.allocate(pipeline.descriptorSetLayouts()[0]);
            VulkanDescriptorWriter writer(ctx_.vkDevice());
            writer.writeStorageImage(set, 0, target, ImageLayout::General);

            PushConstantBuilder pcBuilder;
            r->prepareFrame(writer, set, pcBuilder, frame);

            writer.commit();

            const std::array<VkDescriptorSet, 1> sets{ set };
            const auto wg = r->material()->shader()->localWorkgroupSize();
            const uint32_t gx = (wg[0] > 0) ? ((tgtW + wg[0] - 1) / wg[0]) : 1;
            const uint32_t gy = (wg[1] > 0) ? ((tgtH + wg[1] - 1) / wg[1]) : 1;

            pipeline.execute(rec, PipelineExecuteContext{ .descriptorSets = sets,
                                                          .pushConstants = pcBuilder.bytes(),
                                                          .groupsX = gx,
                                                          .groupsY = gy,
                                                          .groupsZ = 1 });

            if (i + 1 < renderables.size()) {
                const std::array<VulkanImageBarrier, 1> between{ { { .image = targetImage,
                                                                     .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                     .oldLayout = ImageLayout::General,
                                                                     .newLayout = ImageLayout::General,
                                                                     .srcStage = PipelineStage::ComputeShader,
                                                                     .dstStage = PipelineStage::ComputeShader,
                                                                     .srcAccess = AccessFlags::ShaderWrite,
                                                                     .dstAccess = AccessFlags::ShaderWrite } } };
                rec.pipelineBarrier(between);
            }
        }
    }

    void Renderer::recordFrame(VulkanRecorder<queue_kind::Graphics> &rec, VkImage swapImage, uint32_t swapW,
                               uint32_t swapH, std::span<Renderable *const> renderables, const FrameContext &frame) {
        recordPass(rec, *renderTarget_, renderables, frame, true, true);

        VulkanRenderTarget &mainTarget = *renderTarget_;
        VkImage mainTargetImage = mainTarget.image().handle();
        const uint32_t rtW = mainTarget.width();
        const uint32_t rtH = mainTarget.height();

        const std::array<VulkanImageBarrier, 2> toBlit{ { { .image = mainTargetImage,
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

        rec.blitImage(mainTargetImage, ImageLayout::TransferSrcOptimal, rtW, rtH, swapImage,
                      ImageLayout::TransferDstOptimal, swapW, swapH);

        const std::array<VulkanImageBarrier, 1> toPresent{ { { .image = swapImage,
                                                               .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                               .oldLayout = ImageLayout::TransferDstOptimal,
                                                               .newLayout = ImageLayout::PresentSrc,
                                                               .srcStage = PipelineStage::Transfer,
                                                               .dstStage = PipelineStage::BottomOfPipe,
                                                               .srcAccess = AccessFlags::TransferWrite,
                                                               .dstAccess = AccessFlags::None } } };
        rec.pipelineBarrier(toPresent);

        (void)passes_;
    }
}
