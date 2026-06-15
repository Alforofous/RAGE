#pragma once

#include <concepts>
#include <utility>
#include "gpu_queue_kind.hpp"

namespace RAGE {
    template <typename T>
    concept MoveOnly = std::move_constructible<T> && !std::copyable<T>;

    template <typename T>
    concept GpuCommandBuffer = MoveOnly<T>;

    template <typename T>
    concept GpuRecorder = MoveOnly<T>;

    template <typename T>
    concept GpuExecutable = MoveOnly<T>;

    template <typename T>
    concept GpuPendingSubmission = MoveOnly<T>;

    template <typename T>
    concept GpuSubmitInfo = requires(const T info) {
        info.wait;
        info.signal;
    };

    template <typename T>
    concept GpuQueue = requires(T q, const T cq, typename T::Executable exe, typename T::SubmitInfo info) {
        typename T::Kind;
        typename T::Executable;
        typename T::SubmitInfo;
        typename T::PendingSubmission;
        requires QueueKind<typename T::Kind>;
        requires GpuExecutable<typename T::Executable>;
        requires GpuPendingSubmission<typename T::PendingSubmission>;
        requires GpuSubmitInfo<typename T::SubmitInfo>;
        { q.submit(std::move(exe), info) } -> std::same_as<typename T::PendingSubmission>;
        { q.waitIdle() } -> std::same_as<void>;
        { cq.queueFamily() } -> std::convertible_to<uint32_t>;
    };
}
