#pragma once

#include <array>
#include <cstddef>
#include <limits>

namespace RAGE::Core {
    /**
     * Fixed-capacity ring buffer of numeric samples — the data primitive behind the
     * frame-time / memory plots in the debug UI.
     *
     * Optimised for two read patterns:
     *   - **Time-series plot**: caller wants a pointer + length + offset so it can
     *     hand the ring to a UI plot widget (e.g. ImGui::PlotLines) without copying.
     *     See `data()` + `oldestOffset()`.
     *   - **Aggregates**: `min()`, `max()`, `mean()` walk the live samples each call.
     *     For the small N we use (~128) this is cache-bound and cheap; if a hotter
     *     UI ever needs them faster we can swap to incremental tracking, but profile
     *     first.
     *
     * Header-only, no allocations after construction. Capacity is a template
     * parameter so the compiler can inline storage; safe to embed in structs.
     *
     * @tparam T Sample value type. Defaults to float; any arithmetic type works.
     * @tparam N Ring capacity in samples (compile-time, must be > 0).
     */
    template <typename T = float, size_t N = 128> class Histogram {
        static_assert(N > 0, "Histogram capacity must be positive");

    public:
        using value_type = T;

        static constexpr size_t capacity() noexcept { return N; }

        /** Number of samples currently held; grows from 0 to N as samples are pushed. */
        size_t size() const noexcept { return count_; }
        bool empty() const noexcept { return count_ == 0; }
        bool full() const noexcept { return count_ == N; }

        /**
         * Append a new sample. When the ring is full, this overwrites the oldest
         * sample. Constant time.
         */
        void push(T value) noexcept {
            ring_[head_] = value;
            head_ = (head_ + 1) % N;
            if (count_ < N) {
                ++count_;
            }
        }

        /** Reset to empty state; existing storage is retained but inaccessible. */
        void clear() noexcept {
            head_ = 0;
            count_ = 0;
        }

        /** The most recently pushed sample. Calling on `empty()` returns `T{}`. */
        T latest() const noexcept {
            if (count_ == 0) {
                return T{};
            }
            return ring_[(head_ + N - 1) % N];
        }

        /**
         * Pointer to the underlying ring storage. Always of size N. Combined with
         * `oldestOffset()` and `size()` this lets a UI plot widget render the live
         * samples in chronological order without copying.
         *
         * Example (ImGui):
         *   ImGui::PlotLines(label, h.data(), int(h.size()),
         *                    int(h.oldestOffset()), ...);
         */
        const T *data() const noexcept { return ring_.data(); }

        /**
         * Index of the oldest live sample within the ring. Together with `size()`
         * this lets a consumer walk samples in chronological order via
         * `ring_[(oldestOffset + i) % N]` for i in [0, size).
         */
        size_t oldestOffset() const noexcept {
            if (count_ < N) {
                return 0;
            }
            return head_;
        }

        T min() const noexcept {
            if (count_ == 0) {
                return T{};
            }
            T m = std::numeric_limits<T>::max();
            forEach([&m](T v) {
                if (v < m) {
                    m = v;
                }
            });
            return m;
        }

        T max() const noexcept {
            if (count_ == 0) {
                return T{};
            }
            T m = std::numeric_limits<T>::lowest();
            forEach([&m](T v) {
                if (v > m) {
                    m = v;
                }
            });
            return m;
        }

        /**
         * Arithmetic mean of all live samples. Returns `T{}` when empty. For
         * integer T, accumulation is in T's type — caller should pick a wide
         * enough T if overflow is a concern.
         */
        T mean() const noexcept {
            if (count_ == 0) {
                return T{};
            }
            T sum{};
            forEach([&sum](T v) { sum += v; });
            return sum / static_cast<T>(count_);
        }

    private:
        template <typename F> void forEach(F fn) const noexcept {
            const size_t start = oldestOffset();
            for (size_t i = 0; i < count_; ++i) {
                fn(ring_[(start + i) % N]);
            }
        }

        std::array<T, N> ring_{};
        size_t head_ = 0;
        size_t count_ = 0;
    };
}
