#pragma once

#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>
#include "gpu/gpu_command_pool.hpp"
#include "gpu/gpu_queue_kind.hpp"
#include "shared/small_vector.hpp"
#include "queue.hpp"

namespace RAGE::Mocks {
    template <RAGE::QueueKind K> class MockCommandPool {
    public:
        using Kind = K;
        using CommandBuffer = MockCommandBuffer<K>;

        MockCommandPool() = default;

        MockCommandPool(const MockCommandPool &) = delete;
        MockCommandPool &operator=(const MockCommandPool &) = delete;
        MockCommandPool(MockCommandPool &&) = delete;
        MockCommandPool &operator=(MockCommandPool &&) = delete;

        MockCommandBuffer<K> allocate() {
            allocCount_++;
            slots_.push_back({});

            return { this, slots_.size() - 1, generation_ };
        }

        void reset() {
            resetCount_++;
            generation_++;
            outstandingSubmissions_ = 0;
        }

        bool isIdle() const noexcept { return outstandingSubmissions_ == 0; }

        uint32_t allocCount() const { return allocCount_; }
        uint32_t resetCount() const { return resetCount_; }
        uint32_t outstandingSubmissions() const { return outstandingSubmissions_; }

        uint32_t bindPipelineCount() const { return bindPipelineCount_; }
        uint32_t bindDescriptorSetsCount() const { return bindDescriptorSetsCount_; }
        uint32_t dispatchCount() const { return dispatchCount_; }
        uint32_t pipelineBarrierCount() const { return pipelineBarrierCount_; }
        uint32_t copyBufferCount() const { return copyBufferCount_; }
        uint32_t copyBufferToImageCount() const { return copyBufferToImageCount_; }

        uint32_t lastDispatchX() const { return lastDispatchX_; }
        uint32_t lastDispatchY() const { return lastDispatchY_; }
        uint32_t lastDispatchZ() const { return lastDispatchZ_; }
        uint32_t lastBarrierImageCount() const { return lastBarrierImageCount_; }
        uint32_t lastBoundDescriptorCount() const { return lastBoundDescriptorCount_; }

    private:
        friend class MockQueue<K>;
        friend class MockPendingSubmission<K>;
        friend class MockRecorder<K>;

        struct Slot {};

        std::vector<Slot> slots_;
        uint64_t generation_ = 0;
        uint32_t allocCount_ = 0;
        uint32_t resetCount_ = 0;
        uint32_t outstandingSubmissions_ = 0;

        uint32_t bindPipelineCount_ = 0;
        uint32_t bindDescriptorSetsCount_ = 0;
        uint32_t dispatchCount_ = 0;
        uint32_t pipelineBarrierCount_ = 0;
        uint32_t copyBufferCount_ = 0;
        uint32_t copyBufferToImageCount_ = 0;
        uint32_t lastDispatchX_ = 0;
        uint32_t lastDispatchY_ = 0;
        uint32_t lastDispatchZ_ = 0;
        uint32_t lastBarrierImageCount_ = 0;
        uint32_t lastBoundDescriptorCount_ = 0;
    };

    template <RAGE::QueueKind K> MockRecorder<K> MockCommandBuffer<K>::begin() && {
        MockRecorder<K> rec(pool_, slotIndex_, generation_);
        pool_ = nullptr;

        return rec;
    }

    template <RAGE::QueueKind K>
    void MockRecorder<K>::bindPipeline(RAGE::PipelineBindPoint /*bindPoint*/, MockPipelineHandle /*pipeline*/) {
        pool_->bindPipelineCount_++;
    }

    template <RAGE::QueueKind K>
    void MockRecorder<K>::bindDescriptorSets(RAGE::PipelineBindPoint /*bindPoint*/, MockPipelineLayoutHandle /*layout*/,
                                             uint32_t /*firstSet*/, std::span<const MockDescriptorSetHandle> sets) {
        pool_->bindDescriptorSetsCount_++;
        pool_->lastBoundDescriptorCount_ = static_cast<uint32_t>(sets.size());
    }

    template <RAGE::QueueKind K>
    void MockRecorder<K>::dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
        requires Supports<K, queue_kind::Compute>
    {
        pool_->dispatchCount_++;
        pool_->lastDispatchX_ = groupsX;
        pool_->lastDispatchY_ = groupsY;
        pool_->lastDispatchZ_ = groupsZ;
    }

    template <RAGE::QueueKind K>
    void MockRecorder<K>::pipelineBarrier(std::span<const MockImageBarrier> imageBarriers) {
        pool_->pipelineBarrierCount_++;
        pool_->lastBarrierImageCount_ = static_cast<uint32_t>(imageBarriers.size());
    }

    template <RAGE::QueueKind K>
    void MockRecorder<K>::copyBuffer(const MockBuffer & /*src*/, MockBuffer & /*dst*/,
                                     std::span<const RAGE::BufferCopy> /*regions*/) {
        pool_->copyBufferCount_++;
    }

    template <RAGE::QueueKind K>
    void MockRecorder<K>::copyBufferToImage(const MockBuffer & /*src*/, MockImage & /*dst*/,
                                            RAGE::ImageLayout /*dstLayout*/,
                                            std::span<const RAGE::BufferImageCopy> /*regions*/) {
        pool_->copyBufferToImageCount_++;
    }

    template <RAGE::QueueKind K> MockExecutable<K> MockRecorder<K>::end() && {
        MockExecutable<K> exe(pool_, slotIndex_, generation_);
        pool_ = nullptr;

        return exe;
    }

    template <RAGE::QueueKind K>
    MockPendingSubmission<K>::MockPendingSubmission(MockCommandPool<K> *pool, size_t slotIndex, uint64_t generation,
                                                    RAGE::Core::SmallVector<MockSemaphoreHandle, 4> retained)
        : pool_(pool)
        , slotIndex_(slotIndex)
        , generation_(generation)
        , retainedSemaphores_(std::move(retained)) {}

    template <RAGE::QueueKind K> void MockPendingSubmission<K>::drainAndReset() noexcept {
        if (pool_ == nullptr) {
            return;
        }

        if (pool_->generation_ == generation_) {
            pool_->outstandingSubmissions_--;
        }
        pool_ = nullptr;
    }

    template <RAGE::QueueKind K> MockCommandBuffer<K> MockPendingSubmission<K>::wait() && {
        if (pool_ == nullptr) {
            throw std::runtime_error("MockPendingSubmission::wait() on empty submission");
        }
        if (pool_->generation_ != generation_) {
            pool_ = nullptr;
            throw std::runtime_error("MockPendingSubmission::wait() on stale submission (pool was reset)");
        }

        MockCommandPool<K> *const pool = pool_;
        const size_t slotIndex = slotIndex_;
        const uint64_t generation = generation_;

        drainAndReset();

        return { pool, slotIndex, generation };
    }

    template <RAGE::QueueKind K>
    MockPendingSubmission<K> MockQueue<K>::submit(MockExecutable<K> exe, MockSubmitInfo info) {
        submitCount_++;
        lastWaitCount_ = static_cast<uint32_t>(info.wait.size());
        lastSignalCount_ = static_cast<uint32_t>(info.signal.size());

        RAGE::Core::SmallVector<MockSemaphoreHandle, 4> retained;
        retained.reserve(info.wait.size() + info.signal.size());
        for (const MockSemaphoreWait &w : info.wait) {
            retained.push_back(w.semaphore);
        }
        for (const MockSemaphoreHandle &s : info.signal) {
            retained.push_back(s);
        }

        exe.pool_->outstandingSubmissions_++;
        MockPendingSubmission<K> pending(exe.pool_, exe.slotIndex_, exe.generation_, std::move(retained));
        exe.pool_ = nullptr;

        return pending;
    }

    static_assert(GpuCommandBuffer<MockCommandBuffer<queue_kind::Graphics>>);
    static_assert(GpuRecorder<MockRecorder<queue_kind::Graphics>>);
    static_assert(GpuExecutable<MockExecutable<queue_kind::Graphics>>);
    static_assert(GpuPendingSubmission<MockPendingSubmission<queue_kind::Graphics>>);
    static_assert(GpuQueue<MockQueue<queue_kind::Graphics>>, "MockQueue<Graphics> must satisfy GpuQueue concept");
    static_assert(GpuCommandPool<MockCommandPool<queue_kind::Graphics>>,
                  "MockCommandPool<Graphics> must satisfy GpuCommandPool concept");
}
