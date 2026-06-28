#pragma once

#include <cstdint>
#include "math/vec.hpp"

namespace RAGE {
    /**
     * GPU uniform-buffer payload describing the SVDAG's spatial layout, written by the
     * renderer once per frame and bound at descriptor set 0, binding 10 of the voxel
     * raycast shader.
     *
     * Layout matches GLSL std140 — every member is a `vec4`/`ivec4` so alignment rules
     * collapse to a naturally-aligned 16-byte stride.
     *
     * @note Shader-side declaration must mirror this struct exactly:
     *
     *     layout(set = 0, binding = 10, std140) uniform SvdagParams {
     *         vec4 origin_brickSize;            // .xyz = world coords of cube origin, .w = brick world size
     *         ivec4 root_levels_paddedDim_pad;  // .x = rootIndex, .y = levels, .z = paddedDim
     *     } svdagParams;
     *
     * Field names encode the packing (`origin_brickSize.w == brickWorldSize`) so a
     * reader doesn't have to count bytes to decode meaning.
     */
    struct SvdagParamsUbo {
        Vec4 originWorld_brickSize;
        int32_t rootIndex = 0;
        int32_t levels = 0;
        int32_t paddedDim = 0;
        int32_t _pad = 0;
    };
    static_assert(sizeof(SvdagParamsUbo) == 32,
                  "SvdagParamsUbo layout must match the shader's std140 expectation");
}
