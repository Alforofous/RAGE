#include "player_controller.hpp"

#include <GLFW/glfw3.h>
#include "shared/profiling.hpp"
#include "window.hpp"

namespace RAGE::App {
    PlayerController::PlayerController(Window &window, Camera &camera, Node3D &playerEntity,
                                       Toolkit::KinematicBody &body, Config config)
        : window_(window)
        , camera_(camera)
        , playerEntity_(playerEntity)
        , body_(body)
        , config_(config)
        , fly_(camera, window) {
        fly_.setMouseVeto([this]() { return uiWantsMouse_(); });
        fly_.setKeyboardVeto([this]() { return walkMode_ || uiWantsKeyboard_(); });
    }

    PlayerController::PlayerController(Window &window, Camera &camera, Node3D &playerEntity,
                                       Toolkit::KinematicBody &body)
        : PlayerController(window, camera, playerEntity, body, Config{}) {}

    void PlayerController::setUiFocus(std::function<bool()> wantsMouse,
                                      std::function<bool()> wantsKeyboard) {
        wantsMouse_ = std::move(wantsMouse);
        wantsKeyboard_ = std::move(wantsKeyboard);
    }

    void PlayerController::setScrollSource(std::function<float()> scrollDelta) {
        scrollDelta_ = std::move(scrollDelta);
    }

    void PlayerController::update(float dt) {
        const bool walkKey = !uiWantsKeyboard_()
                             && glfwGetKey(window_.glfwHandle(), GLFW_KEY_V) == GLFW_PRESS;
        if (walkKey && !prevWalkKey_) {
            toggleWalkMode_();
        }
        prevWalkKey_ = walkKey;

        if (!walkMode_ && scrollDelta_) {
            fly_.applyScrollDelta(scrollDelta_());
        }
        fly_.update(dt);

        if (walkMode_) {
            updateWalk_(dt);
        }
    }

    void PlayerController::toggleWalkMode_() {
        walkMode_ = !walkMode_;
        const Vec3 camWorld = camera_.worldMatrix().transformPoint(Vec3::zero());
        if (walkMode_) {
            playerEntity_.setPosition(camWorld - Vec3(0.0f, config_.eyeHeight, 0.0f));
            camera_.setPosition(Vec3(0.0f, config_.eyeHeight, 0.0f));
        } else {
            playerEntity_.setPosition(Vec3::zero());
            camera_.setPosition(camWorld);
        }
    }

    void PlayerController::updateWalk_(float dt) {
        GLFWwindow *gw = window_.glfwHandle();
        const Mat4 camWorld = camera_.worldMatrix();
        Vec3 fwd = camWorld.transformDirection(Vec3(0.0f, 0.0f, -1.0f));
        fwd.y = 0.0f;
        Vec3 right = camWorld.transformDirection(Vec3(1.0f, 0.0f, 0.0f));
        right.y = 0.0f;
        Vec3 walk(0.0f, 0.0f, 0.0f);
        if (!uiWantsKeyboard_()) {
            float f = 0.0f;
            float r = 0.0f;
            if (glfwGetKey(gw, GLFW_KEY_W) == GLFW_PRESS) { f += 1.0f; }
            if (glfwGetKey(gw, GLFW_KEY_S) == GLFW_PRESS) { f -= 1.0f; }
            if (glfwGetKey(gw, GLFW_KEY_D) == GLFW_PRESS) { r += 1.0f; }
            if (glfwGetKey(gw, GLFW_KEY_A) == GLFW_PRESS) { r -= 1.0f; }
            const Vec3 dir = (fwd * f) + (right * r);
            if (dir.length() > 1e-3f) {
                walk = dir.normalized() * config_.walkSpeed;
            }
        }
        const Toolkit::MoveInput moveIn{
            .walk = walk,
            .jump = !uiWantsKeyboard_() && glfwGetKey(gw, GLFW_KEY_SPACE) == GLFW_PRESS,
        };
        const Core::ProfileZone playerZone("Physics.Player");
        body_.update(moveIn, dt);
    }
}
