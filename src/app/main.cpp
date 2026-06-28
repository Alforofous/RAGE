#include <algorithm>
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
#include <stdexcept>
#include <thread>
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "app/build_paths.hpp"
#include "debug_ui.hpp"
#include "engine/content/vox_loader.hpp"
#include "engine/materials/material.hpp"
#include "engine/rendering/pixel_debug.hpp"
#include "engine/rendering/renderer.hpp"
#include "phase_tracker.hpp"
#include "platform/process_memory.hpp"
#include "shared/histogram.hpp"
#include "profiler.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/directional_light.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/svdag.hpp"
#include "engine/scene/voxel3d.hpp"
#include "free_fly_controller.hpp"
#include "gpu/vulkan/vulkan_allocator.hpp"
#include "gpu/vulkan/vulkan_context.hpp"
#include "gpu/vulkan/vulkan_queue.hpp"
#include "gpu/vulkan/vulkan_shader_module.hpp"
#include "gpu/vulkan/vulkan_swapchain.hpp"
#include "math/color.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"
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

int main(int argc, char **argv) {
    bool autoLaunchTracy = false;
    enum class SceneKind { Sphere, Cubes, Terrain, BigWorld };
    SceneKind scene = SceneKind::Sphere;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--profile") == 0) {
            autoLaunchTracy = true;
        } else if (std::strcmp(argv[i], "--scene=cubes") == 0) {
            scene = SceneKind::Cubes;
        } else if (std::strcmp(argv[i], "--scene=terrain") == 0) {
            scene = SceneKind::Terrain;
        } else if (std::strcmp(argv[i], "--scene=bigworld") == 0) {
            scene = SceneKind::BigWorld;
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

        VulkanContext ctx(buildContextInfo());
        VulkanAllocator allocator(ctx.vkInstance(), ctx.physicalDevice(), ctx.vkDevice());
        const SurfaceGuard surfaceGuard(ctx.vkInstance(), window.glfwHandle());

        const std::filesystem::path shaderDir = executableDir(argv[0]) / "shaders";
        auto voxelShader = std::make_shared<const VulkanShaderModule>(
            VulkanShaderModule::fromFile(ctx.vkDevice(), shaderDir / "voxel_raycast.comp.spv"));

        auto voxelMaterial = std::make_shared<Material<VulkanShaderModule>>(voxelShader);

        constexpr float kVoxelSize = 0.05f;

        const std::filesystem::path assetsDir = executableDir(argv[0]) / "assets";

        struct VoxelLoadJob {
            std::string label;
            Content::VoxModel model;
            Voxel3D *target = nullptr;
            std::atomic<float> loadProgress{ 0.0f };
        };
        std::vector<std::unique_ptr<VoxelLoadJob>> loadJobs;

        const auto [initW, initH] = window.framebufferExtent();
        Camera camera(std::numbers::pi_v<float> * 0.4f,
                      static_cast<float>(initW) / static_cast<float>(initH > 0 ? initH : 1), 0.1f, 100.0f);

        {
            VulkanSwapchain swapchain({ .physicalDevice = ctx.physicalDevice(),
                                        .device = ctx.vkDevice(),
                                        .surface = surfaceGuard.handle(),
                                        .graphicsQueueFamily = ctx.graphicsQueue().queueFamily(),
                                        .width = initW,
                                        .height = initH,
                                        .vsync = true });

            Renderer renderer(ctx, allocator, swapchain);

            const auto stageVoxelFromFile = [&](const std::filesystem::path &voxPath, Vec3 position) {
                auto job = std::make_unique<VoxelLoadJob>();
                job->label = voxPath.filename().string();
                job->model = Content::loadVox(voxPath);
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

            const auto buildScene = [&]() {
            if (scene == SceneKind::Cubes) {
                constexpr int32_t kCubeDim = 16;
                constexpr int kGridN = 16;          // 16×16 = 256 cubes
                constexpr float kCubeSpacing = 1.5f;
                constexpr uint32_t kCubeColor = 0xFFCCDDEEu;
                const std::vector<uint32_t> cubeVoxels(
                    static_cast<size_t>(kCubeDim) * kCubeDim * kCubeDim, kCubeColor);
                const float kOriginX = -static_cast<float>(kGridN - 1) * kCubeSpacing * 0.5f;
                const float kOriginZ = -16.0f;
                for (int gx = 0; gx < kGridN; ++gx) {
                    for (int gz = 0; gz < kGridN; ++gz) {
                        auto cube = std::make_unique<Voxel3D>(renderer.brickPool(),
                                                              IVec3(kCubeDim, kCubeDim, kCubeDim),
                                                              kVoxelSize);
                        cube->fillFromPackedRGBA8(cubeVoxels.data(),
                                                  IVec3(kCubeDim, kCubeDim, kCubeDim));
                        cube->setMaterial(voxelMaterial);
                        cube->setPosition(Vec3(kOriginX + (static_cast<float>(gx) * kCubeSpacing),
                                                -0.4f,
                                                kOriginZ + (static_cast<float>(gz) * kCubeSpacing)));
                        root.add(std::move(cube));
                    }
                }
                std::fprintf(stdout, "Built procedural cube scene: %d×%d = %d cubes\n", kGridN,
                             kGridN, kGridN * kGridN);
            } else if (scene == SceneKind::Terrain) {
                constexpr int32_t kW = 256;
                constexpr int32_t kH = 64;
                constexpr int32_t kD = 256;
                const auto pack = [](int r, int g, int b) -> uint32_t {
                    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8u)
                           | (static_cast<uint32_t>(b) << 16u) | 0xFF000000u;
                };
                const uint32_t kStone = pack(0x70, 0x70, 0x70);
                const uint32_t kDirt  = pack(0x6E, 0x4E, 0x35);
                const uint32_t kGrass = pack(0x4A, 0x8A, 0x3D);
                std::vector<uint32_t> src(static_cast<size_t>(kW) * kH * kD, 0u);
                for (int32_t z = 0; z < kD; ++z) {
                    for (int32_t x = 0; x < kW; ++x) {
                        const float fx = static_cast<float>(x);
                        const float fz = static_cast<float>(z);
                        const float h = (std::sin(fx * 0.10f) * 4.0f) + (std::sin(fz * 0.08f) * 3.0f)
                                        + 28.0f;
                        const int32_t height = std::clamp(static_cast<int32_t>(h), 1, kH - 1);
                        for (int32_t y = 0; y <= height; ++y) {
                            const int32_t depth = height - y;
                            uint32_t color = kStone;
                            if (depth == 0) {
                                color = kGrass;
                            } else if (depth <= 3) {
                                color = kDirt;
                            }
                            const size_t idx = (static_cast<size_t>(z) * kH * kW)
                                               + (static_cast<size_t>(y) * kW) + static_cast<size_t>(x);
                            src[idx] = color;
                        }
                    }
                }
                auto terrain = std::make_unique<Voxel3D>(renderer.brickPool(), IVec3(kW, kH, kD),
                                                         kVoxelSize);
                terrain->fillFromPackedRGBA8(src.data(), IVec3(kW, kH, kD));
                terrain->setMaterial(voxelMaterial);
                terrain->setPosition(Vec3(-6.4f, -3.2f, -16.0f));
                root.add(std::move(terrain));
                std::fprintf(stdout, "Built procedural terrain scene: %d×%d×%d voxels\n", kW, kH, kD);
            } else if (scene == SceneKind::BigWorld) {
                constexpr int32_t kTileW = 128;
                constexpr int32_t kTileH = 32;
                constexpr int32_t kTileD = 128;
                constexpr int kGridN = 8;
                const auto pack = [](int r, int g, int b) -> uint32_t {
                    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8u)
                           | (static_cast<uint32_t>(b) << 16u) | 0xFF000000u;
                };
                const uint32_t kStone = pack(0x70, 0x70, 0x70);
                const uint32_t kDirt  = pack(0x6E, 0x4E, 0x35);
                const uint32_t kGrass = pack(0x4A, 0x8A, 0x3D);
                std::vector<uint32_t> tileVoxels(static_cast<size_t>(kTileW) * kTileH * kTileD, 0u);
                for (int32_t z = 0; z < kTileD; ++z) {
                    for (int32_t x = 0; x < kTileW; ++x) {
                        const float fx = static_cast<float>(x);
                        const float fz = static_cast<float>(z);
                        const float h = (std::sin(fx * 0.10f) * 3.0f) + (std::sin(fz * 0.08f) * 2.0f)
                                        + 14.0f;
                        const int32_t height = std::clamp(static_cast<int32_t>(h), 1, kTileH - 1);
                        for (int32_t y = 0; y <= height; ++y) {
                            const int32_t depth = height - y;
                            uint32_t color = kStone;
                            if (depth == 0) {
                                color = kGrass;
                            } else if (depth <= 3) {
                                color = kDirt;
                            }
                            const size_t idx = (static_cast<size_t>(z) * kTileH * kTileW)
                                               + (static_cast<size_t>(y) * kTileW) + static_cast<size_t>(x);
                            tileVoxels[idx] = color;
                        }
                    }
                }
                const float kTileWorld = static_cast<float>(kTileW) * kVoxelSize;
                const float kOriginX = -kTileWorld * static_cast<float>(kGridN) * 0.5f;
                const float kOriginZ = -kTileWorld * static_cast<float>(kGridN) * 0.5f;
                for (int gx = 0; gx < kGridN; ++gx) {
                    for (int gz = 0; gz < kGridN; ++gz) {
                        auto tile = std::make_unique<Voxel3D>(renderer.brickPool(),
                                                              IVec3(kTileW, kTileH, kTileD), kVoxelSize);
                        tile->fillFromPackedRGBA8(tileVoxels.data(), IVec3(kTileW, kTileH, kTileD));
                        tile->setMaterial(voxelMaterial);
                        tile->setPosition(Vec3(kOriginX + (static_cast<float>(gx) * kTileWorld),
                                                -1.6f,
                                                kOriginZ + (static_cast<float>(gz) * kTileWorld)));
                        root.add(std::move(tile));
                    }
                }
                std::fprintf(stdout, "Built tiled big-world scene: %d×%d tiles of %d×%d×%d voxels\n",
                             kGridN, kGridN, kTileW, kTileH, kTileD);
            } else {
                root.add(stageVoxelFromFile(assetsDir / "floor.vox", Vec3(-6.4f, -7.0f, -22.4f)));
                root.add(stageVoxelFromFile(assetsDir / "sphere.vox", Vec3(-6.4f, -6.4f, -22.4f)));
            }
            };   // end of buildScene lambda
            buildScene();

            std::atomic<bool> loaderDone{ false };
            profiler.attach(renderer);
            App::PhaseTracker phaseTracker;
            phaseTracker.attach(renderer);

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

            App::DebugUi debugUi(ctx, window.glfwHandle());
            debugUi.attach(renderer);

            App::FreeFlyController controller(camera, window);
            controller.setMouseVeto([&debugUi]() { return debugUi.wantsMouse(); });
            controller.setKeyboardVeto([&debugUi]() { return debugUi.wantsKeyboard(); });

            bool mipSkipEnabled = renderer.mipSkipEnabled();
            int heatmapMode = renderer.heatmapMode();
            int heatmapMaxSteps = renderer.heatmapMaxSteps();
            bool useSvdag = renderer.useSvdag();
            bool brickDedup = renderer.brickPool().isDedupEnabled();

            Core::Histogram<float, 128> frameMsHistory;
            Core::Histogram<float, 128> renderMsHistory;
            Core::Histogram<float, 128> brickmapMBHistory;
            Core::Histogram<float, 128> gpuMemoryMBHistory;
            Core::Histogram<float, 128> processRSSMBHistory;
            debugUi.setBuilder([&]() {
                debugUi.beginPanel("RAGE Debug");
                if (!loaderDone.load()) {
                    debugUi.text("Loading assets...");
                    for (const auto &job : loadJobs) {
                        debugUi.text("  %s: load %.0f%%", job->label.c_str(),
                                     job->loadProgress.load() * 100.0f);
                    }
                }
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
                static const char *const kHeatmapOpts[] = { "Off", "Step count" };
                if (debugUi.radio("Heatmap", &heatmapMode,
                                  std::span<const char *const>(kHeatmapOpts, std::size(kHeatmapOpts)))) {
                    renderer.setHeatmapMode(heatmapMode);
                }
                if (heatmapMode != 0 && debugUi.sliderInt("Heatmap max steps", &heatmapMaxSteps, 32, 4096)) {
                    renderer.setHeatmapMaxSteps(heatmapMaxSteps);
                }
                debugUi.separatorText("Camera");
                float speedMult = controller.speedMultiplier();
                if (debugUi.sliderFloat("Speed (x)", &speedMult, 0.1f, 10.0f)) {
                    controller.setSpeedMultiplier(speedMult);
                }

                debugUi.separatorText("Memory");
                debugUi.text("Brick dedup:    %s", brickDedup ? "on" : "off");
                if (debugUi.button(brickDedup ? "Restart with no dedup" : "Restart with dedup")) {
                    resetScene(!brickDedup);
                    brickDedup = !brickDedup;
                }
                {
                    std::lock_guard lock(loaderErrorMutex);
                    if (!loaderError.empty()) {
                        debugUi.text("! %s", loaderError.c_str());
                    }
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
                const double dedupRatio =
                    bricksUnique > 0 ? static_cast<double>(bricksLogical) / static_cast<double>(bricksUnique) : 1.0;

                debugUi.text("Pool:           %.2f MB", poolMB);
                debugUi.text("Handle grids:   %.2f KB", handleGridKB);
                debugUi.text("Bricks unique:  %zu / %zu", bricksUnique, BrickPool::kMaxBricks);
                debugUi.text("Bricks logical: %zu", bricksLogical);
                debugUi.text("Dedup ratio:    %.2fx", dedupRatio);
                debugUi.text("Dense:          %.2f MB", denseMB);
                debugUi.separator();
                debugUi.text("Total:          %.2f MB  (saved %.2f MB / %.0f%%)",
                             brickmapTotalMB, savingsMB, savingsPct);

                debugUi.separatorText("History");
                debugUi.plot("Frame", frameMsHistory.data(), frameMsHistory.capacity(),
                             frameMsHistory.size(), frameMsHistory.oldestOffset(), "%.2f ms",
                             0.0f, FLT_MAX);
                debugUi.plot("Render", renderMsHistory.data(), renderMsHistory.capacity(),
                             renderMsHistory.size(), renderMsHistory.oldestOffset(), "%.2f ms",
                             0.0f, FLT_MAX);
                debugUi.plot("Brickmap", brickmapMBHistory.data(), brickmapMBHistory.capacity(),
                             brickmapMBHistory.size(), brickmapMBHistory.oldestOffset(), "%.2f MB",
                             0.0f, FLT_MAX);
                debugUi.plot("GPU memory", gpuMemoryMBHistory.data(),
                             gpuMemoryMBHistory.capacity(), gpuMemoryMBHistory.size(),
                             gpuMemoryMBHistory.oldestOffset(), "%.1f MB", 0.0f, FLT_MAX);
                debugUi.plot("Process RSS", processRSSMBHistory.data(),
                             processRSSMBHistory.capacity(), processRSSMBHistory.size(),
                             processRSSMBHistory.oldestOffset(), "%.0f MB", 0.0f, FLT_MAX);

                debugUi.separatorText("Phases");
                phaseTracker.forEachPhase([&](const std::string &name,
                                              const App::PhaseTracker::PhaseHistory &h) {
                    debugUi.plot(name.c_str(), h.data(), h.capacity(), h.size(),
                                 h.oldestOffset(), "%.3f ms", 0.0f, FLT_MAX);
                });

                if (renderer.useSvdag() && !renderer.svdag().nodes.empty()) {
                    debugUi.separatorText("SVDAG (live)");
                    const Svdag &sv = renderer.svdag();
                    const size_t flatGridBytes =
                        renderer.worldBrickGrid().handles().size() * sizeof(BrickHandle);
                    const double svdagMB = static_cast<double>(svdagBytes(sv)) / (1024.0 * 1024.0);
                    const double flatGridMB =
                        static_cast<double>(flatGridBytes) / (1024.0 * 1024.0);
                    const double svdagSavedMB = flatGridMB - svdagMB;
                    const double svdagSavedPct =
                        flatGridMB > 0.0 ? (svdagSavedMB / flatGridMB) * 100.0 : 0.0;
                    debugUi.text("Nodes:          %zu", sv.nodes.size());
                    debugUi.text("Levels:         %d (paddedDim=%d)", sv.levels, sv.paddedDim);
                    debugUi.text("SVDAG size:     %.3f MB", svdagMB);
                    debugUi.text("Flat grid:      %.3f MB", flatGridMB);
                    debugUi.text("Saved vs grid:  %.3f MB (%.0f%%)", svdagSavedMB, svdagSavedPct);
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
                gpuMemoryMBHistory.push(static_cast<float>(allocator.stats().usedBytes)
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
                    fpsLastUpdate = now;
                    fpsAccumDt = 0.0;
                    fpsAccumFrames = 0;
                }

                controller.applyScrollDelta(debugUi.scrollDelta());
                controller.update(dt);

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
                renderer.render(root, camera, FrameExtent{ .width = w, .height = h });
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
