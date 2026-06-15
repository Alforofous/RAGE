#pragma once

#include <concepts>

namespace RAGE {
    template <typename T>
    concept GpuSemaphoreHandle = std::copyable<T> && requires(const T handle) {
        { handle.isValid() } -> std::convertible_to<bool>;
    };

    /**
     * Acquires refcounted GPU semaphore handles from a backing pool.
     *
     * Implementations are single-threaded by contract: a pool instance and all handles
     * produced from it must be used from a single thread. Acquiring on thread A while a
     * handle is dropped on thread B is undefined behaviour. Multi-threaded recording is
     * supported by using one pool per recording thread, not by synchronising one pool.
     *
     * @param T Concrete pool type.
     */
    template <typename T>
    concept GpuSemaphorePool = requires(T pool) {
        typename T::Handle;
        requires GpuSemaphoreHandle<typename T::Handle>;
        { pool.acquire() } -> std::same_as<typename T::Handle>;
    };
}
