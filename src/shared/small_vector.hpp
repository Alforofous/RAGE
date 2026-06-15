#pragma once

#include <cassert>
#include <cstddef>
#include <new>
#include <utility>

namespace RAGE::Core {
    /**
     * Contiguous container with inline storage for the first N elements.
     *
     * Behaves like `std::vector` but skips the heap allocation while size stays
     * at or below N. The object itself is `N * sizeof(T)` bytes larger than a
     * `std::vector`. Once `size() > N`, contents spill to a heap allocation and
     * capacity doubles on growth.
     *
     * @param T Element type. Must be move-constructible.
     * @param N Number of inline slots before spilling to the heap.
     *
     * @note Move-only. Hot-path buffer where copies signal misuse, so they're
     *       deleted.
     */
    template <typename T, size_t N> class SmallVector {
    public:
        SmallVector() = default;

        ~SmallVector() {
            clear();
            if (heap_ != nullptr) {
                ::operator delete(heap_, std::align_val_t{ alignof(T) });
            }
        }

        SmallVector(const SmallVector &) = delete;
        SmallVector &operator=(const SmallVector &) = delete;

        SmallVector(SmallVector &&other) noexcept { moveFrom(other); }

        SmallVector &operator=(SmallVector &&other) noexcept {
            if (this != &other) {
                clear();
                if (heap_ != nullptr) {
                    ::operator delete(heap_, std::align_val_t{ alignof(T) });
                    heap_ = nullptr;
                    capacity_ = N;
                }
                moveFrom(other);
            }

            return *this;
        }

        void reserve(size_t n) {
            if (n > capacity_) {
                growTo(n);
            }
        }

        void push_back(const T &v) {
            if (size_ == capacity_) {
                growTo(capacity_ * 2);
            }
            new (data() + size_) T(v);
            size_++;
        }

        void push_back(T &&v) {
            if (size_ == capacity_) {
                growTo(capacity_ * 2);
            }
            new (data() + size_) T(std::move(v));
            size_++;
        }

        void clear() noexcept {
            for (size_t i = 0; i < size_; i++) {
                data()[i].~T();
            }
            size_ = 0;
        }

        size_t size() const noexcept { return size_; }
        bool empty() const noexcept { return size_ == 0; }

        T *data() noexcept { return heap_ != nullptr ? heap_ : reinterpret_cast<T *>(inline_); }
        const T *data() const noexcept { return heap_ != nullptr ? heap_ : reinterpret_cast<const T *>(inline_); }

        T *begin() noexcept { return data(); }
        T *end() noexcept { return data() + size_; }
        const T *begin() const noexcept { return data(); }
        const T *end() const noexcept { return data() + size_; }

        T &operator[](size_t i) noexcept { return data()[i]; }
        const T &operator[](size_t i) const noexcept { return data()[i]; }

    private:
        void growTo(size_t newCap) {
            assert(newCap > capacity_);
            T *newBuf = static_cast<T *>(::operator new(newCap * sizeof(T), std::align_val_t{ alignof(T) }));
            T *oldBuf = data();
            for (size_t i = 0; i < size_; i++) {
                new (newBuf + i) T(std::move(oldBuf[i]));
                oldBuf[i].~T();
            }
            if (heap_ != nullptr) {
                ::operator delete(heap_, std::align_val_t{ alignof(T) });
            }
            heap_ = newBuf;
            capacity_ = newCap;
        }

        void moveFrom(SmallVector &other) noexcept {
            if (other.heap_ != nullptr) {
                heap_ = other.heap_;
                capacity_ = other.capacity_;
                size_ = other.size_;
                other.heap_ = nullptr;
                other.capacity_ = N;
                other.size_ = 0;
            } else {
                for (size_t i = 0; i < other.size_; i++) {
                    new (reinterpret_cast<T *>(inline_) + i) T(std::move(other.data()[i]));
                    other.data()[i].~T();
                }
                size_ = other.size_;
                other.size_ = 0;
            }
        }

        alignas(T) std::byte inline_[N * sizeof(T)]{};
        T *heap_ = nullptr;
        size_t size_ = 0;
        size_t capacity_ = N;
    };
}
