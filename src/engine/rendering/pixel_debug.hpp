#pragma once

#include <cstdint>

namespace RAGE {
    /**
     * Per-pixel introspection payload written by the voxel raycast compute shader for one
     * "picked" pixel each frame. Layout mirrors GLSL std430 — 4-byte scalars packed tightly.
     *
     * Renderer allocates one of these as a host-visible storage buffer at descriptor set 0
     * binding 5. The frame UBO carries the picked pixel coordinates; the shader's single thread
     * matching those coordinates writes the entire struct, then the CPU reads it back after
     * GPU sync.
     *
     * @note Debug-only. Removing the pick infrastructure means deleting this header, the
     *       binding-5 declaration in voxel_raycast.comp, the debugPixel_pad field in
     *       FrameUniforms, and the pick-related members/methods in Renderer.
     */
    struct PixelDebug {
        int32_t hit = 0;
        int32_t casterIdx = -1;
        int32_t voxelX = 0;
        int32_t voxelY = 0;
        int32_t voxelZ = 0;
        float tHit = 0.0f;
        float cameraX = 0.0f;
        float cameraY = 0.0f;
        float cameraZ = 0.0f;
        float rayDirX = 0.0f;
        float rayDirY = 0.0f;
        float rayDirZ = 0.0f;
        float hitWorldX = 0.0f;
        float hitWorldY = 0.0f;
        float hitWorldZ = 0.0f;
        float normalX = 0.0f;
        float normalY = 0.0f;
        float normalZ = 0.0f;
        uint32_t packedColor = 0;
        float toLightX = 0.0f;
        float toLightY = 0.0f;
        float toLightZ = 0.0f;
        float NdotL = 0.0f;
        int32_t shadowed = 0;
        int32_t blockerCasterIdx = -1;
        int32_t blockerX = 0;
        int32_t blockerY = 0;
        int32_t blockerZ = 0;
        float finalR = 0.0f;
        float finalG = 0.0f;
        float finalB = 0.0f;
    };
    static_assert(sizeof(PixelDebug) == 124, "PixelDebug layout must match shader std430 expectation");
}
