#pragma once

#include <functional>
#include "engine/scene/camera.hpp"
#include "window.hpp"

namespace RAGE::App {
    /**
     * A free-fly first-person camera controller.
     *
     * Polls GLFW key state and mouse motion each tick and applies movement / yaw / pitch to a
     * RAGE::Camera. Lives in the app layer because the engine intentionally knows nothing about
     * input devices.
     *
     * Controls:
     *   W / S         — forward / backward (along camera-local -Z)
     *   A / D         — left / right       (along camera-local +X)
     *   E / Q         — up / down          (along camera-local +Y)
     *   Left-click    — capture cursor and start mouse-look
     *   Esc           — release cursor and pause mouse-look
     *   Mouse         — yaw + pitch (only while captured)
     *
     * Pitch is clamped to roughly ±89° to avoid gimbal flip. Rotation is composed as
     * yawQuat * pitchQuat so pitch acts around the camera's local X regardless of yaw.
     */
    class FreeFlyController {
    public:
        FreeFlyController(Camera &camera, Window &window, float moveSpeed = 2.0f,
                          float mouseSensitivity = 0.0025f);

        void update(float deltaSeconds);

        void setMoveSpeed(float v) { moveSpeed_ = v; }
        void setMouseSensitivity(float v) { mouseSensitivity_ = v; }

        /**
         * Apply a one-frame mouse-scroll delta as a speed-multiplier adjustment, but only
         * while mouse-look is active (so users don't get camera speed flipping while
         * scrolling debug UI). Multiplier ratchets exponentially per scroll tick and is
         * clamped to [0.1, 10] so a couple of accidental scrolls don't pin it to one end.
         */
        void applyScrollDelta(float delta);

        float speedMultiplier() const { return speedMultiplier_; }
        void setSpeedMultiplier(float v);

        // Predicates the app installs so other layers (e.g. a debug UI hovering over a panel)
        // can veto input each frame. Default-empty predicates mean "input is never vetoed".
        // The controller knows nothing about who answers — keeps this class UI-library-free.
        using Predicate = std::function<bool()>;
        void setMouseVeto(Predicate p) { mouseVeto_ = std::move(p); }
        void setKeyboardVeto(Predicate p) { keyboardVeto_ = std::move(p); }

    private:
        void captureMouse();
        void releaseMouse();

        Camera &camera_;
        Window &window_;
        float moveSpeed_ = 2.0f;
        float mouseSensitivity_ = 0.0025f;
        float yaw_ = 0.0f;
        float pitch_ = 0.0f;
        double lastMouseX_ = 0.0;
        double lastMouseY_ = 0.0;
        bool mouseCaptured_ = false;
        float speedMultiplier_ = 1.0f;
        Predicate mouseVeto_;
        Predicate keyboardVeto_;
    };
}
