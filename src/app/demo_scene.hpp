#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include "async_vox_loader.hpp"
#include "engine/materials/material.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/scene/voxel3d.hpp"
#include "engine/toolkit/collision/collision_world.hpp"
#include "engine/toolkit/entity/kinematic_body.hpp"
#include "gpu/vulkan/vulkan_shader_module.hpp"

namespace RAGE::App {
    /**
     * @brief The demo's game content: spinning checker cubes (T1), falling props
     *        with distinct masses (T5), and the .vox statue. This is what a real
     *        game replaces — main only constructs, builds, and ticks it.
     */
    class DemoScene {
    public:
        DemoScene(Node3D &root, Toolkit::CollisionWorld &collider,
                  std::shared_ptr<Material<VulkanShaderModule>> material, float voxelSize);

        DemoScene(const DemoScene &) = delete;
        DemoScene &operator=(const DemoScene &) = delete;
        DemoScene(DemoScene &&) = delete;
        DemoScene &operator=(DemoScene &&) = delete;

        /// Create the content: cubes + props immediately, the statue staged on `loader`.
        void build(AsyncVoxLoader &loader, const std::filesystem::path &assetsDir);

        /// Per-frame: spin the cubes (absolute angle from `now`) and tick prop physics.
        void update(float dt, double now);

    private:
        void addSpinners_();
        void addFallingProps_();

        Node3D &root_;
        Toolkit::CollisionWorld &collider_;
        std::shared_ptr<Material<VulkanShaderModule>> material_;
        float voxelSize_ = 1.0f;

        std::vector<Voxel3D *> spinners_;
        struct Prop {
            Voxel3D *volume;
            std::unique_ptr<Toolkit::KinematicBody> body;
        };
        std::vector<Prop> props_;
    };
}
