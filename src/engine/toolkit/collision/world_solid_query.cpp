#include "world_solid_query.hpp"

#include <algorithm>
#include <cmath>
#include "engine/scene/brick.hpp"
#include "engine/scene/voxel3d.hpp"
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

    WorldSolidQuery::WorldSolidQuery(const WorldBrickGrid &grid, const BrickPool &pool,
                                     float voxelSize)
        : grid_(grid)
        , pool_(pool)
        , voxelSize_(voxelSize) {}

    void WorldSolidQuery::registerVolume(const Voxel3D &volume) {
        if (std::ranges::find(volumes_, &volume) == volumes_.end()) {
            volumes_.push_back(&volume);
        }
    }

    void WorldSolidQuery::unregisterVolume(const Voxel3D &volume) {
        std::erase(volumes_, &volume);
    }

    bool WorldSolidQuery::latticeSolid_(IVec3 worldVoxel) const {
        const IVec3 brickCoord{ worldVoxel.x >> 3, worldVoxel.y >> 3, worldVoxel.z >> 3 };
        const BrickHandle h = grid_.handleAt(brickCoord);
        if (h == kEmptyBrick) {
            return false;
        }
        const uint32_t packed = pool_.brick(h).voxels[brickVoxelIndex(
            worldVoxel.x & 7, worldVoxel.y & 7, worldVoxel.z & 7)];
        return (packed >> 24u) > 0u;
    }

    bool WorldSolidQuery::volumeSolidAt_(const Voxel3D &volume, const Mat4 &invWorld,
                                         Vec3 worldPoint) {
        const VoxelData *data = volume.voxelData();
        if (data == nullptr) {
            return false;
        }
        const Vec3 local = invWorld.transformPoint(worldPoint);
        const float vs = volume.voxelSize();
        const IVec3 c{ floorToVoxel(local.x, vs), floorToVoxel(local.y, vs),
                       floorToVoxel(local.z, vs) };
        return (data->voxel(c) >> 24u) > 0u;
    }

    std::span<const WorldSolidQuery::VolumeScratch> WorldSolidQuery::gatherVolumes_(
        const SweepBox &bounds, const Voxel3D *ignore,
        std::span<const Voxel3D *const> excluded) const {
        scratch_.clear();
        for (const Voxel3D *v : volumes_) {
            if (v == ignore || std::ranges::find(excluded, v) != excluded.end()) {
                continue;
            }
            // Conservative world AABB of the volume's local bounds (8 corners).
            const Mat4 &world = v->worldMatrix();
            const IVec3 dims = v->dimensions();
            const Vec3 ext{ static_cast<float>(dims.x) * v->voxelSize(),
                            static_cast<float>(dims.y) * v->voxelSize(),
                            static_cast<float>(dims.z) * v->voxelSize() };
            Vec3 mn(0.0f, 0.0f, 0.0f);
            Vec3 mx(0.0f, 0.0f, 0.0f);
            bool first = true;
            for (int32_t corner = 0; corner < 8; ++corner) {
                const Vec3 local{ (corner & 1) != 0 ? ext.x : 0.0f,
                                  (corner & 2) != 0 ? ext.y : 0.0f,
                                  (corner & 4) != 0 ? ext.z : 0.0f };
                const Vec3 pnt = world.transformPoint(local);
                if (first) {
                    mn = pnt;
                    mx = pnt;
                    first = false;
                } else {
                    for (int32_t a = 0; a < 3; ++a) {
                        mn[a] = std::min(mn[a], pnt[a]);
                        mx[a] = std::max(mx[a], pnt[a]);
                    }
                }
            }
            if (mx.x < bounds.min.x || mn.x > bounds.max.x || mx.y < bounds.min.y
                || mn.y > bounds.max.y || mx.z < bounds.min.z || mn.z > bounds.max.z) {
                continue;
            }
            scratch_.push_back(VolumeScratch{ .volume = v, .invWorld = world.inverted() });
        }
        return scratch_;
    }

    bool WorldSolidQuery::cellSolid_(IVec3 worldVoxel,
                                     std::span<const VolumeScratch> scope) const {
        if (latticeSolid_(worldVoxel)) {
            return true;
        }
        if (scope.empty()) {
            return false;
        }
        const Vec3 center{ (static_cast<float>(worldVoxel.x) + 0.5f) * voxelSize_,
                           (static_cast<float>(worldVoxel.y) + 0.5f) * voxelSize_,
                           (static_cast<float>(worldVoxel.z) + 0.5f) * voxelSize_ };
        return std::ranges::any_of(scope, [&](const VolumeScratch &sc) {
            return volumeSolidAt_(*sc.volume, sc.invWorld, center);
        });
    }

    bool WorldSolidQuery::solid(IVec3 worldVoxel, const Voxel3D *ignore,
                                std::span<const Voxel3D *const> excluded) const {
        const Vec3 cellMin{ static_cast<float>(worldVoxel.x) * voxelSize_,
                            static_cast<float>(worldVoxel.y) * voxelSize_,
                            static_cast<float>(worldVoxel.z) * voxelSize_ };
        const SweepBox cell{ .min = cellMin,
                             .max = cellMin + Vec3(voxelSize_, voxelSize_, voxelSize_) };
        return cellSolid_(worldVoxel, gatherVolumes_(cell, ignore, excluded));
    }

    bool WorldSolidQuery::overlapsSolid_(const SweepBox &box,
                                         std::span<const VolumeScratch> scope) const {
        const float vs = voxelSize_;
        const int32_t xMin = static_cast<int32_t>(std::floor((box.min.x + kFaceEps) / vs));
        const int32_t xMax = static_cast<int32_t>(std::floor((box.max.x - kFaceEps) / vs));
        const int32_t yMin = static_cast<int32_t>(std::floor((box.min.y + kFaceEps) / vs));
        const int32_t yMax = static_cast<int32_t>(std::floor((box.max.y - kFaceEps) / vs));
        const int32_t zMin = static_cast<int32_t>(std::floor((box.min.z + kFaceEps) / vs));
        const int32_t zMax = static_cast<int32_t>(std::floor((box.max.z - kFaceEps) / vs));
        for (int32_t z = zMin; z <= zMax; ++z) {
            for (int32_t y = yMin; y <= yMax; ++y) {
                for (int32_t x = xMin; x <= xMax; ++x) {
                    if (cellSolid_(IVec3{ x, y, z }, scope)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    Vec3 WorldSolidQuery::depenetrate(const SweepBox &box, float maxPush, const Voxel3D *ignore,
                                      std::span<const Voxel3D *const> excluded) const {
        const Vec3 pad(maxPush, maxPush, maxPush);
        const std::span<const VolumeScratch> scope = gatherVolumes_(
            SweepBox{ .min = box.min - pad, .max = box.max + pad }, ignore, excluded);
        if (!overlapsSolid_(box, scope)) {
            return Vec3(0.0f, 0.0f, 0.0f);
        }
        // Probe axis pushes in half-voxel steps; return the shortest that clears.
        const float step = voxelSize_ * 0.5f;
        Vec3 best(0.0f, 0.0f, 0.0f);
        float bestDist = maxPush + step;
        for (float dist = step; dist <= maxPush + 1e-6f; dist += step) {
            if (dist >= bestDist) {
                break;
            }
            for (int32_t axis = 0; axis < 3; ++axis) {
                for (const float sign : { 1.0f, -1.0f }) {
                    Vec3 push(0.0f, 0.0f, 0.0f);
                    push[axis] = (sign * dist) + (sign * kSkin);
                    const SweepBox moved{ .min = box.min + push, .max = box.max + push };
                    if (!overlapsSolid_(moved, scope)) {
                        best = push;
                        bestDist = dist;
                        break;
                    }
                }
                if (bestDist <= dist) {
                    break;
                }
            }
            if (bestDist <= dist) {
                break;
            }
        }
        return best;
    }

    float WorldSolidQuery::sweepAxis_(SweepBox box, int32_t axis, float delta,
                                      std::span<const VolumeScratch> scope, bool &hit) const {
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
                    if (!cellSolid_(c, scope)) {
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

    SweepResult WorldSolidQuery::sweepAABB(const SweepBox &box, Vec3 delta, const Voxel3D *ignore,
                                           std::span<const Voxel3D *const> excluded) const {
        SweepResult result{};
        SweepBox current = box;

        SweepBox sweptBounds = box;
        for (int32_t a = 0; a < 3; ++a) {
            sweptBounds.min[a] = std::min(sweptBounds.min[a], sweptBounds.min[a] + delta[a]);
            sweptBounds.max[a] = std::max(sweptBounds.max[a], sweptBounds.max[a] + delta[a]);
        }
        const std::span<const VolumeScratch> scope = gatherVolumes_(sweptBounds, ignore, excluded);

        static constexpr int32_t kAxisOrder[3] = { 1, 0, 2 };
        bool hits[3] = { false, false, false };
        Vec3 moved(0.0f, 0.0f, 0.0f);
        for (const int32_t axis : kAxisOrder) {
            const float m = sweepAxis_(current, axis, delta[axis], scope, hits[axis]);
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
