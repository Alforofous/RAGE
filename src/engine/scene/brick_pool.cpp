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
            if (h == kEmptyBrick || h >= capacity) {
                throw std::out_of_range("BrickPool: invalid BrickHandle");
            }
        }
    }

    BrickPool::BrickPool() {
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
            bricks_[h] = Brick{};
        } else {
            if (bricks_.size() >= kMaxBricks) {
                throw std::runtime_error("BrickPool: exhausted (max bricks = " + std::to_string(kMaxBricks) + ")");
            }
            h = static_cast<BrickHandle>(bricks_.size());
            bricks_.emplace_back();
            refCount_.push_back(0u);
            dirtyFlags_.push_back(0u);
        }
        refCount_[h] = 1u;
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
                if (brickEqual(bricks_[it->second], contents)) {
                    ++refCount_[it->second];
                    return it->second;
                }
            }
            const BrickHandle handle = allocateSlotLocked_();
            bricks_[handle] = contents;
            contentHashToHandle_.insert({ h, handle });
            return handle;
        }
        const BrickHandle handle = allocateSlotLocked_();
        bricks_[handle] = contents;
        return handle;
    }

    void BrickPool::release(BrickHandle h) {
        if (h == kEmptyBrick) {
            return;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        if (refCount_[h] == 0u) {
            return;
        }
        --refCount_[h];
        if (refCount_[h] == 0u) {
            removeFromDedupLocked_(h);
            freeList_.push_back(h);
        }
    }

    const Brick &BrickPool::brick(BrickHandle h) const {
        throwIfInvalid(h, bricks_.size());
        return bricks_[h];
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
        bricks_[h].voxels[localIndex] = packed;
        markDirtyLocked_(h);
    }

    uint32_t BrickPool::refCount(BrickHandle h) const {
        if (h == kEmptyBrick) {
            return 0u;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        return refCount_[h];
    }

    BrickHandle BrickPool::prepareForWrite(BrickHandle h) {
        if (h == kEmptyBrick) {
            return kEmptyBrick;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        if (refCount_[h] > 1u) {
            const BrickHandle newH = allocateSlotLocked_();
            bricks_[newH] = bricks_[h];
            --refCount_[h];
            return newH;
        }
        removeFromDedupLocked_(h);
        return h;
    }

    void BrickPool::removeFromDedupLocked_(BrickHandle h) {
        if (contentHashToHandle_.empty()) {
            return;
        }
        const size_t hash = hashBrick(bricks_[h]);
        auto range = contentHashToHandle_.equal_range(hash);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == h) {
                contentHashToHandle_.erase(it);
                return;
            }
        }
    }

    void BrickPool::markDirtyLocked_(BrickHandle h) {
        if (dirtyFlags_[h] != 0u) {
            return;
        }
        dirtyFlags_[h] = 1u;
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
        for (BrickHandle h : dirtyHandles_) {
            dirtyFlags_[h] = 0u;
        }
        std::vector<BrickHandle> out;
        std::swap(out, dirtyHandles_);
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
            total += refCount_[i];
        }
        return total;
    }

    void BrickPool::setDedupEnabled(bool enabled) {
        std::lock_guard lock(mutex_);
        dedupEnabled_ = enabled;
    }

    bool BrickPool::isDedupEnabled() const {
        std::lock_guard lock(mutex_);
        return dedupEnabled_;
    }
}
