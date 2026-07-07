#pragma once

#include "engine/content/generator.hpp"

namespace RAGE::Content {
    /// 16×16 grid of identical 16³ cubes. Demonstrates brick dedup (256 cubes → ~1 unique brick).
    class CubeGridGenerator : public Generator {
    public:
        std::vector<std::unique_ptr<Voxel3D>> generate(BrickPool &pool, float voxelSize) override;
    };

}
