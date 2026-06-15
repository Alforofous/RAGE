#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include "gpu/gpu_queue.hpp"
#include "gpu/gpu_queue_kind.hpp"
#include "gpu/gpu_types.hpp"
#include "shared/small_vector.hpp"
#include "allocator.hpp"
#include "semaphore_pool.hpp"

namespace RAGE::Mocks {
    template <RAGE::QueueKind K> class MockCommandPool;
    template <RAGE::QueueKind K> class MockQueue;
    template <RAGE::QueueKind K> class MockRecorder;
    template <RAGE::QueueKind K> class MockExecutable;
    template <RAGE::QueueKind K> class MockPendingSubmission;

    template <RAGE::QueueKind K> class MockCommandBuffer {
    public:
        using Kind = K;

        MockCommandBuffer() = delete;

        MockCommandBuffer(const MockCommandBuffer &) = delete;
        MockCommandBuffer &operator=(const MockCommandBuffer &) = delete;

        MockCommandBuffer(MockCommandBuffer &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {}

        MockCommandBuffer &operator=(MockCommandBuffer &&other) noexcept {
            if (this != &other) {
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
            }

            return *this;
        }

        MockRecorder<K> begin() &&;

    private:
        friend class MockCommandPool<K>;
        friend class MockPendingSubmission<K>;

        MockCommandBuffer(MockCommandPool<K> *pool, size_t slotIndex, uint64_t generation)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation) {}

        MockCommandPool<K> *pool_ = nullptr;
        size_t slotIndex_ = 0;
        uint64_t generation_ = 0;
    };

    struct MockImageBarrier {
        MockImage *image = nullptr;
        RAGE::ImageLayout oldLayout = RAGE::ImageLayout::Undefined;
        RAGE::ImageLayout newLayout = RAGE::ImageLayout::Undefined;
        RAGE::PipelineStage srcStage = RAGE::PipelineStage::TopOfPipe;
        RAGE::PipelineStage dstStage = RAGE::PipelineStage::BottomOfPipe;
        RAGE::AccessFlags srcAccess = RAGE::AccessFlags::None;
        RAGE::AccessFlags dstAccess = RAGE::AccessFlags::None;
        uint32_t baseMipLevel = 0;
        uint32_t mipCount = 1;
        uint32_t baseArrayLayer = 0;
        uint32_t layerCount = 1;
    };

    struct MockPipelineHandle {
        uint32_t id = 0;
    };

    struct MockPipelineLayoutHandle {
        uint32_t id = 0;
    };

    struct MockDescriptorSetHandle {
        uint32_t id = 0;
    };

    template <RAGE::QueueKind K> class MockRecorder {
    public:
        using Kind = K;

        MockRecorder() = delete;
        MockRecorder(const MockRecorder &) = delete;
        MockRecorder &operator=(const MockRecorder &) = delete;

        MockRecorder(MockRecorder &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {}

        MockRecorder &operator=(MockRecorder &&other) noexcept {
            if (this != &other) {
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
            }

            return *this;
        }

        MockExecutable<K> end() &&;

        void bindPipeline(RAGE::PipelineBindPoint bindPoint, MockPipelineHandle pipeline);
        void bindDescriptorSets(RAGE::PipelineBindPoint bindPoint, MockPipelineLayoutHandle layout, uint32_t firstSet,
                                std::span<const MockDescriptorSetHandle> sets);
        void dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
            requires Supports<K, queue_kind::Compute>;
        void pipelineBarrier(std::span<const MockImageBarrier> imageBarriers);
        void copyBuffer(const MockBuffer &src, MockBuffer &dst, std::span<const RAGE::BufferCopy> regions);
        void copyBufferToImage(const MockBuffer &src, MockImage &dst, RAGE::ImageLayout dstLayout,
                               std::span<const RAGE::BufferImageCopy> regions);

    private:
        friend class MockCommandBuffer<K>;

        MockRecorder(MockCommandPool<K> *pool, size_t slotIndex, uint64_t generation)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation) {}

        MockCommandPool<K> *pool_ = nullptr;
        size_t slotIndex_ = 0;
        uint64_t generation_ = 0;
    };

    template <RAGE::QueueKind K> class MockExecutable {
    public:
        using Kind = K;

        MockExecutable() = delete;
        MockExecutable(const MockExecutable &) = delete;
        MockExecutable &operator=(const MockExecutable &) = delete;

        MockExecutable(MockExecutable &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_) {}

        MockExecutable &operator=(MockExecutable &&other) noexcept {
            if (this != &other) {
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
            }

            return *this;
        }

    private:
        friend class MockRecorder<K>;
        friend class MockQueue<K>;

        MockExecutable(MockCommandPool<K> *pool, size_t slotIndex, uint64_t generation)
            : pool_(pool)
            , slotIndex_(slotIndex)
            , generation_(generation) {}

        MockCommandPool<K> *pool_ = nullptr;
        size_t slotIndex_ = 0;
        uint64_t generation_ = 0;
    };

    struct MockSemaphoreWait {
        MockSemaphoreHandle semaphore;
        uint32_t stage = 0;
    };

    struct MockSubmitInfo {
        std::span<const MockSemaphoreWait> wait = {};
        std::span<const MockSemaphoreHandle> signal = {};
    };

    template <RAGE::QueueKind K> class MockPendingSubmission {
    public:
        using Kind = K;

        MockPendingSubmission() = delete;
        MockPendingSubmission(const MockPendingSubmission &) = delete;
        MockPendingSubmission &operator=(const MockPendingSubmission &) = delete;

        MockPendingSubmission(MockPendingSubmission &&other) noexcept
            : pool_(std::exchange(other.pool_, nullptr))
            , slotIndex_(other.slotIndex_)
            , generation_(other.generation_)
            , retainedSemaphores_(std::move(other.retainedSemaphores_)) {}

        MockPendingSubmission &operator=(MockPendingSubmission &&other) noexcept {
            if (this != &other) {
                drainAndReset();
                pool_ = std::exchange(other.pool_, nullptr);
                slotIndex_ = other.slotIndex_;
                generation_ = other.generation_;
                retainedSemaphores_ = std::move(other.retainedSemaphores_);
            }

            return *this;
        }

        ~MockPendingSubmission() { drainAndReset(); }

        MockCommandBuffer<K> wait() &&;

    private:
        friend class MockQueue<K>;

        MockPendingSubmission(MockCommandPool<K> *pool, size_t slotIndex, uint64_t generation,
                              RAGE::Core::SmallVector<MockSemaphoreHandle, 4> retained);

        void drainAndReset() noexcept;

        MockCommandPool<K> *pool_ = nullptr;
        size_t slotIndex_ = 0;
        uint64_t generation_ = 0;
        RAGE::Core::SmallVector<MockSemaphoreHandle, 4> retainedSemaphores_;
    };

    template <RAGE::QueueKind K> class MockQueue {
    public:
        using Kind = K;
        using CommandBuffer = MockCommandBuffer<K>;
        using Executable = MockExecutable<K>;
        using Semaphore = MockSemaphoreHandle;
        using SubmitInfo = MockSubmitInfo;
        using PendingSubmission = MockPendingSubmission<K>;

        MockQueue() = default;

        MockQueue(const MockQueue &) = delete;
        MockQueue &operator=(const MockQueue &) = delete;
        MockQueue(MockQueue &&) = default;
        MockQueue &operator=(MockQueue &&) = default;

        MockPendingSubmission<K> submit(MockExecutable<K> exe, MockSubmitInfo info = {});

        void waitIdle() { idleCount_++; }

        uint32_t queueFamily() const { return 0; }

        uint32_t submitCount() const { return submitCount_; }
        uint32_t idleCount() const { return idleCount_; }
        uint32_t lastWaitCount() const { return lastWaitCount_; }
        uint32_t lastSignalCount() const { return lastSignalCount_; }

    private:
        uint32_t submitCount_ = 0;
        uint32_t idleCount_ = 0;
        uint32_t lastWaitCount_ = 0;
        uint32_t lastSignalCount_ = 0;
    };
}
