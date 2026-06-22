#include "free_fly_controller.hpp"
#include <GLFW/glfw3.h>
#include "math/quat.hpp"
#include "math/vec.hpp"

namespace RAGE::App {
    FreeFlyController::FreeFlyController(Camera &camera, const Window &window, float moveSpeed, float turnSpeed)
        : camera_(camera)
        , window_(window)
        , moveSpeed_(moveSpeed)
        , turnSpeed_(turnSpeed) {}

    void FreeFlyController::update(float deltaSeconds) {
        GLFWwindow *w = window_.glfwHandle();
        if (w == nullptr) {
            return;
        }

        const auto pressed = [w](int key) -> bool { return glfwGetKey(w, key) == GLFW_PRESS; };

        if (pressed(GLFW_KEY_Q)) {
            yaw_ += turnSpeed_ * deltaSeconds;
        }
        if (pressed(GLFW_KEY_E)) {
            yaw_ -= turnSpeed_ * deltaSeconds;
        }
        camera_.setRotation(Quat::fromAxisAngle(Vec3::unitY(), yaw_));

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
        if (pressed(GLFW_KEY_SPACE)) {
            local.y += 1.0f;
        }
        if (pressed(GLFW_KEY_LEFT_SHIFT)) {
            local.y -= 1.0f;
        }

        if (local.lengthSquared() > 0.0f) {
            const Vec3 worldDelta = camera_.rotation().rotate(local.normalized()) * (moveSpeed_ * deltaSeconds);
            camera_.setPosition(camera_.position() + worldDelta);
        }
    }
}
