#pragma once

#include <cstdint>
#include "math/vec.hpp"

namespace RAGE {
    inline constexpr uint32_t kMaxDirectionalLights = 4;

    /**
     * Uniform-buffer payload written by the renderer once per frame and bound at descriptor
     * set 0, binding 2 of every material.
     *
     * Contains data that is **constant across all renderables in a frame** — camera params,
     * ambient + directional lights, and any future shared frame state. Per-renderable data
     * (transforms, per-object resources) lives in push constants or in descriptor bindings the
     * renderable writes via its prepareFrame hook (binding 1 by convention).
     *
     * Layout matches GLSL std140 — every member is a vec4 / ivec4 so alignment rules collapse
     * to a naturally-aligned 16-byte stride.
     *
     * @note Shader-side declaration must mirror this struct exactly:
     *
     *     layout(set = 0, binding = 2, std140) uniform FrameUniforms {
     *         vec4 cameraPos_fovY;
     *         vec4 cameraForward_aspect;
     *         vec4 cameraUp_unused;
     *         vec4 ambientColor_intensity;
     *         ivec4 dirLightCount_pad;
     *         vec4 dirLightDir_intensity[4];
     *         vec4 dirLightColor[4];
     *         ivec4 debugPixel_pad;
     *         ivec4 debugFlags;   // (mipSkipEnabled, heatmapMode, heatmapMaxSteps, _)
     *     } frame;
     */
    struct FrameUniforms {
        Vec4 cameraPos_fovY;
        Vec4 cameraForward_aspect;
        Vec4 cameraUp_unused;
        Vec4 ambientColor_intensity;
        int32_t dirLightCount = 0;
        int32_t _pad0 = 0;
        int32_t _pad1 = 0;
        int32_t _pad2 = 0;
        Vec4 dirLightDir_intensity[kMaxDirectionalLights];
        Vec4 dirLightColor[kMaxDirectionalLights];
        int32_t debugPixelX = -1;
        int32_t debugPixelY = -1;
        int32_t _pad3 = 0;
        int32_t _pad4 = 0;
        int32_t mipSkipEnabled = 1;
        int32_t heatmapMode = 0;
        int32_t heatmapMaxSteps = 1024;
        int32_t useSvdag = 0;
        int32_t useGridTexture = 0;
        int32_t _pad5 = 0;
        int32_t _pad6 = 0;
        int32_t _pad7 = 0;
    };
    static_assert(sizeof(FrameUniforms) == 256, "FrameUniforms layout must match shader expectation");
}
