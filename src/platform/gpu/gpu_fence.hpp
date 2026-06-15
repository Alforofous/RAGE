#pragma once

#include <concepts>
#include "gpu_queue.hpp"

namespace RAGE {
    template <typename T>
    concept GpuFenceHandle = requires(T handle, const T cHandle) {
        { handle.wait() };
        { cHandle.isSignaled() } -> std::convertible_to<bool>;
        { cHandle.isValid() } -> std::convertible_to<bool>;
    } && MoveOnly<T> && std::default_initializable<T>;

    /**
     * Acquires move-only GPU fence handles from a backing pool.
     *
     * Implementations are single-threaded by contract: a pool instance and all handles
     * produced from it must be used from a single thread. Multi-threaded submission is
     * supported by using one pool per submitting thread, not by synchronising one pool.
     *
     * @param T Concrete pool type.
     */
    template <typename T>
    concept GpuFencePool = requires(T pool) {
        typename T::Handle;
        requires GpuFenceHandle<typename T::Handle>;
        { pool.acquire() } -> std::same_as<typename T::Handle>;
    };
}
