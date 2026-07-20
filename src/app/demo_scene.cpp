#include "demo_scene.hpp"

#include <array>
#include <numbers>
#include "math/color.hpp"
#include "math/quat.hpp"
#include "shared/profiling.hpp"

namespace RAGE::App {
    DemoScene::DemoScene(Node3D &root, Toolkit::CollisionWorld &collider,
                         std::shared_ptr<Material<VulkanShaderModule>> material,
                         float voxelSize)
        : root_(root)
        , collider_(collider)
        , material_(std::move(material))
        , voxelSize_(voxelSize) {}

    void DemoScene::build(AsyncVoxLoader &loader, const std::filesystem::path &assetsDir) {
        addSpinners_();
        addFallingProps_();
        // The .vox statue rides the streamed scene as a free-standing volume.
        auto statue = loader.stage(assetsDir / "sphere.vox", voxelSize_);
        statue->setMaterial(material_);
        statue->setPosition(Vec3(4.0f, 6.0f, -6.0f));
        root_.add(std::move(statue));
    }

    // T1 demo: free-standing (full-TRS) volumes spinning above the terrain.
    void DemoScene::addSpinners_() {
        constexpr int32_t kSpinnerDim = 24;
        const std::array<uint32_t, 3> palette{ 0xFF4A6FE3u, 0xFF3DB57Bu, 0xFFD1583Fu };
        for (size_t i = 0; i < palette.size(); ++i) {
            auto v = std::make_unique<Voxel3D>(IVec3{ kSpinnerDim, kSpinnerDim, kSpinnerDim },
                                               voxelSize_);
            const uint32_t color = palette[i];
            v->fill([color](IVec3 c) {
                const bool checker = (((c.x / 6) + (c.y / 6) + (c.z / 6)) & 1) == 0;
                return Color::fromRGBA8(checker ? color : 0xFFE8E4DCu);
            });
            v->setMaterial(material_);
            v->setPosition(Vec3(-3.0f + (3.0f * static_cast<float>(i)), 3.0f, -4.0f));
            spinners_.push_back(v.get());
            root_.add(std::move(v));
            collider_.add(*spinners_.back());
        }
    }

    // T5 demo: falling free-standing props with distinct masses — walk into the
    // light one and bulldoze it; the heavy one barely budges. Body boxes are
    // bottom-center based while Voxel3D renders corner-origin, so collision sits
    // half a cube off in XZ — known v1 slop.
    void DemoScene::addFallingProps_() {
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
            auto v = std::make_unique<Voxel3D>(IVec3{ kPropDim, kPropDim, kPropDim },
                                               voxelSize_);
            v->fillSolid(Color::fromRGBA8(specs[i].color));
            v->setMaterial(material_);
            v->setPosition(Vec3(1.8f + (0.9f * static_cast<float>(i)),
                                5.0f + (0.8f * static_cast<float>(i)), 1.8f));
            Voxel3D *raw = v.get();
            root_.add(std::move(v));
            const Toolkit::KinematicBodyConfig propCfg{
                .size = Vec3(0.6f, 0.6f, 0.6f),
                .gravity = 22.0f,
                .terminalSpeed = 50.0f,
                .jumpSpeed = 0.0f,
                .stepUpHeight = 0.0f,
                .mass = specs[i].mass,
            };
            props_.push_back(Prop{
                .volume = raw,
                .body = std::make_unique<Toolkit::KinematicBody>(*raw, propCfg, raw),
            });
            collider_.add(*props_.back().body);
        }
    }

    void DemoScene::update(float dt, double now) {
        for (size_t i = 0; i < spinners_.size(); ++i) {
            const float speed = 0.4f + (0.3f * static_cast<float>(i));
            constexpr float kInvSqrt3 = std::numbers::inv_sqrt3_v<float>;
            const Vec3 axis = (i % 2 == 0) ? Vec3(0.0f, 1.0f, 0.0f)
                                           : Vec3(kInvSqrt3, kInvSqrt3, kInvSqrt3);
            spinners_[i]->setRotation(
                Quat::fromAxisAngle(axis, static_cast<float>(now) * speed));
        }
        const Core::ProfileZone physicsZone("Physics.Bodies");
        for (Prop &prop : props_) {
            prop.body->update(Toolkit::MoveInput{}, dt);
        }
    }
}
