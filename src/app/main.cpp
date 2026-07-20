#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <numbers>
#include <GLFW/glfw3.h>
#include "async_vox_loader.hpp"
#include "debug_panel.hpp"
#include "demo_scene.hpp"
#include "engine/toolkit/content/chunk_generators.hpp"
#include "engine/toolkit/content/chunk_store_factory.hpp"
#include "engine/toolkit/content/chunk_streamer.hpp"
#include "engine/toolkit/entity/kinematic_body.hpp"
#include "engine/toolkit/pipeline/voxel_pipeline.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/directional_light.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/voxel3d.hpp"
#include "math/color.hpp"
#include "math/vec.hpp"
#include "options.hpp"
#include "player_controller.hpp"
#include "profiler.hpp"
#include "shared/profiling.hpp"
#include "window.hpp"
#include "world_config.hpp"

namespace {
    using namespace RAGE;
}

int main(int argc, char **argv) {
    const App::Options opts = App::Options::parse(argc, argv);

    // Tracy's on-demand mode only records what happens after the GUI connects,
    // so launch + wait BEFORE the engine allocates anything.
    App::Profiler profiler;
    if (opts.profile) {
        profiler.launchGuiAndWait();
    }

    try {
        App::Window window(1280, 720, "RAGE Smoke");

        const App::WorldPipelineConfig kWorld{};
        constexpr float kVoxelSize = 0.05f;
        Toolkit::VoxelPipeline pipeline(window.vulkanSurfaceSource(),
                                        Toolkit::VoxelPipelineSettings{
                                            .appName = "RAGE Smoke",
                                            .shaderDir = App::executableDir(argv[0]) / "shaders",
                                            .vsync = opts.vsync,
                                            .limits = kWorld.rendererLimits(true),
                                        });

        // The player: a standalone entity carrying the camera at its head. It is
        // deliberately NOT in the render root — nothing draws it, and driving it
        // every frame must not dirty the scene.
        Node3D playerEntity;
        const auto [initW, initH] = window.framebufferExtent();
        Camera &camera = playerEntity.add<Camera>(
            std::numbers::pi_v<float> * 0.4f,
            static_cast<float>(initW) / static_cast<float>(initH > 0 ? initH : 1), 0.1f, 100.0f);
        camera.setPosition(Vec3(0.0f, 4.0f, 8.0f));

        // The scene: one windowed world Voxel3D (the streamer slides its storage
        // window and fills it; the renderer marches it; collision reads it) plus
        // the demo content.
        Node3D root;
        Voxel3D &world = root.add<Voxel3D>(kWorld.worldVoxelDims(), kVoxelSize);
        world.setMaterial(pipeline.defaultVoxelMaterial());

        Toolkit::CollisionWorld collider(*world.voxelData(), pipeline.renderer().brickPool(),
                                         kVoxelSize);

        auto gen = std::make_unique<Toolkit::Content::TerrainChunkGenerator>();
        if (gen->chunkBrickDims() != kWorld.chunkBrickDims) {
            throw std::runtime_error(
                "WorldPipelineConfig.chunkBrickDims does not match the terrain generator");
        }
        const auto chunkStore = Toolkit::Content::chunkStore(
            std::move(gen), pipeline.renderer().brickPool(), kVoxelSize, kWorld.yRange,
            opts.worldDir);
        Toolkit::Content::ChunkStreamer streamer(*chunkStore, world, kWorld.streamRadius);

        App::AsyncVoxLoader loader;
        App::DemoScene demo(root, collider, pipeline.defaultVoxelMaterial(), kVoxelSize);
        demo.build(loader, App::executableDir(argv[0]) / "assets");
        loader.start();

        Toolkit::KinematicBody playerBody(playerEntity, Toolkit::KinematicBodyConfig{});
        collider.add(playerBody);   // the player is just another collidable

        App::PlayerController player(window, camera, playerEntity, playerBody);
        App::DebugPanel debug(pipeline, window, profiler, loader, player, root, streamer,
                              App::DebugPanel::StreamInfo{ .hRadius = kWorld.streamRadius,
                                                           .yRange = kWorld.yRange });
        player.setUiFocus([&debug]() { return debug.wantsMouse(); },
                          [&debug]() { return debug.wantsKeyboard(); });
        player.setScrollSource([&debug]() { return debug.scrollDelta(); });

        pipeline.renderer().addLight(std::make_shared<DirectionalLight>(
            Vec3(-1.0f, -1.0f, -1.0f), Color(1.0f, 0.95f, 0.85f), 1.2f));
        pipeline.renderer().setAmbientLight(
            { .color = Color(0.4f, 0.5f, 0.8f), .intensity = 0.25f });

        double lastTime = glfwGetTime();
        while (!window.shouldClose()) {
            window.pollEvents();
            const double now = glfwGetTime();
            const auto dt = static_cast<float>(now - lastTime);
            lastTime = now;

            debug.frame(dt);
            player.update(dt);
            demo.update(dt, now);
            {
                const Core::ProfileZone streamerZone("ChunkStreamer.Update");
                streamer.update(camera.worldMatrix().transformPoint(Vec3::zero()));
            }

            const auto [w, h] = window.framebufferExtent();
            window.clearResized();
            if (h > 0) {
                camera.setAspect(static_cast<float>(w) / static_cast<float>(h));
            }

            const double renderStart = glfwGetTime();
            pipeline.render(root, camera);
            debug.pushRenderMs(static_cast<float>((glfwGetTime() - renderStart) * 1000.0));
        }
        loader.cancelAndJoin();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());

        return 1;
    }

    return 0;
}
