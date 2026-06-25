#pragma once

#include "math/color.hpp"

namespace RAGE {
    /**
     * Scene-global ambient light.
     *
     * Not a scene-graph node — has no position or direction. Represents a constant fill term
     * applied uniformly to every surface. Held by the Renderer (set via setAmbientLight) and
     * written into the frame UBO each frame.
     *
     * Default is a faint cool fill (0.05 intensity), enough to keep shadowed surfaces from
     * being pitch black without overpowering directional lights.
     */
    struct AmbientLight {
        Color color = Color::white();
        float intensity = 0.05f;
    };
}
