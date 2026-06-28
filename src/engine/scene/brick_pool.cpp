#include "brick_pool.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

namespace RAGE {
    namespace {
        size_t hashBrick(const Brick &b) {
            constexpr size_t kFnvSeed = 14695981039346656037ull;
            constexpr size_t kFnvPrime = 1099511628211ull;
            size_t h = kFnvSeed;
            for (size_t i = 0; i < Brick::kVoxelCount; ++i) {
                h ^= static_cast<size_t>(b.voxels[i]);
                h *= kFnvPrime;
            }
            return h;
        }

        bool brickEqual(const Brick &a, const Brick &b) {
            return std::memcmp(a.voxels, b.voxels, sizeof(Brick::voxels)) == 0;
        }

        void throwIfInvalid(BrickHandle h, size_t capacity) {
            if (h == kEmptyBrick || h.id >= capacity) {
                throw std::out_of_range("BrickPool: invalid BrickHandle");
            }
        }
    }

    BrickPool::BrickPool(bool enableDedup)
        : dedupEnabled_(enableDedup) {
        bricks_.reserve(kMaxBricks);
        refCount_.reserve(kMaxBricks);
        dirtyFlags_.reserve(kMaxBricks);
        dirtyHandles_.reserve(kMaxBricks);
        bricks_.emplace_back();
        refCount_.push_back(0u);
        dirtyFlags_.push_back(0u);
    }

    BrickHandle BrickPool::allocateSlotLocked_() {
        BrickHandle h = kEmptyBrick;
        if (!freeList_.empty()) {
            h = freeList_.back();
            freeList_.pop_back();
            bricks_[h.id] = Brick{};
        } else {
            if (bricks_.size() >= kMaxBricks) {
                throw std::runtime_error("BrickPool: exhausted (max bricks = " + std::to_string(kMaxBricks) + ")");
            }
            h = BrickHandle{ static_cast<uint32_t>(bricks_.size()) };
            bricks_.emplace_back();
            refCount_.push_back(0u);
            dirtyFlags_.push_back(0u);
        }
        refCount_[h.id] = 1u;
        markDirtyLocked_(h);
        return h;
    }

    BrickHandle BrickPool::allocate() {
        std::lock_guard lock(mutex_);
        return allocateSlotLocked_();
    }

    BrickHandle BrickPool::acquireBrick(const Brick &contents) {
        std::lock_guard lock(mutex_);
        if (dedupEnabled_) {
            const size_t h = hashBrick(contents);
            const auto range = contentHashToHandle_.equal_range(h);
            for (auto it = range.first; it != range.second; ++it) {
                if (brickEqual(bricks_[it->second.id], contents)) {
                    ++refCount_[it->second.id];
                    return it->second;
                }
            }
            const BrickHandle handle = allocateSlotLocked_();
            bricks_[handle.id] = contents;
            contentHashToHandle_.insert({ h, handle });
            return handle;
        }
        const BrickHandle handle = allocateSlotLocked_();
        bricks_[handle.id] = contents;
        return handle;
    }

    void BrickPool::release(BrickHandle h) {
        if (h == kEmptyBrick) {
            return;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        if (refCount_[h.id] == 0u) {
            return;
        }
        --refCount_[h.id];
        if (refCount_[h.id] == 0u) {
            removeFromDedupLocked_(h);
            // Clears here so drainDirty can filter — avoids O(n) middle-erase per release.
            dirtyFlags_[h.id] = 0u;
            freeList_.push_back(h);
        }
    }

    const Brick &BrickPool::brick(BrickHandle h) const {
        throwIfInvalid(h, bricks_.size());
        return bricks_[h.id];
    }

    void BrickPool::writeVoxel(BrickHandle h, uint32_t localIndex, uint32_t packed) {
        if (h == kEmptyBrick) {
            return;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        if (localIndex >= Brick::kVoxelCount) {
            throw std::out_of_range("BrickPool::writeVoxel: local voxel index out of range");
        }
        bricks_[h.id].voxels[localIndex] = packed;
        markDirtyLocked_(h);
    }

    uint32_t BrickPool::refCount(BrickHandle h) const {
        if (h == kEmptyBrick) {
            return 0u;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        return refCount_[h.id];
    }

    BrickHandle BrickPool::prepareForWrite(BrickHandle h) {
        if (h == kEmptyBrick) {
            return kEmptyBrick;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        if (refCount_[h.id] > 1u) {
            const BrickHandle newH = allocateSlotLocked_();
            bricks_[newH.id] = bricks_[h.id];
            --refCount_[h.id];
            return newH;
        }
        removeFromDedupLocked_(h);
        return h;
    }

    void BrickPool::removeFromDedupLocked_(BrickHandle h) {
        if (contentHashToHandle_.empty()) {
            return;
        }
        const size_t hash = hashBrick(bricks_[h.id]);
        auto range = contentHashToHandle_.equal_range(hash);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == h) {
                contentHashToHandle_.erase(it);
                return;
            }
        }
    }

    void BrickPool::markDirtyLocked_(BrickHandle h) {
        if (dirtyFlags_[h.id] != 0u) {
            return;
        }
        dirtyFlags_[h.id] = 1u;
        dirtyHandles_.push_back(h);
    }

    void BrickPool::markDirty(BrickHandle h) {
        if (h == kEmptyBrick) {
            return;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        markDirtyLocked_(h);
    }

    std::vector<BrickHandle> BrickPool::drainDirty() {
        std::lock_guard lock(mutex_);
        std::vector<BrickHandle> out;
        out.reserve(dirtyHandles_.size());
        for (BrickHandle h : dirtyHandles_) {
            if (dirtyFlags_[h.id] != 0u) {                  // skip handles freed since markDirty
                out.push_back(h);
                dirtyFlags_[h.id] = 0u;
            }
        }
        dirtyHandles_.clear();
        return out;
    }

    size_t BrickPool::capacity() const {
        std::lock_guard lock(mutex_);
        return bricks_.size();
    }

    size_t BrickPool::allocated() const {
        std::lock_guard lock(mutex_);
        return bricks_.size() - 1u - freeList_.size();
    }

    size_t BrickPool::logicalBricks() const {
        std::lock_guard lock(mutex_);
        size_t total = 0;
        for (size_t i = 1; i < refCount_.size(); ++i) {
            total += static_cast<size_t>(refCount_[i]);
        }
        return total;
    }

}
