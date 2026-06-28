#include "scene_generators.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "engine/scene/brick_pool.hpp"
#include "engine/scene/voxel3d.hpp"

namespace RAGE::Content {
    namespace {
        uint32_t pack(int r, int g, int b) {
            return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8u)
                   | (static_cast<uint32_t>(b) << 16u) | 0xFF000000u;
        }
    }

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

    std::vector<std::unique_ptr<Voxel3D>> TerrainGenerator::generate(BrickPool &pool, float voxelSize) {
        constexpr int32_t kW = 256;
        constexpr int32_t kH = 64;
        constexpr int32_t kD = 256;
        const uint32_t kStone = pack(0x70, 0x70, 0x70);
        const uint32_t kDirt  = pack(0x6E, 0x4E, 0x35);
        const uint32_t kGrass = pack(0x4A, 0x8A, 0x3D);

        std::vector<uint32_t> src(static_cast<size_t>(kW) * kH * kD, 0u);
        for (int32_t z = 0; z < kD; ++z) {
            for (int32_t x = 0; x < kW; ++x) {
                const float fx = static_cast<float>(x);
                const float fz = static_cast<float>(z);
                const float h = (std::sin(fx * 0.10f) * 4.0f) + (std::sin(fz * 0.08f) * 3.0f) + 28.0f;
                const int32_t height = std::clamp(static_cast<int32_t>(h), 1, kH - 1);
                for (int32_t y = 0; y <= height; ++y) {
                    const int32_t depth = height - y;
                    uint32_t color = kStone;
                    if (depth == 0) {
                        color = kGrass;
                    } else if (depth <= 3) {
                        color = kDirt;
                    }
                    const size_t idx = (static_cast<size_t>(z) * kH * kW)
                                        + (static_cast<size_t>(y) * kW) + static_cast<size_t>(x);
                    src[idx] = color;
                }
            }
        }

        auto terrain = std::make_unique<Voxel3D>(pool, IVec3(kW, kH, kD), voxelSize);
        terrain->fillFromPackedRGBA8(src.data(), IVec3(kW, kH, kD));
        terrain->setPosition(Vec3(-6.4f, -3.2f, -16.0f));

        std::fprintf(stdout, "Built procedural terrain scene: %d×%d×%d voxels\n", kW, kH, kD);

        std::vector<std::unique_ptr<Voxel3D>> out;
        out.push_back(std::move(terrain));
        return out;
    }

    std::vector<std::unique_ptr<Voxel3D>> BigWorldGenerator::generate(BrickPool &pool, float voxelSize) {
        constexpr int32_t kTileW = 128;
        constexpr int32_t kTileH = 32;
        constexpr int32_t kTileD = 128;
        constexpr int kGridN = 8;
        const uint32_t kStone = pack(0x70, 0x70, 0x70);
        const uint32_t kDirt  = pack(0x6E, 0x4E, 0x35);
        const uint32_t kGrass = pack(0x4A, 0x8A, 0x3D);

        std::vector<uint32_t> tileVoxels(static_cast<size_t>(kTileW) * kTileH * kTileD, 0u);
        for (int32_t z = 0; z < kTileD; ++z) {
            for (int32_t x = 0; x < kTileW; ++x) {
                const float fx = static_cast<float>(x);
                const float fz = static_cast<float>(z);
                const float h = (std::sin(fx * 0.10f) * 3.0f) + (std::sin(fz * 0.08f) * 2.0f) + 14.0f;
                const int32_t height = std::clamp(static_cast<int32_t>(h), 1, kTileH - 1);
                for (int32_t y = 0; y <= height; ++y) {
                    const int32_t depth = height - y;
                    uint32_t color = kStone;
                    if (depth == 0) {
                        color = kGrass;
                    } else if (depth <= 3) {
                        color = kDirt;
                    }
                    const size_t idx = (static_cast<size_t>(z) * kTileH * kTileW)
                                        + (static_cast<size_t>(y) * kTileW) + static_cast<size_t>(x);
                    tileVoxels[idx] = color;
                }
            }
        }

        const float kTileWorld = static_cast<float>(kTileW) * voxelSize;
        const float kOriginX = -kTileWorld * static_cast<float>(kGridN) * 0.5f;
        const float kOriginZ = -kTileWorld * static_cast<float>(kGridN) * 0.5f;

        std::vector<std::unique_ptr<Voxel3D>> out;
        out.reserve(static_cast<size_t>(kGridN) * kGridN);
        for (int gx = 0; gx < kGridN; ++gx) {
            for (int gz = 0; gz < kGridN; ++gz) {
                auto tile = std::make_unique<Voxel3D>(pool, IVec3(kTileW, kTileH, kTileD), voxelSize);
                tile->fillFromPackedRGBA8(tileVoxels.data(), IVec3(kTileW, kTileH, kTileD));
                tile->setPosition(Vec3(kOriginX + (static_cast<float>(gx) * kTileWorld), -1.6f,
                                        kOriginZ + (static_cast<float>(gz) * kTileWorld)));
                out.push_back(std::move(tile));
            }
        }
        std::fprintf(stdout, "Built tiled big-world scene: %d×%d tiles of %d×%d×%d voxels\n", kGridN,
                     kGridN, kTileW, kTileH, kTileD);
        return out;
    }
}
