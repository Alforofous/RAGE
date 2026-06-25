#include "renderer.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <utility>
#include "engine/rendering/frame_context.hpp"
#include "engine/rendering/frame_uniforms.hpp"
#include "engine/rendering/pixel_debug.hpp"
#include "engine/rendering/scene_casters.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/directional_light.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/voxel3d.hpp"
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

    void Renderer::addLight(std::shared_ptr<DirectionalLight> light) { directionalLights_.push_back(std::move(light)); }

    void Renderer::setPickTarget(int32_t pixelX, int32_t pixelY) {
        pendingPickX_ = pixelX;
        pendingPickY_ = pixelY;
    }

    bool Renderer::tryReadPick(PixelDebug &out) {
        if (!pickResultReady_) {
            return false;
        }
        out = pickResult_;
        pickResultReady_ = false;
        return true;
    }

    void Renderer::rebuildFrameResources(FrameExtent extent) {
        renderTarget_.emplace(
            allocator_,
            VulkanRenderTargetCreateInfo{ .width = extent.width,
                                          .height = extent.height,
                                          .format = ImageFormat::RGBA8_UNORM,
                                          .usage = ImageUsage::Storage | ImageUsage::TransferSrc | ImageUsage::TransferDst });

        if (!frameUniformBuffer_.has_value()) {
            frameUniformBuffer_.emplace(allocator_.createBuffer({
                .size = sizeof(FrameUniforms),
                .usage = BufferUsage::Uniform,
                .memory = MemoryLocation::CpuToGpu,
            }));
        }

        if (!sceneCastersBuffer_.has_value()) {
            sceneCastersBuffer_.emplace(allocator_.createBuffer({
                .size = sizeof(SceneCasters),
                .usage = BufferUsage::Uniform,
                .memory = MemoryLocation::CpuToGpu,
            }));
        }

        if (!pixelDebugBuffer_.has_value()) {
            pixelDebugBuffer_.emplace(allocator_.createBuffer({
                .size = sizeof(PixelDebug),
                .usage = BufferUsage::Storage,
                .memory = MemoryLocation::CpuToGpu,
            }));
            std::memset(pixelDebugBuffer_->mappedData(), 0, sizeof(PixelDebug));
        }

        if (!thumbnailTarget_.has_value()) {
            thumbnailTarget_.emplace(
                allocator_,
                VulkanRenderTargetCreateInfo{ .width = 320,
                                              .height = 180,
                                              .format = ImageFormat::RGBA8_UNORM,
                                              .usage = ImageUsage::TransferSrc | ImageUsage::TransferDst });
        }
        if (!thumbnailStaging_.has_value()) {
            constexpr uint64_t kThumbnailBytes = static_cast<uint64_t>(320) * 180 * 4;
            thumbnailStaging_.emplace(allocator_.createBuffer({
                .size = kThumbnailBytes,
                .usage = BufferUsage::TransferDst,
                .memory = MemoryLocation::CpuToGpu,
            }));
        }

        renderDoneByImage_.clear();
        renderDoneByImage_.reserve(swapchain_.imageCount());
        for (uint32_t i = 0; i < swapchain_.imageCount(); ++i) {
            renderDoneByImage_.push_back(ctx_.semaphores().acquire());
        }
    }

    void Renderer::collectShadowCasters(Node3D &node) {
        if (shadowCasters_.size() >= kMaxSceneCasters) {
            return;
        }
        auto *v = dynamic_cast<Voxel3D *>(&node);
        if (v != nullptr && v->voxelBuffer() != nullptr) {
            shadowCasters_.push_back(v);
        }
        for (const auto &child : node.children()) {
            collectShadowCasters(*child);
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

        if (thumbnailQueued_ && frameImage_ && thumbnailStaging_.has_value()) {
            const auto width = static_cast<uint16_t>(thumbnailTarget_->width());
            const auto height = static_cast<uint16_t>(thumbnailTarget_->height());
            frameImage_(thumbnailStaging_->mappedData(), width, height);
            thumbnailQueued_ = false;
        }

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

        shadowCasters_.clear();
        collectShadowCasters(root);

        {
            SceneCasters scene{};
            scene.count = static_cast<int32_t>(shadowCasters_.size());
            for (size_t i = 0; i < shadowCasters_.size(); ++i) {
                Voxel3D *v = shadowCasters_[i];
                SceneCasterEntry &e = scene.entries[i];
                e.invModel = v->worldMatrix().inverted();
                const IVec3 dims = v->dimensions();
                e.dimsX = dims.x;
                e.dimsY = dims.y;
                e.dimsZ = dims.z;
                e.voxelSize = v->voxelSize();
            }
            std::memcpy(sceneCastersBuffer_->mappedData(), &scene, sizeof(scene));
        }

        {
            FrameUniforms uniforms{};
            const Mat4 camWorld = camera.worldMatrix();
            const Vec3 cameraPos = camWorld.transformPoint(Vec3::zero());
            const Vec3 cameraForward = camWorld.transformDirection(Vec3(0.0f, 0.0f, -1.0f));
            const Vec3 cameraUp = camWorld.transformDirection(Vec3::unitY());
            uniforms.cameraPos_fovY = Vec4(cameraPos, camera.fov());
            uniforms.cameraForward_aspect = Vec4(cameraForward, camera.aspect());
            uniforms.cameraUp_unused = Vec4(cameraUp, 0.0f);

            uniforms.ambientColor_intensity =
                Vec4(ambient_.color.r, ambient_.color.g, ambient_.color.b, ambient_.intensity);

            const int32_t lightCount = std::min(static_cast<int32_t>(directionalLights_.size()),
                                                static_cast<int32_t>(kMaxDirectionalLights));
            uniforms.dirLightCount = lightCount;
            for (int32_t i = 0; i < lightCount; ++i) {
                const DirectionalLight &light = *directionalLights_[i];
                const Vec3 dir = light.worldDirection();
                uniforms.dirLightDir_intensity[i] = Vec4(dir, light.intensity());
                const Color c = light.color();
                uniforms.dirLightColor[i] = Vec4(c.r, c.g, c.b, 0.0f);
            }

            uniforms.debugPixelX = pendingPickX_;
            uniforms.debugPixelY = pendingPickY_;

            std::memcpy(frameUniformBuffer_->mappedData(), &uniforms, sizeof(uniforms));
        }

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

        const bool pickRequested = (pendingPickX_ >= 0 && pendingPickY_ >= 0);
        if (pickRequested) {
            std::move(*inFlight_).wait();
            inFlight_.reset();
            std::memcpy(&pickResult_, pixelDebugBuffer_->mappedData(), sizeof(PixelDebug));
            pickResultReady_ = true;
            pendingPickX_ = -1;
            pendingPickY_ = -1;
        }

        const bool presentOutOfDate = swapchain_.present(ctx_.graphicsQueue(), renderDone, acq.imageIndex);
        if (presentOutOfDate) {
            needsRecreate_ = true;
        }

        if (frameEnd_) {
            frameEnd_();
        }
    }

    VulkanRenderTarget &Renderer::resolveTarget(const Pass &pass) {
        return (pass.target != nullptr) ? *pass.target : *renderTarget_;
    }

    void Renderer::recordPass(VulkanRecorder<queue_kind::Graphics> &rec, VulkanRenderTarget &target,
                              std::span<Renderable *const> /*renderables*/, const FrameContext & /*frame*/,
                              bool /*isFirstPass*/, bool /*isLastPass*/) {
        VkImage targetImage = target.image().handle();
        const uint32_t tgtW = target.width();
        const uint32_t tgtH = target.height();

        const std::array<VulkanImageBarrier, 1> toGeneral{ { { .image = targetImage,
                                                               .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                               .oldLayout = ImageLayout::Undefined,
                                                               .newLayout = ImageLayout::General,
                                                               .srcStage = PipelineStage::TopOfPipe,
                                                               .dstStage = PipelineStage::Transfer,
                                                               .srcAccess = AccessFlags::None,
                                                               .dstAccess = AccessFlags::TransferWrite } } };
        rec.pipelineBarrier(toGeneral);

        rec.clearColorImage(targetImage, ImageLayout::General, 0.02f, 0.03f, 0.06f, 1.0f);

        const std::array<VulkanImageBarrier, 1> clearToCompute{ { { .image = targetImage,
                                                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .oldLayout = ImageLayout::General,
                                                                    .newLayout = ImageLayout::General,
                                                                    .srcStage = PipelineStage::Transfer,
                                                                    .dstStage = PipelineStage::ComputeShader,
                                                                    .srcAccess = AccessFlags::TransferWrite,
                                                                    .dstAccess = AccessFlags::ShaderWrite } } };
        rec.pipelineBarrier(clearToCompute);

        if (shadowCasters_.empty()) {
            return;
        }

        Voxel3D *anyCaster = shadowCasters_[0];
        if (!anyCaster->hasMaterial()) {
            return;
        }
        VulkanPipeline &pipeline = pipelineCache_.getOrCreate(*anyCaster->material());

        VkDescriptorSet set = descPool_.allocate(pipeline.descriptorSetLayouts()[0]);
        VulkanDescriptorWriter writer(ctx_.vkDevice());
        writer.writeStorageImage(set, 0, target, ImageLayout::General);
        writer.writeUniformBuffer(set, 2, *frameUniformBuffer_);
        writer.writeUniformBuffer(set, 3, *sceneCastersBuffer_);
        writer.writeStorageBuffer(set, 5, *pixelDebugBuffer_);

        const VulkanBuffer *fallbackBuffer = shadowCasters_[0]->voxelBuffer();
        for (uint32_t slot = 0; slot < kMaxSceneCasters; ++slot) {
            const VulkanBuffer *buf = (slot < shadowCasters_.size())
                                          ? shadowCasters_[slot]->voxelBuffer()
                                          : fallbackBuffer;
            writer.writeStorageBufferArray(set, 4, slot, *buf);
        }

        writer.commit();

        const std::array<VkDescriptorSet, 1> sets{ set };
        const auto wg = anyCaster->material()->shader()->localWorkgroupSize();
        const uint32_t gx = (wg[0] > 0) ? ((tgtW + wg[0] - 1) / wg[0]) : 1;
        const uint32_t gy = (wg[1] > 0) ? ((tgtH + wg[1] - 1) / wg[1]) : 1;

        if (beforeGpuPass_) {
            beforeGpuPass_("voxel_raycast", rec.rawHandle());
        }
        pipeline.execute(rec, PipelineExecuteContext{ .descriptorSets = sets,
                                                      .pushConstants = {},
                                                      .groupsX = gx,
                                                      .groupsY = gy,
                                                      .groupsZ = 1 });
        if (afterGpuPass_) {
            afterGpuPass_("voxel_raycast", rec.rawHandle());
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

        if (frameImage_ && thumbnailTarget_.has_value() && thumbnailStaging_.has_value()) {
            VkImage thumbImage = thumbnailTarget_->image().handle();
            const std::array<VulkanImageBarrier, 1> thumbToDst{ { { .image = thumbImage,
                                                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .oldLayout = ImageLayout::Undefined,
                                                                    .newLayout = ImageLayout::TransferDstOptimal,
                                                                    .srcStage = PipelineStage::TopOfPipe,
                                                                    .dstStage = PipelineStage::Transfer,
                                                                    .srcAccess = AccessFlags::None,
                                                                    .dstAccess = AccessFlags::TransferWrite } } };
            rec.pipelineBarrier(thumbToDst);

            rec.blitImage(mainTargetImage, ImageLayout::TransferSrcOptimal, rtW, rtH, thumbImage,
                          ImageLayout::TransferDstOptimal, thumbnailTarget_->width(), thumbnailTarget_->height());

            const std::array<VulkanImageBarrier, 1> thumbToSrc{ { { .image = thumbImage,
                                                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .oldLayout = ImageLayout::TransferDstOptimal,
                                                                    .newLayout = ImageLayout::TransferSrcOptimal,
                                                                    .srcStage = PipelineStage::Transfer,
                                                                    .dstStage = PipelineStage::Transfer,
                                                                    .srcAccess = AccessFlags::TransferWrite,
                                                                    .dstAccess = AccessFlags::TransferRead } } };
            rec.pipelineBarrier(thumbToSrc);

            const std::array<BufferImageCopy, 1> regions{ { { .bufferOffset = 0,
                                                              .bufferRowLength = 0,
                                                              .bufferImageHeight = 0,
                                                              .mipLevel = 0,
                                                              .baseArrayLayer = 0,
                                                              .layerCount = 1,
                                                              .imageOffsetX = 0,
                                                              .imageOffsetY = 0,
                                                              .imageOffsetZ = 0,
                                                              .imageWidth = thumbnailTarget_->width(),
                                                              .imageHeight = thumbnailTarget_->height(),
                                                              .imageDepth = 1 } } };
            rec.copyImageToBuffer(thumbImage, ImageLayout::TransferSrcOptimal, *thumbnailStaging_, regions);

            thumbnailQueued_ = true;
        }

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
