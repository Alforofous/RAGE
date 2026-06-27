#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
    bool launchProfiler = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--profile") == 0) {
            launchProfiler = true;
        }
    }

    if (launchProfiler) {
        const char *tracyExe = RAGE::App::kTracyServerExePath;
        if (tracyExe == nullptr || tracyExe[0] == '\0') {
            std::fprintf(stderr,
                         "--profile: profiling support not built. Reconfigure with "
                         "-DRAGE_ENABLE_PROFILING=ON.\n");
        } else {
            std::string spawn = "start \"\" \"";
            spawn += tracyExe;
            spawn += "\" -a 127.0.0.1";
            std::fprintf(stdout, "--profile: launching %s\n", tracyExe);
            std::fflush(stdout);
            std::system(spawn.c_str());
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
            std::atomic<float> mipProgress{ 0.0f };
        };
        std::vector<std::unique_ptr<VoxelLoadJob>> loadJobs;

        const auto stageVoxelFromFile = [&](const std::filesystem::path &voxPath, Vec3 position) {
            auto job = std::make_unique<VoxelLoadJob>();
            job->label = voxPath.filename().string();
            job->model = Content::loadVox(voxPath);
            auto v = std::make_unique<Voxel3D>(allocator, job->model.dims, kVoxelSize);
            job->target = v.get();
            v->setMaterial(voxelMaterial);
            v->setPosition(position);
            v->onMipBuildProgress([&jobRef = *job](float p) { jobRef.mipProgress.store(p); });
            std::fprintf(stdout, "Staged %s (%dx%dx%d)\n", voxPath.string().c_str(), job->model.dims.x,
                         job->model.dims.y, job->model.dims.z);
            loadJobs.push_back(std::move(job));
            return v;
        };

        Node3D root;
        root.add(stageVoxelFromFile(assetsDir / "floor.vox", Vec3(-6.4f, -7.0f, -22.4f)));
        root.add(stageVoxelFromFile(assetsDir / "sphere.vox", Vec3(-6.4f, -6.4f, -22.4f)));

        std::atomic<bool> loaderDone{ false };
        std::atomic<bool> loaderCancel{ false };
        for (const auto &job : loadJobs) {
            job->target->onMipBuildShouldCancel([&loaderCancel]() { return loaderCancel.load(); });
        }
        std::thread loaderThread([&]() {
            for (const auto &job : loadJobs) {
                if (loaderCancel.load()) {
                    break;
                }
                job->target->fillFromPackedRGBA8(job->model.voxels.data(), job->model.dims);
                job->model.voxels.clear();
                job->model.voxels.shrink_to_fit();
            }
            loaderDone.store(true);
        });

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

            App::Profiler profiler;
            profiler.attach(renderer);

            auto sun = std::make_shared<DirectionalLight>(Vec3(-1.0f, -1.0f, -1.0f),
                                                          Color(1.0f, 0.95f, 0.85f), 1.2f);
            renderer.addLight(sun);
            renderer.setAmbientLight({ .color = Color(0.4f, 0.5f, 0.8f), .intensity = 0.25f });

            App::FreeFlyController controller(camera, window);

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
                    fpsLastUpdate = now;
                    fpsAccumDt = 0.0;
                    fpsAccumFrames = 0;
                }

                controller.update(dt);

                const auto [w, h] = window.framebufferExtent();
                window.clearResized();
                if (h > 0) {
                    camera.setAspect(static_cast<float>(w) / static_cast<float>(h));
                }

                GLFWwindow *gw = window.glfwHandle();
                const bool rightDown = (glfwGetMouseButton(gw, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
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
