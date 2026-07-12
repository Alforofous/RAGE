#include "collision_world.hpp"

#include <algorithm>
#include <cmath>
#include "engine/scene/brick.hpp"
#include "engine/scene/voxel_data.hpp"

namespace RAGE::Toolkit {
    namespace {
        /// Keeps clipped faces a hair off voxel planes so float error can't re-enter.
        constexpr float kSkin = 1e-4f;
        /// Shrink applied to a box's max side when deriving occupied voxel ranges, so a
        /// face resting exactly on a plane does not count as occupying the next cell.
        constexpr float kFaceEps = 1e-5f;

        int32_t floorToVoxel(float worldPos, float voxelSize) {
            return static_cast<int32_t>(std::floor(worldPos / voxelSize));
        }
    }

    CollisionWorld::CollisionWorld(const WorldBrickGrid &grid, const BrickPool &pool,
                                   float voxelSize)
        : grid_(grid)
        , pool_(pool)
        , voxelSize_(voxelSize) {}

    void CollisionWorld::registerVolume(const Voxel3D &volume) {
        if (std::ranges::find(volumes_, &volume) == volumes_.end()) {
            volumes_.push_back(&volume);
        }
    }

    void CollisionWorld::unregisterVolume(const Voxel3D &volume) {
        std::erase(volumes_, &volume);
    }

    bool CollisionWorld::latticeSolid_(IVec3 worldVoxel) const {
        const IVec3 brickCoord{ worldVoxel.x >> 3, worldVoxel.y >> 3, worldVoxel.z >> 3 };
        const BrickHandle h = grid_.handleAt(brickCoord);
        if (h == kEmptyBrick) {
            return false;
        }
        const uint32_t packed = pool_.brick(h).voxels[brickVoxelIndex(
            worldVoxel.x & 7, worldVoxel.y & 7, worldVoxel.z & 7)];
        return (packed >> 24u) > 0u;
    }

    bool CollisionWorld::volumeSolidAt_(const Voxel3D &volume, const Mat4 &invWorld,
                                        Vec3 worldPoint) const {
        const Vec3 local = invWorld.transformPoint(worldPoint);
        const float vs = volume.voxelSize();
        const IVec3 c{ floorToVoxel(local.x, vs), floorToVoxel(local.y, vs),
                       floorToVoxel(local.z, vs) };
        const VoxelData *data = volume.voxelData();
        if (data == nullptr) {
            return false;
        }
        return (data->voxel(c) >> 24u) > 0u;
    }

    bool CollisionWorld::solid(IVec3 worldVoxel, const Voxel3D *ignore) const {
        if (latticeSolid_(worldVoxel)) {
            return true;
        }
        if (volumes_.empty()) {
            return false;
        }
        const Vec3 center{ (static_cast<float>(worldVoxel.x) + 0.5f) * voxelSize_,
                           (static_cast<float>(worldVoxel.y) + 0.5f) * voxelSize_,
                           (static_cast<float>(worldVoxel.z) + 0.5f) * voxelSize_ };
        return std::ranges::any_of(volumes_, [&](const Voxel3D *v) {
            return v != ignore && volumeSolidAt_(*v, v->worldMatrix().inverted(), center);
        });
    }

    float CollisionWorld::sweepAxis_(SweepBox box, int32_t axis, float delta,
                                     const Voxel3D *ignore, bool &hit) const {
        hit = false;
        if (delta == 0.0f) {
            return 0.0f;
        }
        const float vs = voxelSize_;
        const int32_t dir = delta > 0.0f ? 1 : -1;
        const int32_t o1 = (axis + 1) % 3;
        const int32_t o2 = (axis + 2) % 3;

        const float lead = dir > 0 ? box.max[axis] : box.min[axis];
        const int32_t firstPlane = dir > 0 ? floorToVoxel(lead - kFaceEps, vs) + 1
                                           : floorToVoxel(lead + kFaceEps, vs) - 1;
        const int32_t lastPlane = floorToVoxel(lead + delta, vs);

        const int32_t o1Min = floorToVoxel(box.min[o1] + kFaceEps, vs);
        const int32_t o1Max = floorToVoxel(box.max[o1] - kFaceEps, vs);
        const int32_t o2Min = floorToVoxel(box.min[o2] + kFaceEps, vs);
        const int32_t o2Max = floorToVoxel(box.max[o2] - kFaceEps, vs);

        for (int32_t v = firstPlane; dir > 0 ? v <= lastPlane : v >= lastPlane; v += dir) {
            for (int32_t a = o1Min; a <= o1Max; ++a) {
                for (int32_t b = o2Min; b <= o2Max; ++b) {
                    IVec3 c{};
                    c[axis] = v;
                    c[o1] = a;
                    c[o2] = b;
                    if (!solid(c, ignore)) {
                        continue;
                    }
                    hit = true;
                    const float plane = dir > 0 ? static_cast<float>(v) * vs
                                                : static_cast<float>(v + 1) * vs;
                    const float clipped = (plane - (static_cast<float>(dir) * kSkin)) - lead;
                    // Never clip *backwards* past the start (box already touching).
                    return dir > 0 ? std::fmax(0.0f, clipped) : std::fmin(0.0f, clipped);
                }
            }
        }
        return delta;
    }

    SweepResult CollisionWorld::sweepAABB(const SweepBox &box, Vec3 delta,
                                          const Voxel3D *ignore) const {
        SweepResult result{};
        SweepBox current = box;

        static constexpr int32_t kAxisOrder[3] = { 1, 0, 2 };
        bool hits[3] = { false, false, false };
        Vec3 moved(0.0f, 0.0f, 0.0f);
        for (const int32_t axis : kAxisOrder) {
            const float m = sweepAxis_(current, axis, delta[axis], ignore, hits[axis]);
            moved[axis] = m;
            current.min[axis] += m;
            current.max[axis] += m;
        }

        result.moved = moved;
        result.hitX = hits[0];
        result.hitY = hits[1];
        result.hitZ = hits[2];
        return result;
    }
}
