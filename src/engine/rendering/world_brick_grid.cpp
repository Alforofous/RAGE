#include "world_brick_grid.hpp"

#include <algorithm>
#include <limits>
#include "engine/scene/voxel_data.hpp"

namespace RAGE {
    WorldBrickGrid::WorldBrickGrid() = default;

    size_t WorldBrickGrid::flatIndex(IVec3 worldCell) const {
        return (static_cast<size_t>(worldCell.z) * static_cast<size_t>(dims_.x) * static_cast<size_t>(dims_.y))
               + (static_cast<size_t>(worldCell.y) * static_cast<size_t>(dims_.x)) + static_cast<size_t>(worldCell.x);
    }

    BrickHandle WorldBrickGrid::handleAt(IVec3 worldCell) const {
        const IVec3 local{ worldCell.x - worldBrickOrigin_.x, worldCell.y - worldBrickOrigin_.y,
                            worldCell.z - worldBrickOrigin_.z };
        if (local.x < 0 || local.x >= dims_.x || local.y < 0 || local.y >= dims_.y || local.z < 0
            || local.z >= dims_.z) {
            return kEmptyBrick;
        }
        return handles_[flatIndex(local)];
    }

    void WorldBrickGrid::rebuild(std::span<const VoxelDataWorldPlacement> placements) {
        if (placements.empty()) {
            worldBrickOrigin_ = { 0, 0, 0 };
            dims_ = { 0, 0, 0 };
            handles_.clear();
            return;
        }

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
            worldBrickOrigin_ = { 0, 0, 0 };
            dims_ = { 0, 0, 0 };
            handles_.clear();
            return;
        }

        constexpr int32_t kMargin = 1;
        worldBrickOrigin_ = { mn.x - kMargin, mn.y - kMargin, mn.z - kMargin };
        dims_ = { (mx.x + kMargin) - worldBrickOrigin_.x, (mx.y + kMargin) - worldBrickOrigin_.y,
                  (mx.z + kMargin) - worldBrickOrigin_.z };

        const size_t total = static_cast<size_t>(dims_.x) * static_cast<size_t>(dims_.y)
                             * static_cast<size_t>(dims_.z);
        handles_.assign(total, kEmptyBrick);

        for (const VoxelDataWorldPlacement &p : placements) {
            if (p.data == nullptr) {
                continue;
            }
            const IVec3 bd = p.data->brickDims();
            const std::span<const BrickHandle> srcHandles = p.data->handles();
            for (int32_t bz = 0; bz < bd.z; ++bz) {
                for (int32_t by = 0; by < bd.y; ++by) {
                    for (int32_t bx = 0; bx < bd.x; ++bx) {
                        const size_t srcIdx = (static_cast<size_t>(bz) * static_cast<size_t>(bd.x)
                                                * static_cast<size_t>(bd.y))
                                              + (static_cast<size_t>(by) * static_cast<size_t>(bd.x))
                                              + static_cast<size_t>(bx);
                        const BrickHandle h = srcHandles[srcIdx];
                        if (h == kEmptyBrick) {
                            continue;
                        }
                        const IVec3 dst{ (p.worldBrickOrigin.x + bx) - worldBrickOrigin_.x,
                                          (p.worldBrickOrigin.y + by) - worldBrickOrigin_.y,
                                          (p.worldBrickOrigin.z + bz) - worldBrickOrigin_.z };
                        handles_[flatIndex(dst)] = h;
                    }
                }
            }
        }
    }
}
