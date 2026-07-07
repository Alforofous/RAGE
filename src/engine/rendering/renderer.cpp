#include "renderer.hpp"
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
        class PhaseScope {
        public:
            PhaseScope(const Renderer::PhaseHook &begin, const Renderer::PhaseHook &end, const char *name)
                : end_(end)
                , name_(name) {
                if (begin) {
                    begin(name);
                }
            }
            ~PhaseScope() {
                if (end_) {
                    end_(name_);
                }
            }
            PhaseScope(const PhaseScope &) = delete;
            PhaseScope &operator=(const PhaseScope &) = delete;
            PhaseScope(PhaseScope &&) = delete;
            PhaseScope &operator=(PhaseScope &&) = delete;

        private:
            const Renderer::PhaseHook &end_;
            const char *name_;
        };
    }

    Renderer::Renderer(VulkanContext &ctx, VulkanAllocator &allocator, VulkanSwapchain &swapchain)
        : ctx_(ctx)
        , allocator_(allocator)
        , swapchain_(swapchain)
        , cmdPool_(ctx.vkDevice(), ctx.graphicsQueue().queueFamily())
        , descPool_(ctx.vkDevice(), {})
        , pipelineCache_(ctx.vkDevice())
        , svdagCache_(allocator) {
        brickPool_.emplace();   // default policy: dedup on
    }

    void Renderer::recreateBrickPool(bool enableDedup) {
        drainInFlight();
        vkDeviceWaitIdle(ctx_.vkDevice());
        brickPool_.reset();
        brickPool_.emplace(enableDedup);
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
            constexpr uint64_t kBrickPoolBytes =
                static_cast<uint64_t>(BrickPool::kMaxBricks) * sizeof(Brick);
            brickPoolBuffer_.emplace(allocator_.createBuffer({
                .size = kBrickPoolBytes,
                .usage = BufferUsage::Storage,
                .memory = MemoryLocation::CpuToGpu,
            }));
            std::memset(brickPoolBuffer_->mappedData(), 0, kBrickPoolBytes);
        }

        if (!worldBrickGridHandlesBuffer_.has_value()) {
            constexpr uint64_t kBytes = kMaxWorldBrickHandles * sizeof(BrickHandle);
            worldBrickGridHandlesBuffer_.emplace(allocator_.createBuffer({
                .size = kBytes,
                .usage = BufferUsage::Storage,
                .memory = MemoryLocation::CpuToGpu,
            }));
            std::memset(worldBrickGridHandlesBuffer_->mappedData(), 0, kBytes);
        }

        if (!worldGridImage_.has_value()) {
            worldGridImage_.emplace(allocator_.createImage({
                .width = kWorldGridTexDimXZ,
                .height = kWorldGridTexDimY,
                .depth = kWorldGridTexDimXZ,
                .format = ImageFormat::R32_UINT,
                .usage = ImageUsage::Sampled | ImageUsage::TransferDst,
                .memory = MemoryLocation::GpuOnly,
            }));
            worldGridImageView_.emplace(
                worldGridImage_->createView({ .viewType = ImageViewType::Type3D }));
            worldGridSampler_ = VulkanSampler::createNearestClamp(ctx_.vkDevice());

            constexpr uint64_t kStagingBytes = static_cast<uint64_t>(kWorldGridTexDimXZ)
                                               * kWorldGridTexDimY * kWorldGridTexDimXZ
                                               * sizeof(BrickHandle);
            worldGridStagingBuffer_.emplace(allocator_.createBuffer({
                .size = kStagingBytes,
                .usage = BufferUsage::TransferSrc,
                .memory = MemoryLocation::CpuToGpu,
            }));
        }

        if (!worldBrickGridParamsBuffer_.has_value()) {
            worldBrickGridParamsBuffer_.emplace(allocator_.createBuffer({
                .size = 32,                  // 2 × vec4 in std140
                .usage = BufferUsage::Uniform,
                .memory = MemoryLocation::CpuToGpu,
            }));
            std::memset(worldBrickGridParamsBuffer_->mappedData(), 0, 32);
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

    void Renderer::collectShadowCasters(Node3D &node) {
        // M3+M4: no fixed cap. World brick grid scales with the union of placements; the
        // brick pool deduplicates identical content. Previously had a `kMaxSceneCasters`
        // ceiling of 16 from the pre-M3 SceneCasters UBO — gone with that descriptor.
        auto *v = dynamic_cast<Voxel3D *>(&node);
        if (v != nullptr && v->voxelData() != nullptr) {
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
        const PhaseScope renderScope(phaseBegin_, phaseEnd_, "Renderer::render");

        const bool extentChanged = (extent != lastExtent_) && (lastExtent_.width != 0);
        if (needsRecreate_ || extentChanged) {
            const PhaseScope rebuild(phaseBegin_, phaseEnd_, "Render.RebuildFrameResources");
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
            const PhaseScope drain(phaseBegin_, phaseEnd_, "Render.DrainInFlight");
            drainInFlight();
        }

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

        // Dirty-brick upload doubles as the content-change signal: brick allocation marks
        // dirty, and a fresh brick can change the chunk handle grids the world grid indexes.
        std::vector<BrickHandle> dirtyBricks;
        {
            const PhaseScope brickUpload(phaseBegin_, phaseEnd_, "Render.DirtyBrickUpload");
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

        std::vector<Renderable *> visible;
        if (!sceneChanged) {
            worldGridUploadDims_ = IVec3{ 0, 0, 0 };
        } else {
            {
                const PhaseScope collect(phaseBegin_, phaseEnd_, "Render.Collect");
                collectVisible(root, visible);
                shadowCasters_.clear();
                collectShadowCasters(root);
            }

            const PhaseScope brickRebuild(phaseBegin_, phaseEnd_, "Render.WorldBrickGridRebuild");
            brickPlacementsScratch_.clear();
            brickPlacementsScratch_.reserve(shadowCasters_.size());
            float brickWorldSize = 0.0f;
            for (const Voxel3D *v : shadowCasters_) {
                if (brickWorldSize == 0.0f) {
                    brickWorldSize = v->voxelSize() * static_cast<float>(Brick::kDim);
                }
                const Vec3 p = v->worldMatrix().transformPoint(Vec3::zero());
                const IVec3 worldBrickOrigin{
                    static_cast<int32_t>(std::floor(p.x / brickWorldSize)),
                    static_cast<int32_t>(std::floor(p.y / brickWorldSize)),
                    static_cast<int32_t>(std::floor(p.z / brickWorldSize)),
                };
                brickPlacementsScratch_.push_back(
                    VoxelDataWorldPlacement{ .data = v->voxelData(), .worldBrickOrigin = worldBrickOrigin });
            }
            worldBrickGrid_.rebuild(brickPlacementsScratch_);

            // Upload params UBO: vec4(origin_xyz, cellSize), ivec4(dims_xyz, _).
            const IVec3 dims = worldBrickGrid_.dims();
            const IVec3 origBrick = worldBrickGrid_.worldBrickOrigin();
            struct WorldBrickGridParamsLayout {
                float originX;
                float originY;
                float originZ;
                float cellSize;
                int32_t dimsX;
                int32_t dimsY;
                int32_t dimsZ;
                int32_t voxelDim;
            };
            const WorldBrickGridParamsLayout params{
                .originX = static_cast<float>(origBrick.x) * brickWorldSize,
                .originY = static_cast<float>(origBrick.y) * brickWorldSize,
                .originZ = static_cast<float>(origBrick.z) * brickWorldSize,
                .cellSize = brickWorldSize,
                .dimsX = dims.x,
                .dimsY = dims.y,
                .dimsZ = dims.z,
                .voxelDim = Brick::kDim,
            };
            std::memcpy(worldBrickGridParamsBuffer_->mappedData(), &params, sizeof(params));

            const auto handles = worldBrickGrid_.handles();
            if (handles.size() > kMaxWorldBrickHandles) {
                throw std::runtime_error(
                    "Renderer: world brick grid handle count " + std::to_string(handles.size())
                    + " exceeds buffer capacity " + std::to_string(kMaxWorldBrickHandles));
            }
            if (!handles.empty()) {
                std::memcpy(worldBrickGridHandlesBuffer_->mappedData(), handles.data(),
                            handles.size() * sizeof(BrickHandle));
            }

            worldGridUploadDims_ = IVec3{ 0, 0, 0 };
            if (useGridTexture_ && !handles.empty()) {
                if (static_cast<uint32_t>(dims.x) > kWorldGridTexDimXZ
                    || static_cast<uint32_t>(dims.y) > kWorldGridTexDimY
                    || static_cast<uint32_t>(dims.z) > kWorldGridTexDimXZ) {
                    throw std::runtime_error(
                        "Renderer: world brick grid dims exceed 3D texture capacity ("
                        + std::to_string(dims.x) + "x" + std::to_string(dims.y) + "x"
                        + std::to_string(dims.z) + ")");
                }
                std::memcpy(worldGridStagingBuffer_->mappedData(), handles.data(),
                            handles.size() * sizeof(BrickHandle));
                worldGridUploadDims_ = dims;
            }

            if (useSvdag_) {
                const PhaseScope svdagBuild(phaseBegin_, phaseEnd_, "Render.SvdagUpdate");
                const Vec3 originWorld(static_cast<float>(origBrick.x) * brickWorldSize,
                                       static_cast<float>(origBrick.y) * brickWorldSize,
                                       static_cast<float>(origBrick.z) * brickWorldSize);
                const auto result = svdagCache_.update(handles, dims, originWorld, brickWorldSize);
                if (result.capacityExceeded) {
                    Core::log(Core::LogLevel::Warn,
                              "SVDAG node count exceeds GPU buffer capacity — falling back to flat grid");
                }
            }
        }

        {
            const PhaseScope writeUbo(phaseBegin_, phaseEnd_, "Render.FrameUniformsWrite");
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
            uniforms.useSvdag = useSvdag_ ? 1 : 0;
            uniforms.useGridTexture = (useGridTexture_ && worldGridUploadDims_.x > 0) ? 1 : 0;

            std::memcpy(frameUniformBuffer_->mappedData(), &uniforms, sizeof(uniforms));
        }

        const PhaseScope recordScope(phaseBegin_, phaseEnd_, "Render.RecordFrame");
        VulkanCommandBuffer<queue_kind::Graphics> cmd = cmdPool_.allocate();
        VulkanRecorder<queue_kind::Graphics> rec = std::move(cmd).begin();
        const auto [swW, swH] = swapchain_.extent();
        const FrameContext frameCtx{ .camera = &camera };
        recordFrame(rec, swapchain_.image(acq.imageIndex), acq.imageIndex, swW, swH, visible, frameCtx);
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
        VulkanPipeline *pipelinePtr = nullptr;
        {
            const PhaseScope pipelineScope(phaseBegin_, phaseEnd_, "Pass.PipelineGetOrCreate");
            pipelinePtr = &pipelineCache_.getOrCreate(*anyCaster->material());
        }
        VulkanPipeline &pipeline = *pipelinePtr;

        VkDescriptorSet set = descPool_.allocate(pipeline.descriptorSetLayouts()[0]);
        VulkanDescriptorWriter writer(ctx_.vkDevice());
        writer.writeStorageImage(set, 0, target, ImageLayout::General);
        writer.writeUniformBuffer(set, 2, *frameUniformBuffer_);
        writer.writeStorageBuffer(set, 3, *brickPoolBuffer_);
        writer.writeStorageBuffer(set, 4, *worldBrickGridHandlesBuffer_);
        writer.writeCombinedImageSampler(set, 11, *worldGridImageView_, worldGridSampler_,
                                         ImageLayout::ShaderReadOnlyOptimal);
        writer.writeStorageBuffer(set, 5, *pixelDebugBuffer_);
        writer.writeUniformBuffer(set, 6, *worldBrickGridParamsBuffer_);
        writer.writeStorageBuffer(set, 9, *svdagCache_.nodesBuffer());
        writer.writeUniformBuffer(set, 10, *svdagCache_.paramsBuffer());

        {
            const PhaseScope commitScope(phaseBegin_, phaseEnd_, "Pass.DescriptorCommit");
            writer.commit();
        }

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

    void Renderer::recordFrame(VulkanRecorder<queue_kind::Graphics> &rec, VkImage swapImage,
                               uint32_t swapImageIndex, uint32_t swapW, uint32_t swapH,
                               std::span<Renderable *const> renderables, const FrameContext &frame) {
        if (worldGridUploadDims_.x == 0 && !worldGridImageInitialized_) {
            // The descriptor set always references the grid texture, so it must sit in
            // ShaderReadOnlyOptimal even on frames that never sample it.
            const std::array<VulkanImageBarrier, 1> init{ { {
                .image = worldGridImage_->handle(),
                .oldLayout = ImageLayout::Undefined,
                .newLayout = ImageLayout::ShaderReadOnlyOptimal,
                .srcStage = PipelineStage::TopOfPipe,
                .dstStage = PipelineStage::ComputeShader,
                .srcAccess = AccessFlags::None,
                .dstAccess = AccessFlags::ShaderRead,
            } } };
            rec.pipelineBarrier(init);
            worldGridImageInitialized_ = true;
        }

        if (worldGridUploadDims_.x > 0) {
            worldGridImageInitialized_ = true;
            if (beforeGpuPass_) {
                beforeGpuPass_("world_grid_upload", rec.rawHandle());
            }
            const std::array<VulkanImageBarrier, 1> toTransfer{ { {
                .image = worldGridImage_->handle(),
                .oldLayout = ImageLayout::Undefined,
                .newLayout = ImageLayout::TransferDstOptimal,
                .srcStage = PipelineStage::ComputeShader,
                .dstStage = PipelineStage::Transfer,
                .srcAccess = AccessFlags::None,
                .dstAccess = AccessFlags::TransferWrite,
            } } };
            rec.pipelineBarrier(toTransfer);

            const std::array<BufferImageCopy, 1> region{ { {
                .imageWidth = static_cast<uint32_t>(worldGridUploadDims_.x),
                .imageHeight = static_cast<uint32_t>(worldGridUploadDims_.y),
                .imageDepth = static_cast<uint32_t>(worldGridUploadDims_.z),
            } } };
            rec.copyBufferToImage(*worldGridStagingBuffer_, *worldGridImage_,
                                  ImageLayout::TransferDstOptimal, region);

            const std::array<VulkanImageBarrier, 1> toSampled{ { {
                .image = worldGridImage_->handle(),
                .oldLayout = ImageLayout::TransferDstOptimal,
                .newLayout = ImageLayout::ShaderReadOnlyOptimal,
                .srcStage = PipelineStage::Transfer,
                .dstStage = PipelineStage::ComputeShader,
                .srcAccess = AccessFlags::TransferWrite,
                .dstAccess = AccessFlags::ShaderRead,
            } } };
            rec.pipelineBarrier(toSampled);
            if (afterGpuPass_) {
                afterGpuPass_("world_grid_upload", rec.rawHandle());
            }
        }

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

        if (beforeGpuPass_) {
            beforeGpuPass_("blit_to_swapchain", rec.rawHandle());
        }
        rec.blitImage(mainTargetImage, ImageLayout::TransferSrcOptimal, rtW, rtH, swapImage,
                      ImageLayout::TransferDstOptimal, swapW, swapH);
        if (afterGpuPass_) {
            afterGpuPass_("blit_to_swapchain", rec.rawHandle());
        }

        if (frameImage_ && thumbnailTarget_.has_value() && thumbnailStaging_.has_value()) {
            if (beforeGpuPass_) {
                beforeGpuPass_("thumbnail_readback", rec.rawHandle());
            }
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
            if (afterGpuPass_) {
                afterGpuPass_("thumbnail_readback", rec.rawHandle());
            }
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

            if (beforeGpuPass_) {
                beforeGpuPass_("debug_ui", rec.rawHandle());
            }
            uiRender_(UiRenderContext{ .cmd = rec.rawHandle(),
                                       .swapImage = swapImage,
                                       .swapImageIndex = swapImageIndex,
                                       .width = swapW,
                                       .height = swapH });
            if (afterGpuPass_) {
                afterGpuPass_("debug_ui", rec.rawHandle());
            }

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
