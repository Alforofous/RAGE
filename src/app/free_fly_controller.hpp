#pragma once

#include "engine/scene/camera.hpp"
#include "window.hpp"

namespace RAGE::App {
    /**
     * A free-fly first-person camera controller.
     *
     * Polls GLFW key state each tick and applies movement / yaw to a RAGE::Camera. Lives in the
     * app layer because the engine intentionally knows nothing about input devices.
     *
     * Controls:
     *   W / S         — forward / backward (along camera-local -Z)
     *   A / D         — left / right       (along camera-local +X)
     *   Space / Shift — up / down          (along camera-local +Y)
     *   Q / E         — yaw left / right   (rotation around world +Y)
     *
     * No pitch, no mouse — the minimal feel of Unity's free-fly mode without mouse-look. Pitch
     * can be added later (mouse delta + local X-axis quaternion), but the WASD/QE/Space/Shift
     * set already lets you fly around any scene.
     */
    class FreeFlyController {
    public:
        FreeFlyController(Camera &camera, const Window &window, float moveSpeed = 2.0f, float turnSpeed = 1.5f);

        void update(float deltaSeconds);

        void setMoveSpeed(float v) { moveSpeed_ = v; }
        void setTurnSpeed(float v) { turnSpeed_ = v; }

    private:
        Camera &camera_;
        const Window &window_;
        float moveSpeed_ = 2.0f;
        float turnSpeed_ = 1.5f;
        float yaw_ = 0.0f;
    };
}
