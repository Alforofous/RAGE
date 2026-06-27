#include "world_grid.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace RAGE {
    namespace {
        int floorDivToCell(float worldCoord, float originCoord, float cellSize) {
            return static_cast<int>(std::floor((worldCoord - originCoord) / cellSize));
        }

        int ceilDivToCell(float worldCoord, float originCoord, float cellSize) {
            return static_cast<int>(std::ceil((worldCoord - originCoord) / cellSize));
        }

        size_t flatBitIndex(IVec3 cell, IVec3 dims) {
            return (static_cast<size_t>(cell.z) * static_cast<size_t>(dims.x) * static_cast<size_t>(dims.y))
                   + (static_cast<size_t>(cell.y) * static_cast<size_t>(dims.x)) + static_cast<size_t>(cell.x);
        }

        void setBit(uint8_t *bits, size_t bitIdx) {
            bits[bitIdx >> 3u] |= static_cast<uint8_t>(1u << (bitIdx & 7u));
        }
    }

    WorldGridLayout computeWorldGridLayout(std::span<const CasterAabb> aabbs, float cellSize,
                                            int paddingCells) {
        WorldGridLayout layout{};
        layout.cellSize = cellSize;
        if (aabbs.empty() || cellSize <= 0.0f) {
            return layout;
        }

        Vec3 worldMin{ std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::infinity() };
        Vec3 worldMax{ -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity() };
        for (const CasterAabb &a : aabbs) {
            worldMin.x = std::min(worldMin.x, a.worldMin.x);
            worldMin.y = std::min(worldMin.y, a.worldMin.y);
            worldMin.z = std::min(worldMin.z, a.worldMin.z);
            worldMax.x = std::max(worldMax.x, a.worldMax.x);
            worldMax.y = std::max(worldMax.y, a.worldMax.y);
            worldMax.z = std::max(worldMax.z, a.worldMax.z);
        }

        const int padding = std::max(0, paddingCells);
        const int loX = floorDivToCell(worldMin.x, 0.0f, cellSize) - padding;
        const int loY = floorDivToCell(worldMin.y, 0.0f, cellSize) - padding;
        const int loZ = floorDivToCell(worldMin.z, 0.0f, cellSize) - padding;
        const int hiX = ceilDivToCell(worldMax.x, 0.0f, cellSize) + padding;
        const int hiY = ceilDivToCell(worldMax.y, 0.0f, cellSize) + padding;
        const int hiZ = ceilDivToCell(worldMax.z, 0.0f, cellSize) + padding;

        layout.origin = Vec3{ static_cast<float>(loX) * cellSize, static_cast<float>(loY) * cellSize,
                              static_cast<float>(loZ) * cellSize };
        layout.dims = IVec3{ std::max(1, hiX - loX), std::max(1, hiY - loY), std::max(1, hiZ - loZ) };

        const size_t totalBits = static_cast<size_t>(layout.dims.x) * static_cast<size_t>(layout.dims.y)
                                 * static_cast<size_t>(layout.dims.z);
        layout.byteSize = (totalBits + 7u) / 8u;
        return layout;
    }

    void buildWorldGrid(std::span<const CasterAabb> aabbs, const WorldGridLayout &layout,
                         uint8_t *outBits) {
        if (layout.byteSize == 0 || outBits == nullptr) {
            return;
        }
        std::memset(outBits, 0, layout.byteSize);
        if (aabbs.empty() || layout.cellSize <= 0.0f) {
            return;
        }

        for (const CasterAabb &a : aabbs) {
            int loX = floorDivToCell(a.worldMin.x, layout.origin.x, layout.cellSize);
            int loY = floorDivToCell(a.worldMin.y, layout.origin.y, layout.cellSize);
            int loZ = floorDivToCell(a.worldMin.z, layout.origin.z, layout.cellSize);
            int hiX = floorDivToCell(a.worldMax.x, layout.origin.x, layout.cellSize);
            int hiY = floorDivToCell(a.worldMax.y, layout.origin.y, layout.cellSize);
            int hiZ = floorDivToCell(a.worldMax.z, layout.origin.z, layout.cellSize);

            loX = std::clamp(loX, 0, layout.dims.x - 1);
            loY = std::clamp(loY, 0, layout.dims.y - 1);
            loZ = std::clamp(loZ, 0, layout.dims.z - 1);
            hiX = std::clamp(hiX, 0, layout.dims.x - 1);
            hiY = std::clamp(hiY, 0, layout.dims.y - 1);
            hiZ = std::clamp(hiZ, 0, layout.dims.z - 1);

            for (int z = loZ; z <= hiZ; ++z) {
                for (int y = loY; y <= hiY; ++y) {
                    for (int x = loX; x <= hiX; ++x) {
                        setBit(outBits, flatBitIndex({ x, y, z }, layout.dims));
                    }
                }
            }
        }
    }
}
