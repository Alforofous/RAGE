#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>
#include <GLFW/glfw3.h>
#include "app/build_paths.hpp"
#include "debug_ui.hpp"
#include "engine/toolkit/content/chunk_generators.hpp"
#include "engine/toolkit/content/hybrid_chunk_store.hpp"
#include "engine/toolkit/content/file_chunk_store.hpp"
#include "engine/toolkit/content/procedural_chunk_store.hpp"
#include "engine/toolkit/content/streamer.hpp"
#include "engine/toolkit/content/vox_loader.hpp"
#include "engine/toolkit/entity/kinematic_body.hpp"
#include "engine/materials/material.hpp"
#include "engine/rendering/pixel_debug.hpp"
#include "engine/rendering/renderer.hpp"
#include "platform/process_memory.hpp"
#include "shared/histogram.hpp"
#include "profiler.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/directional_light.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/svdag.hpp"
#include "engine/scene/voxel3d.hpp"
#include "engine/toolkit/pipeline/voxel_pipeline.hpp"
#include "free_fly_controller.hpp"
#include "math/color.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"
#include "window.hpp"

namespace {
    using namespace RAGE;

    int32_t nextPow2(int32_t v) {
        int32_t p = 1;
        while (p < v) {
            p *= 2;
        }
        return p;
    }

    /**
     * @brief Top-level world sizing — the single place capacities originate. Everything
     *        below (brick pool size, grid SSBO capacity, grid texture dims) is DERIVED
     *        here and injected into engine components via constructors; engine code owns
     *        no capacity constants and validates what it is given at the seams.
     */
    struct WorldPipelineConfig {
        int32_t streamRadius = 30;
        Toolkit::Content::ChunkStore::YRange yRange{ .min = -1, .max = 2 };
        IVec3 chunkBrickDims{ 4, 4, 4 };

        /// Bricks per axis the loaded window can span (XZ diameter × chunk, Y from yRange).
        IVec3 windowBrickExtent() const {
            const int32_t diameter = (2 * streamRadius) + 1;
            const int32_t yChunks = yRange.max - yRange.min + 1;
            return IVec3{ diameter * chunkBrickDims.x, yChunks * chunkBrickDims.y,
                          diameter * chunkBrickDims.z };
        }

        /// Grid capacity: window extent rounded to the next power of two per axis —
        /// headroom now, cheap wrap masks once the grid goes toroidal.
        IVec3 gridDims() const {
            const IVec3 w = windowBrickExtent();
            return IVec3{ nextPow2(w.x), nextPow2(w.y), nextPow2(w.z) };
        }

        /// Statistical, unlike the geometric bounds above: sized from measured occupancy
        /// of the streamed terrain (~110K unique bricks at radius 30) plus headroom.
        /// BrickPool throws loudly on exhaustion.
        size_t maxBricks() const {
            const IVec3 w = windowBrickExtent();
            const size_t windowCells = static_cast<size_t>(w.x) * static_cast<size_t>(w.y)
                                       * static_cast<size_t>(w.z);
            return (windowCells * 3) / 20;
        }

        Renderer::WorldLimits rendererLimits(bool brickDedup) const {
            return Renderer::WorldLimits{
                .brickPool = { .maxBricks = maxBricks(), .enableDedup = brickDedup },
                .worldGridDims = gridDims(),
            };
        }
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

int main(int argc, char **argv) {
    bool autoLaunchTracy = false;
    bool vsync = true;
    std::filesystem::path worldDir;   // --world=<dir>: persist streamed chunks here
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--profile") == 0) {
            autoLaunchTracy = true;
        } else if (std::strcmp(argv[i], "--no-vsync") == 0) {
            vsync = false;
        } else if (std::strncmp(argv[i], "--world=", 8) == 0) {
            worldDir = argv[i] + 8;
        }
    }

    // Tracy's on-demand mode only records allocs made after the GUI connects, so
    // construct + launch + wait BEFORE the engine allocates anything.
    App::Profiler profiler;
    if (autoLaunchTracy && App::Profiler::isLinked()) {
        profiler.launchProfilerGui();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!profiler.isConnected() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    try {
        App::Window window(1280, 720, "RAGE Smoke");

        const WorldPipelineConfig kWorld{};
        Toolkit::VoxelPipeline pipeline(window.vulkanSurfaceSource(),
                                        Toolkit::VoxelPipelineSettings{
                                            .appName = "RAGE Smoke",
                                            .shaderDir = executableDir(argv[0]) / "shaders",
                                            .vsync = vsync,
                                            .limits = kWorld.rendererLimits(true),
                                        });
        const auto voxelMaterial = pipeline.defaultVoxelMaterial();

        constexpr float kVoxelSize = 0.05f;

        const std::filesystem::path assetsDir = executableDir(argv[0]) / "assets";

        struct VoxelLoadJob {
            std::string label;
            Toolkit::Content::VoxModel model;
            Voxel3D *target = nullptr;
            std::atomic<float> loadProgress{ 0.0f };
        };
        std::vector<std::unique_ptr<VoxelLoadJob>> loadJobs;

        const auto [initW, initH] = window.framebufferExtent();
        // The camera is a child of a standalone entity node (design sheet Q5-A:
        // `entity.add(camera)`). The entity is deliberately NOT in the render root —
        // nothing draws it, and driving it every frame must not dirty the scene. In fly
        // mode the entity stays at origin, so camera local == world and the fly
        // controller behaves exactly as before.
        Node3D playerEntity;
        Camera &camera = playerEntity.add<Camera>(
            std::numbers::pi_v<float> * 0.4f,
            static_cast<float>(initW) / static_cast<float>(initH > 0 ? initH : 1), 0.1f, 100.0f);

        {
            Renderer &renderer = pipeline.renderer();
            Toolkit::CollisionWorld collisionWorld(renderer.worldBrickGrid(),
                                                   renderer.brickPool(), kVoxelSize);

            const auto stageVoxelFromFile = [&](const std::filesystem::path &voxPath, Vec3 position) {
                auto job = std::make_unique<VoxelLoadJob>();
                job->label = voxPath.filename().string();
                job->model = Toolkit::Content::loadVox(voxPath);
                auto v = std::make_unique<Voxel3D>(renderer.brickPool(), job->model.dims, kVoxelSize);
                job->target = v.get();
                v->setMaterial(voxelMaterial);
                v->setPosition(position);
                v->onLoadProgress([&jobRef = *job](float p) { jobRef.loadProgress.store(p); });
                std::fprintf(stdout, "Staged %s (%dx%dx%d)\n", voxPath.string().c_str(), job->model.dims.x,
                             job->model.dims.y, job->model.dims.z);
                loadJobs.push_back(std::move(job));
                return v;
            };

            Node3D root;

            std::unique_ptr<Toolkit::Content::ChunkStore> chunkStore;
            std::optional<Toolkit::Content::Streamer> streamer;
            std::vector<Voxel3D *> spinners;
            struct Prop {
                Voxel3D *volume;
                std::unique_ptr<Toolkit::KinematicBody> body;
            };
            std::vector<Prop> props;
            const int32_t kStreamHRadius = kWorld.streamRadius;
            const Toolkit::Content::ChunkStore::YRange kTerrainYRange = kWorld.yRange;

            // T1 demo: free-standing (full-TRS) volumes spinning above the terrain.
            const auto addSpinners = [&]() {
                constexpr int32_t kSpinnerDim = 24;
                const std::array<uint32_t, 3> palette{ 0xFF4A6FE3u, 0xFF3DB57Bu, 0xFFD1583Fu };
                for (size_t i = 0; i < palette.size(); ++i) {
                    auto v = std::make_unique<Voxel3D>(
                        renderer.brickPool(), IVec3{ kSpinnerDim, kSpinnerDim, kSpinnerDim },
                        kVoxelSize);
                    const uint32_t color = palette[i];
                    v->fill([color](IVec3 c) {
                        const bool checker = (((c.x / 6) + (c.y / 6) + (c.z / 6)) & 1) == 0;
                        return Color::fromRGBA8(checker ? color : 0xFFE8E4DCu);
                    });
                    v->setMaterial(voxelMaterial);
                    v->setRenderKind(VoxelRenderKind::FreeStanding);
                    v->setPosition(Vec3(-3.0f + (3.0f * static_cast<float>(i)), 3.0f, -4.0f));
                    spinners.push_back(v.get());
                    root.add(std::move(v));
                    collisionWorld.registerVolume(*spinners.back());
                }
            };

            // T5 demo: falling free-standing props with distinct masses — walk into the
            // light one and bulldoze it; the heavy one barely budges. Body boxes are
            // bottom-center based while Voxel3D renders corner-origin, so collision sits
            // half a cube off in XZ — known v1 slop.
            const auto addFallingProps = [&]() {
                constexpr int32_t kPropDim = 12;
                struct PropSpec {
                    uint32_t color;
                    float mass;
                };
                const std::array<PropSpec, 3> specs{ {
                    { .color = 0xFF9AC2E8u, .mass = 15.0f },     // light — pushable
                    { .color = 0xFF7AD1E0u, .mass = 80.0f },     // player-weight
                    { .color = 0xFFB4E8B0u, .mass = 2000.0f },   // heavy — immovable-ish
                } };
                for (size_t i = 0; i < specs.size(); ++i) {
                    auto v = std::make_unique<Voxel3D>(
                        renderer.brickPool(), IVec3{ kPropDim, kPropDim, kPropDim }, kVoxelSize);
                    v->fillSolid(Color::fromRGBA8(specs[i].color));
                    v->setMaterial(voxelMaterial);
                    v->setRenderKind(VoxelRenderKind::FreeStanding);
                    v->setPosition(Vec3(1.8f + (0.9f * static_cast<float>(i)),
                                        5.0f + (0.8f * static_cast<float>(i)), 1.8f));
                    Voxel3D *raw = v.get();
                    root.add(std::move(v));
                    const Toolkit::KinematicBodyConfig propCfg{
                        .size = Vec3(0.6f, 0.6f, 0.6f),
                        .gravity = 22.0f,
                        .terminalSpeed = 50.0f,
                        .jumpSpeed = 0.0f,
                        .stepUpHeight = 0.0f,
                        .mass = specs[i].mass,
                    };
                    props.push_back(Prop{
                        .volume = raw,
                        .body = std::make_unique<Toolkit::KinematicBody>(*raw, collisionWorld,
                                                                         propCfg, raw),
                    });
                }
            };

            const auto buildScene = [&]() {
                auto gen = std::make_unique<Toolkit::Content::TerrainChunkGenerator>();
                if (gen->chunkBrickDims() != kWorld.chunkBrickDims) {
                    throw std::runtime_error(
                        "WorldPipelineConfig.chunkBrickDims does not match the terrain generator");
                }
                auto procedural = std::make_unique<Toolkit::Content::ProceduralChunkStore>(
                    std::move(gen), renderer.brickPool(), kVoxelSize, kTerrainYRange);
                if (worldDir.empty()) {
                    chunkStore = std::move(procedural);
                } else {
                    // Persistent world: disk overlay over the generator, caching
                    // every generated chunk so revisits and restarts read from disk.
                    auto fileStore = std::make_unique<Toolkit::Content::FileChunkStore>(
                        worldDir, renderer.brickPool(), kWorld.chunkBrickDims, kVoxelSize,
                        kTerrainYRange);
                    chunkStore = std::make_unique<Toolkit::Content::HybridChunkStore>(
                        std::move(fileStore), std::move(procedural),
                        Toolkit::Content::WriteThrough::CacheBaselineReady);
                }
                streamer.emplace(*chunkStore, root);
                streamer->setOnChunkPrepare(
                    [&voxelMaterial](Voxel3D &v, IVec3) { v.setMaterial(voxelMaterial); });
                const IVec3 cb = kWorld.chunkBrickDims;
                streamer->setOnChunkPlaced([&renderer, cb](IVec3 c, Voxel3D &v) {
                    renderer.worldGridWriteChunk(IVec3{ c.x * cb.x, c.y * cb.y, c.z * cb.z },
                                                 *v.voxelData());
                });
                streamer->setOnChunkEvicted([&renderer, cb](IVec3 c, Voxel3D &v) {
                    renderer.worldGridClearChunk(IVec3{ c.x * cb.x, c.y * cb.y, c.z * cb.z },
                                                 v.voxelData()->brickDims());
                });
                renderer.setWorldGridStreaming(true);
                addSpinners();
                addFallingProps();
                // The .vox statue rides the streamed scene as a free-standing volume.
                auto statue = stageVoxelFromFile(assetsDir / "sphere.vox", Vec3(4.0f, 6.0f, -6.0f));
                statue->setRenderKind(VoxelRenderKind::FreeStanding);
                root.add(std::move(statue));
                camera.setPosition(Vec3(0.0f, 4.0f, 8.0f));
            };
            buildScene();

            std::atomic<bool> loaderDone{ false };
            profiler.attach(renderer);

            std::atomic<bool> loaderCancel{ false };

            const auto wireCancelHooks = [&]() {
                for (const auto &job : loadJobs) {
                    job->target->onLoadShouldCancel([&loaderCancel]() { return loaderCancel.load(); });
                }
            };
            wireCancelHooks();

            std::string loaderError;
            std::mutex loaderErrorMutex;
            const auto recordError = [&](const std::string &what) {
                std::lock_guard lock(loaderErrorMutex);
                loaderError = what;
                std::fprintf(stderr, "[scene] %s\n", what.c_str());
            };

            const auto runLoader = [&]() {
                profiler.setThreadName("AssetLoader");
                App::Profiler::Zone outerZone(profiler, "LoaderThread");
                try {
                    for (const auto &job : loadJobs) {
                        if (loaderCancel.load()) {
                            break;
                        }
                        App::Profiler::Zone perFileZone(profiler, "VoxelData::fillFromPackedRGBA8");
                        job->target->fillFromPackedRGBA8(job->model.voxels.data(), job->model.dims);
                        job->model.voxels.clear();
                        job->model.voxels.shrink_to_fit();
                    }
                } catch (const std::exception &e) {
                    recordError(std::string("loader: ") + e.what());
                }
                loaderDone.store(true);
            };
            std::thread loaderThread(runLoader);

            // Step order is RAII-load-bearing: brick handles must be released back to
            // their issuing pool BEFORE the pool itself is replaced.
            const auto resetScene = [&](bool enableDedup) {
                loaderCancel.store(true);
                if (loaderThread.joinable()) {
                    loaderThread.join();
                }
                streamer.reset();
                chunkStore.reset();
                collisionWorld.clearVolumes();
                props.clear();
                spinners.clear();
                root.clearChildren();
                loadJobs.clear();
                renderer.recreateBrickPool(enableDedup);
                loaderCancel.store(false);
                loaderDone.store(false);
                {
                    std::lock_guard lock(loaderErrorMutex);
                    loaderError.clear();
                }
                try {
                    buildScene();
                } catch (const std::exception &e) {
                    recordError(std::string("buildScene: ") + e.what());
                }
                wireCancelHooks();
                loaderThread = std::thread(runLoader);
            };

            App::DebugUi debugUi(pipeline.context(), window.glfwHandle());
            debugUi.attach(renderer);

            App::FreeFlyController controller(camera, window);
            controller.setMouseVeto([&debugUi]() { return debugUi.wantsMouse(); });
            // Walk mode: the kinematic body drives the entity, the fly controller keeps
            // mouse-look only (keyboard vetoed), and the camera rides at eye height.
            constexpr Toolkit::KinematicBodyConfig kPlayerBody{};
            constexpr float kEyeHeight = 1.62f;
            constexpr float kWalkSpeed = 4.0f;
            bool walkMode = false;
            bool prevWalkKey = false;
            Toolkit::KinematicBody playerBody(playerEntity, collisionWorld, kPlayerBody);

            controller.setKeyboardVeto(
                [&debugUi, &walkMode]() { return walkMode || debugUi.wantsKeyboard(); });

            bool mipSkipEnabled = renderer.mipSkipEnabled();
            int heatmapMode = renderer.heatmapMode();
            int heatmapMaxSteps = renderer.heatmapMaxSteps();
            bool useSvdag = renderer.useSvdag();
            bool gridTexture = renderer.useGridTexture();
            bool brickDedup = renderer.brickPool().isDedupEnabled();

            Core::Histogram<float, 128> frameMsHistory;
            Core::Histogram<float, 128> renderMsHistory;
            Core::Histogram<float, 128> brickmapMBHistory;
            Core::Histogram<float, 128> gpuMemoryMBHistory;
            Core::Histogram<float, 128> processRSSMBHistory;
            double uiFps = 0.0;
            double uiFrameMs = 0.0;
            debugUi.setBuilder([&]() {
                debugUi.beginPanel("RAGE Debug");
                if (!loaderDone.load()) {
                    debugUi.text("Loading assets...");
                    for (const auto &job : loadJobs) {
                        debugUi.text("  %s: load %.0f%%", job->label.c_str(),
                                     job->loadProgress.load() * 100.0f);
                    }
                }
                {
                    std::lock_guard lock(loaderErrorMutex);
                    if (!loaderError.empty()) {
                        debugUi.text("! %s", loaderError.c_str());
                    }
                }

                std::array<char, 64> header{};
                std::snprintf(header.data(), header.size(), "FPS %.0f  (%.2f ms)###fps", uiFps,
                              uiFrameMs);
                if (debugUi.collapsingHeader(header.data())) {
                    debugUi.plot("Frame", frameMsHistory.data(), frameMsHistory.capacity(),
                                 frameMsHistory.size(), frameMsHistory.oldestOffset(), "%.2f ms",
                                 0.0f, FLT_MAX);
                    debugUi.plot("Render", renderMsHistory.data(), renderMsHistory.capacity(),
                                 renderMsHistory.size(), renderMsHistory.oldestOffset(), "%.2f ms",
                                 0.0f, FLT_MAX);
                }

                const double rssMB = static_cast<double>(Platform::processResidentBytes())
                                     / (1024.0 * 1024.0);
                std::snprintf(header.data(), header.size(), "Memory %.0f MB###mem", rssMB);
                if (debugUi.collapsingHeader(header.data())) {
                    debugUi.text("Brick dedup:    %s", brickDedup ? "on" : "off");
                    if (debugUi.button(brickDedup ? "Restart with no dedup" : "Restart with dedup")) {
                        resetScene(!brickDedup);
                        brickDedup = !brickDedup;
                    }
                    const auto bricksUnique = renderer.brickPool().allocated();
                    const auto bricksLogical = renderer.brickPool().logicalBricks();
                    const double poolMB = static_cast<double>(renderer.brickPool().allocatedBytes())
                                          / (1024.0 * 1024.0);
                    size_t handleGridBytes = 0;
                    size_t denseBytes = 0;
                    std::function<void(const Node3D &)> walkScene = [&](const Node3D &node) {
                        if (const auto *v = dynamic_cast<const Voxel3D *>(&node)) {
                            if (const VoxelData *vd = v->voxelData()) {
                                handleGridBytes += vd->handleGridBytes();
                                denseBytes += vd->denseEquivalentBytes();
                            }
                        }
                        for (const auto &child : node.children()) {
                            walkScene(*child);
                        }
                    };
                    walkScene(root);
                    const double handleGridKB = static_cast<double>(handleGridBytes) / 1024.0;
                    const double brickmapTotalMB = poolMB + (handleGridKB / 1024.0);
                    const double denseMB = static_cast<double>(denseBytes) / (1024.0 * 1024.0);
                    const double savingsMB = denseMB - brickmapTotalMB;
                    const double savingsPct = denseMB > 0.0 ? (savingsMB / denseMB) * 100.0 : 0.0;
                    const double dedupRatio = bricksUnique > 0
                        ? static_cast<double>(bricksLogical) / static_cast<double>(bricksUnique)
                        : 1.0;

                    debugUi.text("Pool:           %.2f MB", poolMB);
                    debugUi.text("Handle grids:   %.2f KB", handleGridKB);
                    debugUi.text("Bricks unique:  %zu / %zu", bricksUnique,
                                 renderer.brickPool().maxBricks());
                    debugUi.text("Bricks logical: %zu", bricksLogical);
                    debugUi.text("Dedup ratio:    %.2fx", dedupRatio);
                    debugUi.text("Dense:          %.2f MB", denseMB);
                    debugUi.separator();
                    debugUi.text("Total:          %.2f MB  (saved %.2f MB / %.0f%%)",
                                 brickmapTotalMB, savingsMB, savingsPct);
                    debugUi.plot("Brickmap", brickmapMBHistory.data(), brickmapMBHistory.capacity(),
                                 brickmapMBHistory.size(), brickmapMBHistory.oldestOffset(),
                                 "%.2f MB", 0.0f, FLT_MAX);
                    debugUi.plot("Process RSS", processRSSMBHistory.data(),
                                 processRSSMBHistory.capacity(), processRSSMBHistory.size(),
                                 processRSSMBHistory.oldestOffset(), "%.0f MB", 0.0f, FLT_MAX);
                }

                const double gpuMB = static_cast<double>(pipeline.gpuMemoryUsedBytes())
                                     / (1024.0 * 1024.0);
                std::snprintf(header.data(), header.size(), "GPU %.0f MB###gpu", gpuMB);
                if (debugUi.collapsingHeader(header.data())) {
                    debugUi.plot("GPU memory", gpuMemoryMBHistory.data(),
                                 gpuMemoryMBHistory.capacity(), gpuMemoryMBHistory.size(),
                                 gpuMemoryMBHistory.oldestOffset(), "%.1f MB", 0.0f, FLT_MAX);
                    if (renderer.useSvdag() && !renderer.svdag().nodes.empty()) {
                        debugUi.separatorText("SVDAG (live)");
                        const Svdag &sv = renderer.svdag();
                        const size_t flatGridBytes =
                            renderer.worldBrickGrid().handles().size() * sizeof(BrickHandle);
                        const double svdagMB =
                            static_cast<double>(svdagBytes(sv)) / (1024.0 * 1024.0);
                        const double flatGridMB =
                            static_cast<double>(flatGridBytes) / (1024.0 * 1024.0);
                        const double svdagSavedMB = flatGridMB - svdagMB;
                        const double svdagSavedPct =
                            flatGridMB > 0.0 ? (svdagSavedMB / flatGridMB) * 100.0 : 0.0;
                        debugUi.text("Nodes:          %zu", sv.nodes.size());
                        debugUi.text("Levels:         %d (paddedDim=%d)", sv.levels, sv.paddedDim);
                        debugUi.text("SVDAG size:     %.3f MB", svdagMB);
                        debugUi.text("Flat grid:      %.3f MB", flatGridMB);
                        debugUi.text("Saved vs grid:  %.3f MB (%.0f%%)", svdagSavedMB,
                                     svdagSavedPct);
                    }
                }

                if (streamer.has_value()) {
                    std::snprintf(header.data(), header.size(), "Streaming  %zu chunks###stream",
                                  streamer->loadedCount());
                    if (debugUi.collapsingHeader(header.data())) {
                        debugUi.text("Loaded:  %zu chunks", streamer->loadedCount());
                        debugUi.text("Skipped: %zu chunks", streamer->skippedCount());
                        debugUi.text("Radius:  h=%d  Y=[%d,%d]", kStreamHRadius, kTerrainYRange.min,
                                     kTerrainYRange.max);
                    }
                }

                if (debugUi.collapsingHeader("Controls")) {
                    if constexpr (App::kIsDevBuild) {
                        if (App::Profiler::isLinked()) {
                            if (profiler.isProfilerGuiRunning()) {
                                debugUi.text("Tracy: running (close to relaunch)");
                            } else if (debugUi.button("Launch Tracy")) {
                                profiler.launchProfilerGui();
                            }
                        }
                    }
                    if (debugUi.checkbox("Mip skip", &mipSkipEnabled)) {
                        renderer.setMipSkipEnabled(mipSkipEnabled);
                    }
                    if (debugUi.checkbox("SVDAG traversal", &useSvdag)) {
                        renderer.setUseSvdag(useSvdag);
                    }
                    if (debugUi.checkbox("Grid 3D texture", &gridTexture)) {
                        renderer.setUseGridTexture(gridTexture);
                    }
                    static const char *const kHeatmapOpts[] = { "Off", "Step count" };
                    if (debugUi.radio("Heatmap", &heatmapMode,
                                      std::span<const char *const>(kHeatmapOpts,
                                                                   std::size(kHeatmapOpts)))) {
                        renderer.setHeatmapMode(heatmapMode);
                    }
                    if (heatmapMode != 0
                        && debugUi.sliderInt("Heatmap max steps", &heatmapMaxSteps, 32, 4096)) {
                        renderer.setHeatmapMaxSteps(heatmapMaxSteps);
                    }
                    debugUi.separatorText("Camera");
                    debugUi.text("Walk mode (V): %s%s", walkMode ? "on" : "off",
                                 (walkMode && playerBody.grounded()) ? " - grounded" : "");
                    float speedMult = controller.speedMultiplier();
                    if (debugUi.sliderFloat("Speed (x)", &speedMult, 0.1f, 10.0f)) {
                        controller.setSpeedMultiplier(speedMult);
                    }
                }

                debugUi.endPanel();
            });

            auto sun = std::make_shared<DirectionalLight>(Vec3(-1.0f, -1.0f, -1.0f),
                                                          Color(1.0f, 0.95f, 0.85f), 1.2f);
            renderer.addLight(sun);
            renderer.setAmbientLight({ .color = Color(0.4f, 0.5f, 0.8f), .intensity = 0.25f });

            bool prevRight = false;
            double lastTime = glfwGetTime();
            double fpsAccumDt = 0.0;
            uint32_t fpsAccumFrames = 0;
            double fpsLastUpdate = lastTime;
            while (!window.shouldClose()) {
                window.pollEvents();
                const double now = glfwGetTime();
                const auto dt = static_cast<float>(now - lastTime);
                lastTime = now;

                frameMsHistory.push(dt * 1000.0f);
                gpuMemoryMBHistory.push(static_cast<float>(pipeline.gpuMemoryUsedBytes())
                                        / (1024.0f * 1024.0f));
                const float brickPoolMB =
                    static_cast<float>(renderer.brickPool().allocatedBytes()) / (1024.0f * 1024.0f);
                const float handleGridMB = static_cast<float>(renderer.worldBrickGrid().handles().size()
                                                              * sizeof(BrickHandle))
                                           / (1024.0f * 1024.0f);
                brickmapMBHistory.push(brickPoolMB + handleGridMB);
                const uint64_t rss = Platform::processResidentBytes();
                if (rss > 0) {
                    processRSSMBHistory.push(static_cast<float>(rss) / (1024.0f * 1024.0f));
                }

                fpsAccumDt += dt;
                ++fpsAccumFrames;
                if (now - fpsLastUpdate > 0.25) {
                    const double avgMs = (fpsAccumDt / static_cast<double>(fpsAccumFrames)) * 1000.0;
                    const double fps = static_cast<double>(fpsAccumFrames) / (now - fpsLastUpdate);
                    uiFps = fps;
                    uiFrameMs = avgMs;
                    std::array<char, 128> titleBuf{};
                    std::snprintf(titleBuf.data(), titleBuf.size(), "RAGE Smoke | FPS %.1f | %.2f ms", fps, avgMs);
                    window.setTitle(titleBuf.data());
                    profiler.plot("fps", fps);
                    profiler.plot("frame_ms", avgMs);
                    profiler.plot("brick_count",
                                  static_cast<double>(renderer.brickPool().allocated()));
                    profiler.plot("brick_pool_mb",
                                  static_cast<double>(renderer.brickPool().allocatedBytes())
                                      / (1024.0 * 1024.0));
                    if (streamer.has_value()) {
                        profiler.plot("chunks_loaded",
                                      static_cast<double>(streamer->loadedCount()));
                        profiler.plot("chunks_pending",
                                      static_cast<double>(streamer->pendingCount()));
                    }
                    fpsLastUpdate = now;
                    fpsAccumDt = 0.0;
                    fpsAccumFrames = 0;
                }

                if (!walkMode) {
                    controller.applyScrollDelta(debugUi.scrollDelta());
                }
                controller.update(dt);

                for (size_t i = 0; i < spinners.size(); ++i) {
                    const float speed = 0.4f + (0.3f * static_cast<float>(i));
                    constexpr float kInvSqrt3 = std::numbers::inv_sqrt3_v<float>;
                    const Vec3 axis = (i % 2 == 0) ? Vec3(0.0f, 1.0f, 0.0f)
                                                   : Vec3(kInvSqrt3, kInvSqrt3, kInvSqrt3);
                    spinners[i]->setRotation(
                        Quat::fromAxisAngle(axis, static_cast<float>(now) * speed));
                }
                {
                    const App::Profiler::Zone physicsZone(profiler, "Physics.Bodies");
                    for (Prop &prop : props) {
                        prop.body->update(Toolkit::MoveInput{}, dt);
                    }
                }

                const bool walkKey = !debugUi.wantsKeyboard()
                                     && glfwGetKey(window.glfwHandle(), GLFW_KEY_V) == GLFW_PRESS;
                if (walkKey && !prevWalkKey) {
                    walkMode = !walkMode;
                    const Vec3 camWorld = camera.worldMatrix().transformPoint(Vec3::zero());
                    if (walkMode) {
                        playerEntity.setPosition(camWorld - Vec3(0.0f, kEyeHeight, 0.0f));
                        camera.setPosition(Vec3(0.0f, kEyeHeight, 0.0f));
                    } else {
                        playerEntity.setPosition(Vec3::zero());
                        camera.setPosition(camWorld);
                    }
                }
                prevWalkKey = walkKey;

                if (walkMode) {
                    GLFWwindow *gw = window.glfwHandle();
                    const Mat4 camWorldM = camera.worldMatrix();
                    Vec3 fwd = camWorldM.transformDirection(Vec3(0.0f, 0.0f, -1.0f));
                    fwd.y = 0.0f;
                    Vec3 right = camWorldM.transformDirection(Vec3(1.0f, 0.0f, 0.0f));
                    right.y = 0.0f;
                    Vec3 walk(0.0f, 0.0f, 0.0f);
                    if (!debugUi.wantsKeyboard()) {
                        float f = 0.0f;
                        float r = 0.0f;
                        if (glfwGetKey(gw, GLFW_KEY_W) == GLFW_PRESS) { f += 1.0f; }
                        if (glfwGetKey(gw, GLFW_KEY_S) == GLFW_PRESS) { f -= 1.0f; }
                        if (glfwGetKey(gw, GLFW_KEY_D) == GLFW_PRESS) { r += 1.0f; }
                        if (glfwGetKey(gw, GLFW_KEY_A) == GLFW_PRESS) { r -= 1.0f; }
                        const Vec3 dir = (fwd * f) + (right * r);
                        if (dir.length() > 1e-3f) {
                            walk = dir.normalized() * kWalkSpeed;
                        }
                    }
                    const Toolkit::MoveInput moveIn{
                        .walk = walk,
                        .jump = !debugUi.wantsKeyboard()
                                && glfwGetKey(gw, GLFW_KEY_SPACE) == GLFW_PRESS,
                    };
                    {
                        const App::Profiler::Zone playerZone(profiler, "Physics.Player");
                        playerBody.update(moveIn, dt);
                    }
                }

                if (streamer.has_value()) {
                    const App::Profiler::Zone streamerZone(profiler, "Streamer.Update");
                    const float chunkWorldExtent =
                        static_cast<float>(chunkStore->chunkBrickDims().x) * 8.0f * kVoxelSize;
                    const Vec3 camPos = camera.worldMatrix().transformPoint(Vec3::zero());
                    const IVec3 focus{
                        static_cast<int32_t>(std::floor(camPos.x / chunkWorldExtent)),
                        static_cast<int32_t>(std::floor(camPos.y / chunkWorldExtent)),
                        static_cast<int32_t>(std::floor(camPos.z / chunkWorldExtent)),
                    };
                    // Window first: placement events fired by update() must land inside
                    // the window the grid asserts against.
                    const IVec3 cb = kWorld.chunkBrickDims;
                    renderer.setWorldGridWindow(
                        IVec3{ (focus.x - kStreamHRadius) * cb.x, kTerrainYRange.min * cb.y,
                               (focus.z - kStreamHRadius) * cb.z },
                        kWorld.windowBrickExtent());
                    streamer->update(focus, kStreamHRadius);
                }

                const auto [w, h] = window.framebufferExtent();
                window.clearResized();
                if (h > 0) {
                    camera.setAspect(static_cast<float>(w) / static_cast<float>(h));
                }

                GLFWwindow *gw = window.glfwHandle();
                const bool rightDown =
                    !debugUi.wantsMouse() && (glfwGetMouseButton(gw, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
                if (rightDown && !prevRight) {
                    const int cursorMode = glfwGetInputMode(gw, GLFW_CURSOR);
                    int32_t px = static_cast<int32_t>(w) / 2;
                    int32_t py = static_cast<int32_t>(h) / 2;
                    if (cursorMode == GLFW_CURSOR_NORMAL) {
                        double mx = 0.0;
                        double my = 0.0;
                        glfwGetCursorPos(gw, &mx, &my);
                        px = static_cast<int32_t>(mx);
                        py = static_cast<int32_t>(my);
                    }
                    renderer.setPickTarget(px, py);
                }
                prevRight = rightDown;

                const double renderStart = glfwGetTime();
                pipeline.render(root, camera);
                renderMsHistory.push(static_cast<float>((glfwGetTime() - renderStart) * 1000.0));

                PixelDebug pd;
                if (renderer.tryReadPick(pd)) {
                    std::fprintf(stdout,
                                 "[pick] hit=%d caster=%d voxel=(%d,%d,%d) t=%.4f\n"
                                 "       camera=(%.4f,%.4f,%.4f) rayDir=(%.4f,%.4f,%.4f)\n"
                                 "       hitWorld=(%.4f,%.4f,%.4f) normal=(%.3f,%.3f,%.3f)\n"
                                 "       toLight=(%.3f,%.3f,%.3f) NdotL=%.4f shadowed=%d\n"
                                 "       blocker: caster=%d voxel=(%d,%d,%d)\n"
                                 "       finalRGB=(%.3f,%.3f,%.3f) packedRGBA8=0x%08X\n",
                                 pd.hit, pd.casterIdx, pd.voxelX, pd.voxelY, pd.voxelZ, pd.tHit,
                                 pd.cameraX, pd.cameraY, pd.cameraZ,
                                 pd.rayDirX, pd.rayDirY, pd.rayDirZ,
                                 pd.hitWorldX, pd.hitWorldY, pd.hitWorldZ,
                                 pd.normalX, pd.normalY, pd.normalZ,
                                 pd.toLightX, pd.toLightY, pd.toLightZ, pd.NdotL, pd.shadowed,
                                 pd.blockerCasterIdx, pd.blockerX, pd.blockerY, pd.blockerZ,
                                 pd.finalR, pd.finalG, pd.finalB, pd.packedColor);
                    std::fflush(stdout);
                }
            }
            loaderCancel.store(true);
            if (loaderThread.joinable()) {
                loaderThread.join();
            }
        }
    } catch (const std::exception &e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());

        return 1;
    }

    return 0;
}
