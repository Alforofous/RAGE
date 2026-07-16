#pragma once

#include "engine/toolkit/content/generator.hpp"

namespace RAGE::Toolkit::Content {
    /// 16×16 grid of identical 16³ cubes. Demonstrates brick dedup (256 cubes → ~1 unique brick).
    class CubeGridGenerator : public Generator {
    public:
        std::vector<std::unique_ptr<Voxel3D>> generate(BrickPool &pool, float voxelSize) override;
    };

}
