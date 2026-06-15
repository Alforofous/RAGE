#pragma once

#include <cstdint>
#include <utility>
#include <vector>
#include "gpu/gpu_semaphore_pool.hpp"

namespace RAGE::Mocks {
    class MockSemaphorePool;

    class MockSemaphoreHandle {
    public:
        MockSemaphoreHandle() = default;
        ~MockSemaphoreHandle() noexcept { release(); }

        MockSemaphoreHandle(const MockSemaphoreHandle &other) noexcept
            : pool_(other.pool_)
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {
            retain();
        }

        MockSemaphoreHandle(MockSemaphoreHandle &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {}

        MockSemaphoreHandle &operator=(const MockSemaphoreHandle &other) noexcept {
            if (this != &other) {
                release();
                pool_ = other.pool_;
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
                retain();
            }

            return *this;
        }

        MockSemaphoreHandle &operator=(MockSemaphoreHandle &&other) noexcept {
            if (this != &other) {
                release();
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
            }

            return *this;
        }

        bool isValid() const { return pool_ != nullptr; }

    private:
        friend class MockSemaphorePool;

        MockSemaphoreHandle(MockSemaphorePool *pool, uint32_t slotIndex, uint64_t generation)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation) {}

        void retain() noexcept;
        void release() noexcept;

        MockSemaphorePool *pool_ = nullptr;
        uint32_t slotIndex_ = 0;
        uint64_t generation_ = 0;
    };

    class MockSemaphorePool {
    public:
        using Handle = MockSemaphoreHandle;

        static constexpr uint32_t kInvalidSlot = ~0u;

        MockSemaphorePool() = default;

        MockSemaphorePool(const MockSemaphorePool &) = delete;
        MockSemaphorePool &operator=(const MockSemaphorePool &) = delete;
        MockSemaphorePool(MockSemaphorePool &&) = delete;
        MockSemaphorePool &operator=(MockSemaphorePool &&) = delete;

        MockSemaphoreHandle acquire() {
            uint32_t slotIndex = 0;
            if (freeHead_ != kInvalidSlot) {
                slotIndex = freeHead_;
                freeHead_ = slots_[slotIndex].nextFreeIdx;
            } else {
                slots_.push_back({ .generation = 0, .refcount = 0, .nextFreeIdx = kInvalidSlot });
                slotIndex = static_cast<uint32_t>(slots_.size() - 1);
            }
            slots_[slotIndex].refcount = 1;

            return { this, slotIndex, slots_[slotIndex].generation };
        }

        uint32_t activeSlotCount() const {
            uint32_t count = 0;
            for (const Slot &s : slots_) {
                if (s.refcount > 0) {
                    count++;
                }
            }

            return count;
        }

    private:
        friend class MockSemaphoreHandle;

        struct Slot {
            uint64_t generation = 0;
            uint32_t refcount = 0;
            uint32_t nextFreeIdx = kInvalidSlot;
        };

        void retainSlot(uint32_t slotIndex) noexcept { slots_[slotIndex].refcount++; }
        void releaseSlot(uint32_t slotIndex) noexcept {
            if (--slots_[slotIndex].refcount == 0) {
                slots_[slotIndex].generation++;
                slots_[slotIndex].nextFreeIdx = freeHead_;
                freeHead_ = slotIndex;
            }
        }

        std::vector<Slot> slots_;
        uint32_t freeHead_ = kInvalidSlot;
    };

    static_assert(GpuSemaphorePool<MockSemaphorePool>, "MockSemaphorePool must satisfy GpuSemaphorePool concept");

    inline void MockSemaphoreHandle::retain() noexcept {
        if (pool_ != nullptr) {
            pool_->retainSlot(slotIndex_);
        }
    }

    inline void MockSemaphoreHandle::release() noexcept {
        if (pool_ != nullptr) {
            pool_->releaseSlot(slotIndex_);
            pool_ = nullptr;
        }
    }
}
