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

    bool CollisionWorld::volumeIsBodyOwned_(const Voxel3D *volume) const {
        return std::ranges::any_of(bodies_,
                                   [volume](const Body &b) { return b.ownedVolume == volume; });
    }

    void CollisionWorld::gatherVolumes_(const SweepBox &bounds, const Voxel3D *ignore) const {
        volumeScratch_.clear();
        for (const Voxel3D *v : volumes_) {
            if (v == ignore || volumeIsBodyOwned_(v)) {
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
            volumeScratch_.push_back(VolumeScratch{ .volume = v, .invWorld = world.inverted() });
        }
    }

    bool CollisionWorld::scratchSolid_(IVec3 worldVoxel) const {
        if (latticeSolid_(worldVoxel)) {
            return true;
        }
        if (volumeScratch_.empty()) {
            return false;
        }
        const Vec3 center{ (static_cast<float>(worldVoxel.x) + 0.5f) * voxelSize_,
                           (static_cast<float>(worldVoxel.y) + 0.5f) * voxelSize_,
                           (static_cast<float>(worldVoxel.z) + 0.5f) * voxelSize_ };
        return std::ranges::any_of(volumeScratch_, [&](const VolumeScratch &sc) {
            return volumeSolidAt_(*sc.volume, sc.invWorld, center);
        });
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
            return v != ignore && !volumeIsBodyOwned_(v)
                   && volumeSolidAt_(*v, v->worldMatrix().inverted(), center);
        });
    }

    /// Precondition: `gatherVolumes_` covered `box`.
    bool CollisionWorld::overlapsSolid_(const SweepBox &box, const Voxel3D * /*ignore*/) const {
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
                    if (scratchSolid_(IVec3{ x, y, z })) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    Vec3 CollisionWorld::depenetrate(const SweepBox &box, float maxPush,
                                     const Voxel3D *ignore) const {
        const Vec3 pad(maxPush, maxPush, maxPush);
        gatherVolumes_(SweepBox{ .min = box.min - pad, .max = box.max + pad }, ignore);
        if (!overlapsSolid_(box, ignore)) {
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
                    if (!overlapsSolid_(moved, ignore)) {
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

    BodyId CollisionWorld::addBody(const SweepBox &box, float mass,
                                   const Voxel3D *ownedVolume) {
        bodies_.push_back(Body{ .id = nextBodyId_, .box = box, .mass = mass,
                                .ownedVolume = ownedVolume });
        return BodyId{ nextBodyId_++ };
    }

    void CollisionWorld::removeBody(BodyId id) {
        std::erase_if(bodies_, [id](const Body &b) { return b.id == id.id; });
    }

    void CollisionWorld::updateBodyBox(BodyId id, const SweepBox &box) {
        for (Body &b : bodies_) {
            if (b.id == id.id) {
                b.box = box;
                return;
            }
        }
    }

    Vec3 CollisionWorld::separationFor(BodyId self) const {
        const Body *me = nullptr;
        for (const Body &b : bodies_) {
            if (b.id == self.id) {
                me = &b;
                break;
            }
        }
        if (me == nullptr) {
            return Vec3(0.0f, 0.0f, 0.0f);
        }

        Vec3 total(0.0f, 0.0f, 0.0f);
        for (const Body &other : bodies_) {
            if (other.id == me->id) {
                continue;
            }
            // Per-axis overlap depths; positive on every axis = boxes intersect.
            float depth[3];
            bool overlaps = true;
            for (int32_t a = 0; a < 3 && overlaps; ++a) {
                const float d = std::min(me->box.max[a], other.box.max[a])
                                - std::max(me->box.min[a], other.box.min[a]);
                depth[a] = d;
                overlaps = d > 0.0f;
            }
            if (!overlaps) {
                continue;
            }
            int32_t axis = 0;
            if (depth[1] < depth[axis]) {
                axis = 1;
            }
            if (depth[2] < depth[axis]) {
                axis = 2;
            }
            const float myCenter = (me->box.min[axis] + me->box.max[axis]) * 0.5f;
            const float otherCenter = (other.box.min[axis] + other.box.max[axis]) * 0.5f;
            const float dir = myCenter < otherCenter ? -1.0f : 1.0f;
            const float myShare = other.mass / (me->mass + other.mass);
            total[axis] += dir * depth[axis] * myShare;
        }
        return total;
    }

    /// Precondition: `gatherVolumes_` covered the whole swept region.
    float CollisionWorld::sweepAxis_(SweepBox box, int32_t axis, float delta, bool &hit) const {
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
                    if (!scratchSolid_(c)) {
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

        SweepBox sweptBounds = box;
        for (int32_t a = 0; a < 3; ++a) {
            sweptBounds.min[a] = std::min(sweptBounds.min[a], sweptBounds.min[a] + delta[a]);
            sweptBounds.max[a] = std::max(sweptBounds.max[a], sweptBounds.max[a] + delta[a]);
        }
        gatherVolumes_(sweptBounds, ignore);

        static constexpr int32_t kAxisOrder[3] = { 1, 0, 2 };
        bool hits[3] = { false, false, false };
        Vec3 moved(0.0f, 0.0f, 0.0f);
        for (const int32_t axis : kAxisOrder) {
            const float m = sweepAxis_(current, axis, delta[axis], hits[axis]);
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
