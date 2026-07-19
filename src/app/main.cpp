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
#include "async_vox_loader.hpp"
#include "debug_panel.hpp"
#include "engine/toolkit/content/chunk_generators.hpp"
#include "engine/toolkit/content/chunk_store_factory.hpp"
#include "engine/toolkit/content/chunk_streamer.hpp"
#include "engine/toolkit/entity/kinematic_body.hpp"
#include "engine/materials/material.hpp"
#include "engine/rendering/renderer.hpp"
#include "profiler.hpp"
#include "shared/profiling.hpp"
#include "engine/scene/camera.hpp"
#include "engine/scene/directional_light.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/voxel3d.hpp"
#include "engine/toolkit/pipeline/voxel_pipeline.hpp"
#include "player_controller.hpp"
#include "math/color.hpp"
#include "math/ivec.hpp"
#include "math/vec.hpp"
#include "window.hpp"
#include "world_config.hpp"

namespace {
    using namespace RAGE;
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

        const App::WorldPipelineConfig kWorld{};
        Toolkit::VoxelPipeline pipeline(window.vulkanSurfaceSource(),
                                        Toolkit::VoxelPipelineSettings{
                                            .appName = "RAGE Smoke",
                                            .shaderDir = App::executableDir(argv[0]) / "shaders",
                                            .vsync = vsync,
                                            .limits = kWorld.rendererLimits(true),
                                        });
        const auto voxelMaterial = pipeline.defaultVoxelMaterial();

        constexpr float kVoxelSize = 0.05f;

        const std::filesystem::path assetsDir = App::executableDir(argv[0]) / "assets";

        App::AsyncVoxLoader loader;

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

            Node3D root;

            // The world: one windowed Voxel3D. The streamer slides its storage
            // window and fills it; the renderer marches it; collision reads it.
            Voxel3D &world =
                root.add<Voxel3D>(renderer.brickPool(), kWorld.worldVoxelDims(), kVoxelSize);
            world.setMaterial(voxelMaterial);

            Toolkit::CollisionWorld collisionWorld(*world.voxelData(), renderer.brickPool(),
                                                   kVoxelSize);

            std::unique_ptr<Toolkit::Content::ChunkStore> chunkStore;
            std::optional<Toolkit::Content::ChunkStreamer> streamer;
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
                    v->setPosition(Vec3(-3.0f + (3.0f * static_cast<float>(i)), 3.0f, -4.0f));
                    spinners.push_back(v.get());
                    root.add(std::move(v));
                    collisionWorld.add(*spinners.back());
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
                        .body = std::make_unique<Toolkit::KinematicBody>(*raw, propCfg, raw),
                    });
                    collisionWorld.add(*props.back().body);
                }
            };

            const auto buildScene = [&]() {
                auto gen = std::make_unique<Toolkit::Content::TerrainChunkGenerator>();
                if (gen->chunkBrickDims() != kWorld.chunkBrickDims) {
                    throw std::runtime_error(
                        "WorldPipelineConfig.chunkBrickDims does not match the terrain generator");
                }
                chunkStore = Toolkit::Content::chunkStore(std::move(gen), renderer.brickPool(),
                                                          kVoxelSize, kTerrainYRange, worldDir);
                streamer.emplace(*chunkStore, world);
                addSpinners();
                addFallingProps();
                // The .vox statue rides the streamed scene as a free-standing volume.
                auto statue = loader.stage(assetsDir / "sphere.vox", renderer.brickPool(),
                                           kVoxelSize);
                statue->setMaterial(voxelMaterial);
                statue->setPosition(Vec3(4.0f, 6.0f, -6.0f));
                root.add(std::move(statue));
                camera.setPosition(Vec3(0.0f, 4.0f, 8.0f));
            };
            buildScene();
            loader.start();

            constexpr Toolkit::KinematicBodyConfig kPlayerBody{};
            Toolkit::KinematicBody playerBody(playerEntity, kPlayerBody);
            collisionWorld.add(playerBody);   // the player is just another collidable
            App::PlayerController player(window, camera, playerEntity, playerBody);
            App::DebugPanel debug(pipeline, window, profiler, loader, player, root, streamer,
                                  App::DebugPanel::StreamInfo{ .hRadius = kStreamHRadius,
                                                               .yRange = kTerrainYRange });
            player.setUiFocus([&debug]() { return debug.wantsMouse(); },
                              [&debug]() { return debug.wantsKeyboard(); });
            player.setScrollSource([&debug]() { return debug.scrollDelta(); });

            auto sun = std::make_shared<DirectionalLight>(Vec3(-1.0f, -1.0f, -1.0f),
                                                          Color(1.0f, 0.95f, 0.85f), 1.2f);
            renderer.addLight(sun);
            renderer.setAmbientLight({ .color = Color(0.4f, 0.5f, 0.8f), .intensity = 0.25f });

            double lastTime = glfwGetTime();
            double fpsAccumDt = 0.0;
            uint32_t fpsAccumFrames = 0;
            double fpsLastUpdate = lastTime;
            while (!window.shouldClose()) {
                window.pollEvents();
                const double now = glfwGetTime();
                const auto dt = static_cast<float>(now - lastTime);
                lastTime = now;

                debug.frame(dt);

                player.update(dt);

                for (size_t i = 0; i < spinners.size(); ++i) {
                    const float speed = 0.4f + (0.3f * static_cast<float>(i));
                    constexpr float kInvSqrt3 = std::numbers::inv_sqrt3_v<float>;
                    const Vec3 axis = (i % 2 == 0) ? Vec3(0.0f, 1.0f, 0.0f)
                                                   : Vec3(kInvSqrt3, kInvSqrt3, kInvSqrt3);
                    spinners[i]->setRotation(
                        Quat::fromAxisAngle(axis, static_cast<float>(now) * speed));
                }
                {
                    const Core::ProfileZone physicsZone("Physics.Bodies");
                    for (Prop &prop : props) {
                        prop.body->update(Toolkit::MoveInput{}, dt);
                    }
                }

                if (streamer.has_value()) {
                    const Core::ProfileZone streamerZone("ChunkStreamer.Update");
                    const float chunkWorldExtent =
                        static_cast<float>(chunkStore->chunkBrickDims().x) * 8.0f * kVoxelSize;
                    const Vec3 camPos = camera.worldMatrix().transformPoint(Vec3::zero());
                    const IVec3 focus{
                        static_cast<int32_t>(std::floor(camPos.x / chunkWorldExtent)),
                        static_cast<int32_t>(std::floor(camPos.y / chunkWorldExtent)),
                        static_cast<int32_t>(std::floor(camPos.z / chunkWorldExtent)),
                    };
                    streamer->update(focus, kStreamHRadius);
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
        }
    } catch (const std::exception &e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());

        return 1;
    }

    return 0;
}
