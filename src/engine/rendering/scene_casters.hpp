#pragma once

#include <cstdint>
#include "math/mat.hpp"

namespace RAGE {
    inline constexpr uint32_t kMaxSceneCasters = 16;

    /**
     * Per-caster entry in the SceneCasters UBO consumed by the voxel raycast compute shader.
     *
     * Layout mirrors GLSL std140's struct rules: mat4 + ivec4 + vec4 = 64 + 16 + 16 = 96 bytes,
     * naturally 16-byte aligned. Holds everything a shadow-tracing pass needs to march a world-
     * space ray through a Voxel3D: the inverse model matrix (world → caster local), the grid
     * dimensions, and the voxel cell size.
     */
    struct SceneCasterEntry {
        Mat4 invModel;
        int32_t dimsX = 0;
        int32_t dimsY = 0;
        int32_t dimsZ = 0;
        int32_t _pad0 = 0;
        float voxelSize = 0.0f;
        float _pad1 = 0.0f;
        float _pad2 = 0.0f;
        float _pad3 = 0.0f;
    };
    static_assert(sizeof(SceneCasterEntry) == 96, "SceneCasterEntry layout drifted from std140 expectation");

    /**
     * UBO payload that lists every shadow-casting Voxel3D in the scene.
     *
     * Renderer rebuilds this once per frame from the scene graph and binds it at descriptor
     * set 0 binding 3. The companion descriptor binding 4 is an array of `kMaxSceneCasters`
     * storage buffers holding each caster's voxel data; the shader indexes both arrays by the
     * same caster index.
     *
     * `count` reflects how many entries the shader should iterate; entries past `count` are
     * untouched. The 16-slot descriptor array is fully written each frame (unused slots repeat
     * the first caster's buffer) so the descriptor set is always valid for Vulkan.
     */
    struct SceneCasters {
        int32_t count = 0;
        int32_t _pad0 = 0;
        int32_t _pad1 = 0;
        int32_t _pad2 = 0;
        SceneCasterEntry entries[kMaxSceneCasters];
    };
    static_assert(sizeof(SceneCasters) == 16 + kMaxSceneCasters * 96,
                  "SceneCasters layout must match shader expectation");
}
