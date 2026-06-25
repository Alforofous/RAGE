#include "free_fly_controller.hpp"
#include <algorithm>
#include <numbers>
#include <GLFW/glfw3.h>
#include "math/quat.hpp"
#include "math/vec.hpp"

namespace RAGE::App {
    FreeFlyController::FreeFlyController(Camera &camera, Window &window, float moveSpeed,
                                         float mouseSensitivity)
        : camera_(camera)
        , window_(window)
        , moveSpeed_(moveSpeed)
        , mouseSensitivity_(mouseSensitivity) {}

    void FreeFlyController::captureMouse() {
        GLFWwindow *w = window_.glfwHandle();
        if (w == nullptr || mouseCaptured_) {
            return;
        }
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwGetCursorPos(w, &lastMouseX_, &lastMouseY_);
        mouseCaptured_ = true;
    }

    void FreeFlyController::releaseMouse() {
        GLFWwindow *w = window_.glfwHandle();
        if (w == nullptr || !mouseCaptured_) {
            return;
        }
        glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        mouseCaptured_ = false;
    }

    void FreeFlyController::update(float deltaSeconds) {
        GLFWwindow *w = window_.glfwHandle();
        if (w == nullptr) {
            return;
        }

        const auto pressed = [w](int key) -> bool { return glfwGetKey(w, key) == GLFW_PRESS; };

        if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            captureMouse();
        }
        if (pressed(GLFW_KEY_ESCAPE)) {
            releaseMouse();
        }

        if (mouseCaptured_) {
            double mx = 0.0;
            double my = 0.0;
            glfwGetCursorPos(w, &mx, &my);
            const auto dx = static_cast<float>(mx - lastMouseX_);
            const auto dy = static_cast<float>(my - lastMouseY_);
            lastMouseX_ = mx;
            lastMouseY_ = my;

            yaw_ -= dx * mouseSensitivity_;
            pitch_ -= dy * mouseSensitivity_;
        }

        const float pitchLimit = (std::numbers::pi_v<float> * 0.5f) - 0.01f;
        pitch_ = std::clamp(pitch_, -pitchLimit, pitchLimit);

        const Quat yawQuat = Quat::fromAxisAngle(Vec3::unitY(), yaw_);
        const Quat pitchQuat = Quat::fromAxisAngle(Vec3::unitX(), pitch_);
        camera_.setRotation(yawQuat * pitchQuat);

        Vec3 local = Vec3::zero();
        if (pressed(GLFW_KEY_W)) {
            local.z -= 1.0f;
        }
        if (pressed(GLFW_KEY_S)) {
            local.z += 1.0f;
        }
        if (pressed(GLFW_KEY_A)) {
            local.x -= 1.0f;
        }
        if (pressed(GLFW_KEY_D)) {
            local.x += 1.0f;
        }
        if (pressed(GLFW_KEY_Q)) {
            local.y -= 1.0f;
        }
        if (pressed(GLFW_KEY_E)) {
            local.y += 1.0f;
        }

        if (local.lengthSquared() > 0.0f) {
            const Vec3 worldDelta = camera_.rotation().rotate(local.normalized()) * (moveSpeed_ * deltaSeconds);
            camera_.setPosition(camera_.position() + worldDelta);
        }
    }
}
