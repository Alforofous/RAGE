#pragma once

#include <concepts>
#include "gpu_queue.hpp"

namespace RAGE {
    /**
     * Allocates command buffers from a backing pool, tagged by queue kind.
     *
     * Implementations are single-threaded by contract: a pool instance and all command
     * buffers it produces (plus their downstream Recorder/Executable/PendingSubmission
     * stages) must be used from a single thread. Multi-threaded recording is supported
     * by using one pool per recording thread.
     *
     * @param T Concrete pool type. Must expose `Kind` and `CommandBuffer` associated types.
     */
    template <typename T>
    concept GpuCommandPool = requires(T pool, const T cpool) {
        typename T::Kind;
        typename T::CommandBuffer;
        requires QueueKind<typename T::Kind>;
        requires GpuCommandBuffer<typename T::CommandBuffer>;
        { pool.allocate() } -> std::same_as<typename T::CommandBuffer>;
        { pool.reset() } -> std::same_as<void>;
        { cpool.isIdle() } -> std::convertible_to<bool>;
    };

    template <typename T, typename K>
    concept GpuCommandPoolFor = GpuCommandPool<T> && std::same_as<typename T::Kind, K>;
}
