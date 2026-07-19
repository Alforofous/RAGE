#include "renderer.hpp"
#include "shared/profiling.hpp"
#include <array>
#include <cstring>
#include <exception>
#include <utility>
#include "engine/rendering/frame_context.hpp"
#include "engine/rendering/frame_uniforms.hpp"
#include "engine/rendering/pixel_debug.hpp"
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
    namespace {
        // RAII bracket for the engine's phase callbacks: fires `begin(name)` on construction
        // and `end(name)` on destruction. Lets any return path inside render() leave the
        // phase balanced without each branch having to remember to call end. Engine has no
        // profiler library knowledge — this just routes the engine's own callbacks.

    }

    Renderer::Renderer(VulkanContext &ctx, VulkanAllocator &allocator, VulkanSwapchain &swapchain,
                       WorldLimits limits)
        : ctx_(ctx)
        , allocator_(allocator)
        , swapchain_(swapchain)
        , limits_(limits)
        , cmdPool_(ctx.vkDevice(), ctx.graphicsQueue().queueFamily())
        , descPool_(ctx.vkDevice(), {})
        , pipelineCache_(ctx.vkDevice())
        , svdagCache_(allocator)
        , worldGridSync_(allocator, ctx.vkDevice(), limits.worldGridDims)
        , freeVolumeSync_(allocator, limits.maxFreeVolumes, limits.maxFreeVolumeHandleCells) {
        brickPool_.emplace(limits_.brickPool);
    }

    void Renderer::recreateBrickPool(bool enableDedup) {
        drainInFlight();
        vkDeviceWaitIdle(ctx_.vkDevice());
        brickPool_.reset();
        BrickPoolConfig cfg = limits_.brickPool;
        cfg.enableDedup = enableDedup;
        brickPool_.emplace(cfg);
    }

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

        if (!brickPoolBuffer_.has_value()) {
            const uint64_t kBrickPoolBytes =
                static_cast<uint64_t>(limits_.brickPool.maxBricks) * sizeof(Brick);
            brickPoolBuffer_.emplace(allocator_.createBuffer({
                .size = kBrickPoolBytes,
                .usage = BufferUsage::Storage,
                .memory = MemoryLocation::CpuToGpu,
            }));
            std::memset(brickPoolBuffer_->mappedData(), 0, kBrickPoolBytes);
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

        if (swapchainRebuilt_) {
            swapchainImageCache_.clear();
            swapchainImageCache_.reserve(swapchain_.imageCount());
            for (uint32_t i = 0; i < swapchain_.imageCount(); ++i) {
                swapchainImageCache_.push_back(swapchain_.image(i));
            }
            const auto [sw, sh] = swapchain_.extent();
            swapchainRebuilt_(SwapchainInfo{ .device = ctx_.vkDevice(),
                                             .physicalDevice = ctx_.physicalDevice(),
                                             .instance = ctx_.vkInstance(),
                                             .graphicsQueue = ctx_.graphicsQueue().vkQueue(),
                                             .graphicsQueueFamily = ctx_.graphicsQueue().queueFamily(),
                                             .colorFormat = swapchain_.vkFormat(),
                                             .imageCount = swapchain_.imageCount(),
                                             .images = swapchainImageCache_.data(),
                                             .width = sw,
                                             .height = sh });
        }
    }

    void Renderer::onSwapchainRebuilt(SwapchainRebuiltHook h) {
        swapchainRebuilt_ = std::move(h);
        if (swapchainRebuilt_ && !needsRecreate_ && swapchain_.imageCount() > 0) {
            swapchainImageCache_.clear();
            swapchainImageCache_.reserve(swapchain_.imageCount());
            for (uint32_t i = 0; i < swapchain_.imageCount(); ++i) {
                swapchainImageCache_.push_back(swapchain_.image(i));
            }
            const auto [sw, sh] = swapchain_.extent();
            swapchainRebuilt_(SwapchainInfo{ .device = ctx_.vkDevice(),
                                             .physicalDevice = ctx_.physicalDevice(),
                                             .instance = ctx_.vkInstance(),
                                             .graphicsQueue = ctx_.graphicsQueue().vkQueue(),
                                             .graphicsQueueFamily = ctx_.graphicsQueue().queueFamily(),
                                             .colorFormat = swapchain_.vkFormat(),
                                             .imageCount = swapchain_.imageCount(),
                                             .images = swapchainImageCache_.data(),
                                             .width = sw,
                                             .height = sh });
        }
    }

    void Renderer::collectVolumes(Node3D &node) {
        // M3+M4: no fixed cap. World brick grid scales with the union of placements; the
        // brick pool deduplicates identical content. Previously had a `kMaxSceneCasters`
        // ceiling of 16 from the pre-M3 SceneCasters UBO — gone with that descriptor.
        auto *v = node.asVoxel3D();
        if (v != nullptr && v->voxelData() != nullptr) {
            if (v->voxelData()->isWindowed()) {
                // The windowed volume IS the world grid; at most one is supported.
                if (worldVolume_ != nullptr && worldVolume_ != v) {
                    throw std::logic_error("Renderer: multiple windowed volumes in one scene");
                }
                worldVolume_ = v;
            } else {
                // Every dense volume traces per-object with its full transform.
                freeVolumes_.push_back(v);
            }
        }
        for (const auto &child : node.children()) {
            collectVolumes(*child);
        }
    }

    void Renderer::render(Node3D &root, const Camera &camera, FrameExtent extent) {
        if (extent.width == 0 || extent.height == 0) {
            return;
        }
        const Core::ProfileZone renderScope("Renderer::render");

        const bool extentChanged = (extent != lastExtent_) && (lastExtent_.width != 0);
        if (needsRecreate_ || extentChanged) {
            const Core::ProfileZone rebuild("Render.RebuildFrameResources");
            drainInFlight();
            vkDeviceWaitIdle(ctx_.vkDevice());
            if (lastExtent_.width != 0) {
                swapchain_.recreate(extent.width, extent.height);
            }
            rebuildFrameResources(extent);
            lastExtent_ = extent;
            needsRecreate_ = false;
        }

        {
            const Core::ProfileZone drain("Render.DrainInFlight");
            drainInFlight();
        }

        if (thumbnailQueued_ && Core::profileFrameImageWanted() && thumbnailStaging_.has_value()) {
            const auto width = static_cast<uint16_t>(thumbnailTarget_->width());
            const auto height = static_cast<uint16_t>(thumbnailTarget_->height());
            Core::profileFrameImage(thumbnailStaging_->mappedData(), width, height);
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

        // Dirty-brick upload doubles as the content-change signal: brick allocation marks
        // dirty, and a fresh brick can change the chunk handle grids the world grid indexes.
        std::vector<BrickHandle> dirtyBricks;
        {
            const Core::ProfileZone brickUpload("Render.DirtyBrickUpload");
            auto *poolDst = static_cast<Brick *>(brickPoolBuffer_->mappedData());
            dirtyBricks = brickPool_->drainDirty();
            for (const BrickHandle h : dirtyBricks) {
                poolDst[h.id] = brickPool_->brick(h);
            }
        }

        const bool gridTextureJustEnabled = useGridTexture_ && !prevUseGridTexture_;
        prevUseGridTexture_ = useGridTexture_;
        const bool svdagJustEnabled = useSvdag_ && !prevUseSvdag_;
        prevUseSvdag_ = useSvdag_;
        const uint64_t sceneVersion = root.treeVersion();
        const bool sceneChanged = sceneVersion != lastSceneTreeVersion_ || !dirtyBricks.empty()
                                  || gridTextureJustEnabled || svdagJustEnabled;
        lastSceneTreeVersion_ = sceneVersion;
        if (gridTextureJustEnabled || svdagJustEnabled) {
            // These paths read GPU-side copies of the grid that only refresh in the
            // upload block — force it even when no chunk was patched this frame.
            worldGridGpuDirty_ = true;
        }

        if (sceneChanged) {
            const Core::ProfileZone collect("Render.Collect");
            freeVolumes_.clear();
            worldVolume_ = nullptr;
            collectVolumes(root);
        }

        // Adopt-on-render (api-north-star N8): pool-less volumes join the shared
        // pool the first frame they are visible and not mid-bulk-load.
        {
            const auto adoptIfReady = [this](Voxel3D *v) {
                VoxelData *data = v->voxelData();
                if (!data->isAdopted() && data->isAdoptable()) {
                    data->adoptInto(*brickPool_);
                }
            };
            if (worldVolume_ != nullptr) {
                adoptIfReady(worldVolume_);
            }
            for (Voxel3D *v : freeVolumes_) {
                adoptIfReady(v);
            }
        }

        if (worldVolume_ != nullptr && worldVolume_->voxelData()->isAdopted()
            && worldVolume_->voxelData()->version() != lastWorldVolumeVersion_) {
            lastWorldVolumeVersion_ = worldVolume_->voxelData()->version();
            worldGridGpuDirty_ = true;
        }

        if (worldGridGpuDirty_ && worldVolume_ != nullptr) {
            worldGridGpuDirty_ = false;
            const Core::ProfileZone gridUpload("Render.WorldGridUpload");
            const float brickWorldSize =
                worldVolume_->voxelSize() * static_cast<float>(Brick::kDim);
            worldGridSync_.upload(gridView(*worldVolume_->voxelData()), brickWorldSize,
                                  useGridTexture_);
            svdagFresh_ = false;
            gridQuietFrames_ = 0;
            ++gridChangeStamp_;
        } else {
            ++gridQuietFrames_;
        }

        // SVDAG builds are debounced AND asynchronous: kicked off only once the grid
        // settles, built on a worker thread from a wrapped-storage snapshot, adopted
        // only if the grid hasn't changed since. While stale, frames render through
        // the flat path (uniforms.useSvdag gates on freshness) — no churn-time hitch.
        if (svdagBuildDone_.load()) {
            svdagBuildDone_.store(false);
            svdagBuildRunning_.store(false);
            if (svdagBuildStamp_ == gridChangeStamp_ && useSvdag_) {
                const auto result = svdagCache_.uploadBuilt(std::move(svdagBuildResult_),
                                                            svdagBuildOrigin_,
                                                            svdagBuildBrickSize_);
                if (result.capacityExceeded) {
                    Core::log(Core::LogLevel::Warn,
                              "SVDAG node count exceeds GPU buffer capacity — falling back to flat grid");
                }
                svdagFresh_ = true;
            }
        }
        if (useSvdag_ && !svdagFresh_ && !svdagBuildRunning_.load() && worldVolume_ != nullptr
            && (gridQuietFrames_ >= kSvdagQuietFrames || svdagJustEnabled)) {
            const Core::ProfileZone svdagKick("Render.SvdagKickoff");
            const float brickWorldSize =
                worldVolume_->voxelSize() * static_cast<float>(Brick::kDim);
            const WorldGridView view = gridView(*worldVolume_->voxelData());
            const IVec3 origBrick = view.windowMinBrick;
            svdagBuildStamp_ = gridChangeStamp_;
            svdagBuildStorage_.assign(view.handles.begin(), view.handles.end());
            svdagBuildFixedDims_ = view.storageDims;
            svdagBuildWinMin_ = origBrick;
            svdagBuildWinExtent_ = view.windowExtent;
            svdagBuildOrigin_ = Vec3(static_cast<float>(origBrick.x) * brickWorldSize,
                                     static_cast<float>(origBrick.y) * brickWorldSize,
                                     static_cast<float>(origBrick.z) * brickWorldSize);
            svdagBuildBrickSize_ = brickWorldSize;
            svdagBuildRunning_.store(true);
            svdagWorker_ = std::jthread([this] {
                Core::profileThreadName("SvdagBuilder");
                const Core::ProfileZone buildZone("Svdag.Build");
                // De-wrap the snapshot into window-linear order, then build the DAG.
                const IVec3 n = svdagBuildFixedDims_;
                const IVec3 mn = svdagBuildWinMin_;
                const IVec3 ext = svdagBuildWinExtent_;
                std::vector<BrickHandle> linear;
                linear.reserve(static_cast<size_t>(ext.x) * static_cast<size_t>(ext.y)
                               * static_cast<size_t>(ext.z));
                for (int32_t z = 0; z < ext.z; ++z) {
                    for (int32_t y = 0; y < ext.y; ++y) {
                        for (int32_t x = 0; x < ext.x; ++x) {
                            const IVec3 slot{ (mn.x + x) & (n.x - 1), (mn.y + y) & (n.y - 1),
                                              (mn.z + z) & (n.z - 1) };
                            const size_t idx =
                                (static_cast<size_t>(slot.z) * static_cast<size_t>(n.x)
                                 * static_cast<size_t>(n.y))
                                + (static_cast<size_t>(slot.y) * static_cast<size_t>(n.x))
                                + static_cast<size_t>(slot.x);
                            linear.push_back(svdagBuildStorage_[idx]);
                        }
                    }
                }
                svdagBuildResult_ = linear.empty() ? Svdag{} : buildSvdag(linear, ext);
                svdagBuildDone_.store(true);
            });
        }

        {
            const Core::ProfileZone writeUbo("Render.FrameUniformsWrite");
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
            uniforms.mipSkipEnabled = mipSkipEnabled_ ? 1 : 0;
            uniforms.heatmapMode = heatmapMode_;
            uniforms.heatmapMaxSteps = heatmapMaxSteps_;
            uniforms.useSvdag = (useSvdag_ && svdagFresh_) ? 1 : 0;
            uniforms.useGridTexture =
                (useGridTexture_
                 && (worldGridSync_.textureValid() || worldGridSync_.textureUploadArmed()))
                    ? 1
                    : 0;
            adoptedFreeVolumesScratch_.clear();
            for (Voxel3D *v : freeVolumes_) {
                if (v->voxelData()->isAdopted()) {
                    adoptedFreeVolumesScratch_.push_back(v);
                }
            }
            uniforms.freeVolumeCount =
                static_cast<int32_t>(freeVolumeSync_.upload(adoptedFreeVolumesScratch_));

            std::memcpy(frameUniformBuffer_->mappedData(), &uniforms, sizeof(uniforms));
        }

        const Core::ProfileZone recordScope("Render.RecordFrame");
        VulkanCommandBuffer<queue_kind::Graphics> cmd = cmdPool_.allocate();
        VulkanRecorder<queue_kind::Graphics> rec = std::move(cmd).begin();
        const auto [swW, swH] = swapchain_.extent();
        const FrameContext frameCtx{ .camera = &camera };
        recordFrame(rec, swapchain_.image(acq.imageIndex), acq.imageIndex, swW, swH, frameCtx);
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

        Core::profileFrameMark();
    }

    VulkanRenderTarget &Renderer::resolveTarget(const Pass &pass) {
        return (pass.target != nullptr) ? *pass.target : *renderTarget_;
    }

    void Renderer::recordPass(VulkanRecorder<queue_kind::Graphics> &rec, VulkanRenderTarget &target,
                              const FrameContext & /*frame*/, bool /*isFirstPass*/,
                              bool /*isLastPass*/) {
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

        // Pipeline/material source: the world volume when present, else any
        // free-standing volume.
        Voxel3D *anyCaster = worldVolume_;
        if (anyCaster == nullptr && !freeVolumes_.empty()) {
            anyCaster = freeVolumes_[0];
        }
        if (anyCaster == nullptr || !anyCaster->hasMaterial()) {
            return;
        }
        VulkanPipeline *pipelinePtr = nullptr;
        {
            const Core::ProfileZone pipelineScope("Pass.PipelineGetOrCreate");
            pipelinePtr = &pipelineCache_.getOrCreate(*anyCaster->material());
        }
        VulkanPipeline &pipeline = *pipelinePtr;

        VkDescriptorSet set = descPool_.allocate(pipeline.descriptorSetLayouts()[0]);
        VulkanDescriptorWriter writer(ctx_.vkDevice());
        writer.writeStorageImage(set, 0, target, ImageLayout::General);
        writer.writeUniformBuffer(set, 2, *frameUniformBuffer_);
        writer.writeStorageBuffer(set, 3, *brickPoolBuffer_);
        writer.writeStorageBuffer(set, 4, worldGridSync_.handlesBuffer());
        writer.writeCombinedImageSampler(set, 11, worldGridSync_.imageView(),
                                         worldGridSync_.sampler(),
                                         ImageLayout::ShaderReadOnlyOptimal);
        writer.writeStorageBuffer(set, 12, freeVolumeSync_.descsBuffer());
        writer.writeStorageBuffer(set, 13, freeVolumeSync_.handlesBuffer());
        writer.writeStorageBuffer(set, 5, *pixelDebugBuffer_);
        writer.writeUniformBuffer(set, 6, worldGridSync_.paramsBuffer());
        writer.writeStorageBuffer(set, 9, *svdagCache_.nodesBuffer());
        writer.writeUniformBuffer(set, 10, *svdagCache_.paramsBuffer());

        {
            const Core::ProfileZone commitScope("Pass.DescriptorCommit");
            writer.commit();
        }

        const std::array<VkDescriptorSet, 1> sets{ set };
        const auto wg = anyCaster->material()->shader()->localWorkgroupSize();
        const uint32_t gx = (wg[0] > 0) ? ((tgtW + wg[0] - 1) / wg[0]) : 1;
        const uint32_t gy = (wg[1] > 0) ? ((tgtH + wg[1] - 1) / wg[1]) : 1;

        Core::profileGpuPassBegin("voxel_raycast", rec.rawHandle());
        pipeline.execute(rec, PipelineExecuteContext{ .descriptorSets = sets,
                                                      .pushConstants = {},
                                                      .groupsX = gx,
                                                      .groupsY = gy,
                                                      .groupsZ = 1 });
        Core::profileGpuPassEnd();
    }

    void Renderer::recordFrame(VulkanRecorder<queue_kind::Graphics> &rec, VkImage swapImage,
                               uint32_t swapImageIndex, uint32_t swapW, uint32_t swapH,
                               const FrameContext &frame) {
        const bool gridUploadArmed = worldGridSync_.textureUploadArmed();
        if (gridUploadArmed) {
            Core::profileGpuPassBegin("world_grid_upload", rec.rawHandle());
        }
        if (worldGridSync_.recordTextureUpload(rec) && gridUploadArmed) {
            Core::profileGpuPassEnd();
        }

        recordPass(rec, *renderTarget_, frame, true, true);

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

        Core::profileGpuPassBegin("blit_to_swapchain", rec.rawHandle());
        rec.blitImage(mainTargetImage, ImageLayout::TransferSrcOptimal, rtW, rtH, swapImage,
                      ImageLayout::TransferDstOptimal, swapW, swapH);
        Core::profileGpuPassEnd();

        if (Core::profileFrameImageWanted() && thumbnailTarget_.has_value() && thumbnailStaging_.has_value()) {
            Core::profileGpuPassBegin("thumbnail_readback", rec.rawHandle());
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
            Core::profileGpuPassEnd();
        }

        if (uiRender_) {
            const std::array<VulkanImageBarrier, 1> toColor{ { { .image = swapImage,
                                                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                  .oldLayout = ImageLayout::TransferDstOptimal,
                                                                  .newLayout = ImageLayout::ColorAttachmentOptimal,
                                                                  .srcStage = PipelineStage::Transfer,
                                                                  .dstStage = PipelineStage::ColorAttachmentOutput,
                                                                  .srcAccess = AccessFlags::TransferWrite,
                                                                  .dstAccess = AccessFlags::ColorAttachmentWrite } } };
            rec.pipelineBarrier(toColor);

            Core::profileGpuPassBegin("debug_ui", rec.rawHandle());
            uiRender_(UiRenderContext{ .cmd = rec.rawHandle(),
                                       .swapImage = swapImage,
                                       .swapImageIndex = swapImageIndex,
                                       .width = swapW,
                                       .height = swapH });
            Core::profileGpuPassEnd();

            const std::array<VulkanImageBarrier, 1> toPresent{ { { .image = swapImage,
                                                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .oldLayout = ImageLayout::ColorAttachmentOptimal,
                                                                    .newLayout = ImageLayout::PresentSrc,
                                                                    .srcStage = PipelineStage::ColorAttachmentOutput,
                                                                    .dstStage = PipelineStage::BottomOfPipe,
                                                                    .srcAccess = AccessFlags::ColorAttachmentWrite,
                                                                    .dstAccess = AccessFlags::None } } };
            rec.pipelineBarrier(toPresent);
        } else {
            const std::array<VulkanImageBarrier, 1> toPresent{ { { .image = swapImage,
                                                                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                                    .oldLayout = ImageLayout::TransferDstOptimal,
                                                                    .newLayout = ImageLayout::PresentSrc,
                                                                    .srcStage = PipelineStage::Transfer,
                                                                    .dstStage = PipelineStage::BottomOfPipe,
                                                                    .srcAccess = AccessFlags::TransferWrite,
                                                                    .dstAccess = AccessFlags::None } } };
            rec.pipelineBarrier(toPresent);
        }

        (void)passes_;
    }
}
