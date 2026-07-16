#include "body_registry.hpp"

#include <algorithm>

namespace RAGE::Toolkit {
    BodyId BodyRegistry::add(const SweepBox &box, float mass, const Voxel3D *ownedVolume) {
        bodies_.push_back(
            Body{ .id = nextId_, .box = box, .mass = mass, .ownedVolume = ownedVolume });
        rebuildOwnedVolumes_();
        return BodyId{ nextId_++ };
    }

    void BodyRegistry::remove(BodyId id) {
        std::erase_if(bodies_, [id](const Body &b) { return b.id == id.id; });
        rebuildOwnedVolumes_();
    }

    void BodyRegistry::updateBox(BodyId id, const SweepBox &box) {
        for (Body &b : bodies_) {
            if (b.id == id.id) {
                b.box = box;
                return;
            }
        }
    }

    void BodyRegistry::rebuildOwnedVolumes_() {
        ownedVolumes_.clear();
        for (const Body &b : bodies_) {
            if (b.ownedVolume != nullptr) {
                ownedVolumes_.push_back(b.ownedVolume);
            }
        }
    }

    Vec3 BodyRegistry::separationFor(BodyId self) const {
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
}
