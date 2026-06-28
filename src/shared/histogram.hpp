#pragma once

#include <array>
#include <cstddef>
#include <limits>

namespace RAGE::Core {
    /**
     * @brief Fixed-capacity ring buffer of numeric samples.
     *
     * `data() + oldestOffset() + size()` is the shape `ImGui::PlotLines/PlotHistogram`
     * expects, so callers can render the ring without copying. `mean()` overflows
     * silently for integer T — pick a wide enough T at the call site.
     */
    template <typename T = float, size_t N = 128> class Histogram {
        static_assert(N > 0, "Histogram capacity must be positive");

    public:
        using value_type = T;

        static constexpr size_t capacity() noexcept { return N; }
        size_t size() const noexcept { return count_; }
        bool empty() const noexcept { return count_ == 0; }
        bool full() const noexcept { return count_ == N; }

        void push(T value) noexcept {
            ring_[head_] = value;
            head_ = (head_ + 1) % N;
            if (count_ < N) {
                ++count_;
            }
        }

        void clear() noexcept {
            head_ = 0;
            count_ = 0;
        }

        T latest() const noexcept {
            if (count_ == 0) {
                return T{};
            }
            return ring_[(head_ + N - 1) % N];
        }

        const T *data() const noexcept { return ring_.data(); }

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
