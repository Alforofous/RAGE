#include "directional_light.hpp"
#include <cmath>
#include <numbers>
#include "math/quat.hpp"

namespace RAGE {
    DirectionalLight::DirectionalLight(Vec3 direction, Color color, float intensity)
        : color_(color)
        , intensity_(intensity) {
        setDirection(direction);
    }

    void DirectionalLight::setDirection(Vec3 direction) {
        const Vec3 from(0.0f, 0.0f, -1.0f);
        const Vec3 to = direction.normalized();
        const float dot = from.dot(to);

        if (dot > 0.99999f) {
            setRotation(Quat::identity());
            return;
        }
        if (dot < -0.99999f) {
            setRotation(Quat::fromAxisAngle(Vec3::unitY(), std::numbers::pi_v<float>));
            return;
        }

        const Vec3 axis = from.cross(to).normalized();
        const float angle = std::acos(dot);
        setRotation(Quat::fromAxisAngle(axis, angle));
    }

    Vec3 DirectionalLight::worldDirection() const {
        return worldMatrix().transformDirection(Vec3(0.0f, 0.0f, -1.0f)).normalized();
    }
}
