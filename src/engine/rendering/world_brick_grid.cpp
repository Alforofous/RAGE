#include "world_brick_grid.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include "engine/scene/voxel_data.hpp"

namespace RAGE {
    namespace {
        bool isPow2(int32_t v) { return v > 0 && (v & (v - 1)) == 0; }
    }

    WorldBrickGrid::WorldBrickGrid(IVec3 fixedDims)
        : fixedDims_(fixedDims)
        , wrapMask_{ fixedDims.x - 1, fixedDims.y - 1, fixedDims.z - 1 } {
        if (!isPow2(fixedDims.x) || !isPow2(fixedDims.y) || !isPow2(fixedDims.z)) {
            throw std::invalid_argument("WorldBrickGrid: dims must be powers of two, got "
                                        + std::to_string(fixedDims.x) + "x"
                                        + std::to_string(fixedDims.y) + "x"
                                        + std::to_string(fixedDims.z));
        }
        const size_t total = static_cast<size_t>(fixedDims_.x) * static_cast<size_t>(fixedDims_.y)
                             * static_cast<size_t>(fixedDims_.z);
        handles_.assign(total, kEmptyBrick);
    }

    size_t WorldBrickGrid::slotIndex(IVec3 worldCell) const {
        const IVec3 slot{ worldCell.x & wrapMask_.x, worldCell.y & wrapMask_.y,
                          worldCell.z & wrapMask_.z };
        return (static_cast<size_t>(slot.z) * static_cast<size_t>(fixedDims_.x)
                * static_cast<size_t>(fixedDims_.y))
               + (static_cast<size_t>(slot.y) * static_cast<size_t>(fixedDims_.x))
               + static_cast<size_t>(slot.x);
    }

    void WorldBrickGrid::setWindow(IVec3 windowMinBrick, IVec3 windowExtent) {
        if (windowExtent.x > fixedDims_.x || windowExtent.y > fixedDims_.y
            || windowExtent.z > fixedDims_.z || windowExtent.x < 0 || windowExtent.y < 0
            || windowExtent.z < 0) {
            throw std::invalid_argument(
                "WorldBrickGrid: window extent " + std::to_string(windowExtent.x) + "x"
                + std::to_string(windowExtent.y) + "x" + std::to_string(windowExtent.z)
                + " exceeds fixed dims " + std::to_string(fixedDims_.x) + "x"
                + std::to_string(fixedDims_.y) + "x" + std::to_string(fixedDims_.z));
        }
        windowMinBrick_ = windowMinBrick;
        windowExtent_ = windowExtent;
    }

    void WorldBrickGrid::writeChunk(IVec3 worldBrickOrigin, const VoxelData &data) {
        clearChunk(worldBrickOrigin, data.brickDims());
        data.forEachOccupiedBrick([this, worldBrickOrigin](IVec3 brickCoord, BrickHandle h) {
            handles_[slotIndex(IVec3{ worldBrickOrigin.x + brickCoord.x,
                                      worldBrickOrigin.y + brickCoord.y,
                                      worldBrickOrigin.z + brickCoord.z })] = h;
        });
    }

    void WorldBrickGrid::clearChunk(IVec3 worldBrickOrigin, IVec3 brickDims) {
        for (int32_t z = 0; z < brickDims.z; ++z) {
            for (int32_t y = 0; y < brickDims.y; ++y) {
                for (int32_t x = 0; x < brickDims.x; ++x) {
                    handles_[slotIndex(IVec3{ worldBrickOrigin.x + x, worldBrickOrigin.y + y,
                                              worldBrickOrigin.z + z })] = kEmptyBrick;
                }
            }
        }
    }

    BrickHandle WorldBrickGrid::handleAt(IVec3 worldCell) const {
        const IVec3 rel{ worldCell.x - windowMinBrick_.x, worldCell.y - windowMinBrick_.y,
                         worldCell.z - windowMinBrick_.z };
        if (rel.x < 0 || rel.x >= windowExtent_.x || rel.y < 0 || rel.y >= windowExtent_.y
            || rel.z < 0 || rel.z >= windowExtent_.z) {
            return kEmptyBrick;
        }
        return handles_[slotIndex(worldCell)];
    }

    void WorldBrickGrid::rebuild(std::span<const VoxelDataWorldPlacement> placements) {
        std::ranges::fill(handles_, kEmptyBrick);

        constexpr int32_t kMin = std::numeric_limits<int32_t>::min();
        constexpr int32_t kMax = std::numeric_limits<int32_t>::max();
        IVec3 mn{ kMax, kMax, kMax };
        IVec3 mx{ kMin, kMin, kMin };
        for (const VoxelDataWorldPlacement &p : placements) {
            if (p.data == nullptr) {
                continue;
            }
            const IVec3 bd = p.data->brickDims();
            mn.x = std::min(mn.x, p.worldBrickOrigin.x);
            mn.y = std::min(mn.y, p.worldBrickOrigin.y);
            mn.z = std::min(mn.z, p.worldBrickOrigin.z);
            mx.x = std::max(mx.x, p.worldBrickOrigin.x + bd.x);
            mx.y = std::max(mx.y, p.worldBrickOrigin.y + bd.y);
            mx.z = std::max(mx.z, p.worldBrickOrigin.z + bd.z);
        }
        if (mn.x > mx.x) {
            setWindow(IVec3{ 0, 0, 0 }, IVec3{ 0, 0, 0 });
            return;
        }

        setWindow(mn, IVec3{ mx.x - mn.x, mx.y - mn.y, mx.z - mn.z });
        for (const VoxelDataWorldPlacement &p : placements) {
            if (p.data == nullptr) {
                continue;
            }
            writeChunk(p.worldBrickOrigin, *p.data);
        }
    }
}
