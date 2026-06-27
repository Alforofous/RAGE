#include "brick_pool.hpp"

#include <stdexcept>
#include <string>

namespace RAGE {
    namespace {
        void throwIfInvalid(BrickHandle h, size_t capacity) {
            if (h == kEmptyBrick || h >= capacity) {
                throw std::out_of_range("BrickPool: invalid BrickHandle");
            }
        }
    }

    BrickPool::BrickPool() {
        // Reserve max capacity upfront — bricks_ never grows past this, so the
        // references returned by `brick()` stay valid across concurrent `allocate()`
        // calls (worker thread). The dirty-list vectors are reserved at the same size
        // so `markDirty` never reallocates either.
        bricks_.reserve(kMaxBricks);
        dirtyFlags_.reserve(kMaxBricks);
        dirtyHandles_.reserve(kMaxBricks);
        bricks_.emplace_back();     // index 0 = sentinel
        dirtyFlags_.push_back(0u);
    }

    BrickHandle BrickPool::allocate() {
        std::lock_guard lock(mutex_);
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
            dirtyFlags_.push_back(0u);
        }
        if (dirtyFlags_[h] == 0u) {
            dirtyFlags_[h] = 1u;
            dirtyHandles_.push_back(h);
        }
        return h;
    }

    void BrickPool::release(BrickHandle h) {
        if (h == kEmptyBrick) {
            return;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        freeList_.push_back(h);
    }

    Brick &BrickPool::brick(BrickHandle h) {
        throwIfInvalid(h, bricks_.size());
        return bricks_[h];
    }

    const Brick &BrickPool::brick(BrickHandle h) const {
        throwIfInvalid(h, bricks_.size());
        return bricks_[h];
    }

    size_t BrickPool::capacity() const {
        std::lock_guard lock(mutex_);
        return bricks_.size();
    }

    size_t BrickPool::allocated() const {
        std::lock_guard lock(mutex_);
        return bricks_.size() - 1u - freeList_.size();
    }

    void BrickPool::markDirty(BrickHandle h) {
        if (h == kEmptyBrick) {
            return;
        }
        std::lock_guard lock(mutex_);
        throwIfInvalid(h, bricks_.size());
        if (dirtyFlags_[h] != 0u) {
            return;
        }
        dirtyFlags_[h] = 1u;
        dirtyHandles_.push_back(h);
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
}
