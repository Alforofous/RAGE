#pragma once

#include <functional>
#include "engine/scene/camera.hpp"
#include "engine/scene/node3d.hpp"
#include "engine/toolkit/entity/kinematic_body.hpp"
#include "free_fly_controller.hpp"

namespace RAGE::App {
    class Window;

    /**
     * @brief Unified player input: fly mode (FreeFlyController, full 6-DOF) and
     *        walk mode (WASD + jump through the KinematicBody), toggled with V.
     *        The camera is a child of the player entity: in fly mode the entity
     *        rests at origin so camera local == world; toggling to walk moves the
     *        entity under the camera and parks the camera at eye height.
     *
     * UI focus vetoes (ImGui capturing mouse/keyboard) are injected as callbacks
     * so this class stays ignorant of the debug UI.
     */
    class PlayerController {
    public:
        struct Config {
            float eyeHeight = 1.62f;
            float walkSpeed = 4.0f;
        };

        PlayerController(Window &window, Camera &camera, Node3D &playerEntity,
                         Toolkit::KinematicBody &body, Config config);
        /// Defaults-config overload (a default argument can't see Config's NSDMIs here).
        PlayerController(Window &window, Camera &camera, Node3D &playerEntity,
                         Toolkit::KinematicBody &body);

        PlayerController(const PlayerController &) = delete;
        PlayerController &operator=(const PlayerController &) = delete;
        PlayerController(PlayerController &&) = delete;
        PlayerController &operator=(PlayerController &&) = delete;

        /// Wire UI focus: while either returns true, the matching input is ignored.
        void setUiFocus(std::function<bool()> wantsMouse, std::function<bool()> wantsKeyboard);
        /// Per-frame scroll delta source (fly-mode speed adjust); walk mode ignores it.
        void setScrollSource(std::function<float()> scrollDelta);

        /// Poll input, handle the walk/fly toggle, and advance whichever mode is live.
        void update(float dt);

        bool walkMode() const { return walkMode_; }
        bool grounded() const { return body_.grounded(); }
        float speedMultiplier() const { return fly_.speedMultiplier(); }
        void setSpeedMultiplier(float m) { fly_.setSpeedMultiplier(m); }

    private:
        bool uiWantsMouse_() const { return wantsMouse_ && wantsMouse_(); }
        bool uiWantsKeyboard_() const { return wantsKeyboard_ && wantsKeyboard_(); }
        void toggleWalkMode_();
        void updateWalk_(float dt);

        Window &window_;
        Camera &camera_;
        Node3D &playerEntity_;
        Toolkit::KinematicBody &body_;
        Config config_;
        FreeFlyController fly_;
        std::function<bool()> wantsMouse_;
        std::function<bool()> wantsKeyboard_;
        std::function<float()> scrollDelta_;
        bool walkMode_ = false;
        bool prevWalkKey_ = false;
    };
}
