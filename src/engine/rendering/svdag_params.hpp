#pragma once

#include <cstdint>
#include "math/vec.hpp"

namespace RAGE {
    /**
     * @brief std140 UBO at set=0 binding=10 of voxel_raycast.comp. Shader-side declaration:
     *
     *     layout(set = 0, binding = 10, std140) uniform SvdagParams {
     *         vec4 origin_brickSize;            // .xyz = cube origin (world), .w = brickWorldSize
     *         ivec4 root_levels_paddedDim_pad;  // .x = rootIndex, .y = levels, .z = paddedDim
     *     } svdagParams;
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
