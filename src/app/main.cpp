#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
#include "profiler.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/directional_light.hpp"
#include "engine/scene/node3d.hpp"
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
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--profile") == 0) {
            autoLaunchTracy = true;
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
            root.add(stageVoxelFromFile(assetsDir / "floor.vox", Vec3(-6.4f, -7.0f, -22.4f)));
            root.add(stageVoxelFromFile(assetsDir / "sphere.vox", Vec3(-6.4f, -6.4f, -22.4f)));

            std::atomic<bool> loaderDone{ false };
            App::Profiler profiler;
            profiler.attach(renderer);
            if (autoLaunchTracy && App::Profiler::isLinked()) {
                profiler.launchProfilerGui();
                // Wait for Tracy's on-demand connection so early app work (asset load,
                // first-frame setup) lands on the timeline instead of being dropped.
                // Timeout-bounded so a missing/broken Tracy install never hangs startup.
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (!profiler.isConnected() && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }

            std::atomic<bool> loaderCancel{ false };
            for (const auto &job : loadJobs) {
                job->target->onLoadShouldCancel([&loaderCancel]() { return loaderCancel.load(); });
            }
            std::thread loaderThread([&]() {
                profiler.setThreadName("AssetLoader");
                App::Profiler::Zone outerZone(profiler, "LoaderThread");
                for (const auto &job : loadJobs) {
                    if (loaderCancel.load()) {
                        break;
                    }
                    App::Profiler::Zone perFileZone(profiler, "VoxelData::fillFromPackedRGBA8");
                    job->target->fillFromPackedRGBA8(job->model.voxels.data(), job->model.dims);
                    job->model.voxels.clear();
                    job->model.voxels.shrink_to_fit();
                }
                loaderDone.store(true);
            });

            App::DebugUi debugUi(ctx, window.glfwHandle());
            debugUi.attach(renderer);

            App::FreeFlyController controller(camera, window);
            controller.setMouseVeto([&debugUi]() { return debugUi.wantsMouse(); });
            controller.setKeyboardVeto([&debugUi]() { return debugUi.wantsKeyboard(); });

            bool mipSkipEnabled = renderer.mipSkipEnabled();
            int heatmapMode = renderer.heatmapMode();
            int heatmapMaxSteps = renderer.heatmapMaxSteps();
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
                const auto bricksAllocated = renderer.brickPool().allocated();
                const double poolMB = static_cast<double>(renderer.brickPool().allocatedBytes())
                                      / (1024.0 * 1024.0);
                size_t handleGridBytes = 0;
                size_t denseBytes = 0;
                for (const auto &job : loadJobs) {
                    if (const VoxelData *vd = job->target->voxelData()) {
                        handleGridBytes += vd->handleGridBytes();
                        denseBytes += vd->denseEquivalentBytes();
                    }
                }
                const double handleGridMB = static_cast<double>(handleGridBytes) / (1024.0 * 1024.0);
                const double brickmapTotalMB = poolMB + handleGridMB;
                const double denseMB = static_cast<double>(denseBytes) / (1024.0 * 1024.0);
                const double savingsMB = denseMB - brickmapTotalMB;
                const double savingsPct = denseMB > 0.0 ? (savingsMB / denseMB) * 100.0 : 0.0;

                debugUi.text("Brickmap (sparse)");
                debugUi.text("  Bricks: %zu / %zu", bricksAllocated, BrickPool::kMaxBricks);
                debugUi.text("  Pool:         %.2f MB", poolMB);
                debugUi.text("  Handle grids: %.2f MB", handleGridMB);
                debugUi.text("  Total:        %.2f MB", brickmapTotalMB);
                debugUi.text("If dense (per-Voxel3D)");
                debugUi.text("  Total:        %.2f MB", denseMB);
                debugUi.text("Sparsity saved: %.2f MB (%.1f%%)", savingsMB, savingsPct);

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

                renderer.render(root, camera, FrameExtent{ .width = w, .height = h });

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
