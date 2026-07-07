#include "scene_generators.hpp"

#include <cstdio>
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"

namespace RAGE::Content {
    std::vector<std::unique_ptr<Voxel3D>> CubeGridGenerator::generate(BrickPool &pool, float voxelSize) {
        constexpr int32_t kCubeDim = 16;
        constexpr int kGridN = 16;
        constexpr float kCubeSpacing = 1.5f;
        constexpr uint32_t kCubeColor = 0xFFCCDDEEu;
        const std::vector<uint32_t> cubeVoxels(static_cast<size_t>(kCubeDim) * kCubeDim * kCubeDim,
                                                kCubeColor);
        const float kOriginX = -static_cast<float>(kGridN - 1) * kCubeSpacing * 0.5f;
        const float kOriginZ = -16.0f;

        std::vector<std::unique_ptr<Voxel3D>> out;
        out.reserve(static_cast<size_t>(kGridN) * kGridN);
        for (int gx = 0; gx < kGridN; ++gx) {
            for (int gz = 0; gz < kGridN; ++gz) {
                auto cube = std::make_unique<Voxel3D>(pool, IVec3(kCubeDim, kCubeDim, kCubeDim),
                                                       voxelSize);
                cube->fillFromPackedRGBA8(cubeVoxels.data(), IVec3(kCubeDim, kCubeDim, kCubeDim));
                cube->setPosition(Vec3(kOriginX + (static_cast<float>(gx) * kCubeSpacing), -0.4f,
                                        kOriginZ + (static_cast<float>(gz) * kCubeSpacing)));
                out.push_back(std::move(cube));
            }
        }
        std::fprintf(stdout, "Built procedural cube scene: %d×%d = %d cubes\n", kGridN, kGridN,
                     kGridN * kGridN);
        return out;
    }

}
