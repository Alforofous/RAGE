#pragma once

#include "engine/scene/node3d.hpp"
#include "math/color.hpp"
#include "math/vec.hpp"

namespace RAGE {
    /**
     * A directional light — sun-style infinitely-distant illumination.
     *
     * Inherits Node3D so it lives in the scene graph: parenting it to a moving node (e.g., a
     * day/night-cycle pivot) animates the light's direction for free. The light's "facing"
     * direction is its world transform applied to the local forward axis (-Z), matching the
     * Camera convention.
     *
     * Convention: `direction` arguments are the **direction the light is travelling**, not the
     * direction toward the light. Shading flips this to a "to-light" vector internally.
     *
     * @note A renderer can hold a fixed number of directional lights per frame
     *       (kMaxDirectionalLights in frame_uniforms.hpp). Lights past that count are dropped
     *       silently from the frame UBO.
     */
    class DirectionalLight : public Node3D {
    public:
        DirectionalLight() = default;
        DirectionalLight(Vec3 direction, Color color, float intensity = 1.0f);

        void setDirection(Vec3 direction);
        Vec3 worldDirection() const;

        Color color() const { return color_; }
        void setColor(Color c) { color_ = c; }

        float intensity() const { return intensity_; }
        void setIntensity(float i) { intensity_ = i; }

    private:
        Color color_ = Color::white();
        float intensity_ = 1.0f;
    };
}
