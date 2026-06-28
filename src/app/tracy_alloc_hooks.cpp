// Global operator new/delete overrides that report every host heap allocation
// to Tracy's Memory tab. Gated on RAGE_PROFILING_TRACY; in production the
// operators forward straight to malloc/free with no overhead.

#include <cstdlib>
#include <new>

#ifdef RAGE_PROFILING_TRACY
    #include <tracy/Tracy.hpp>
#endif

namespace {
    void *trackedAlloc(std::size_t size) {
        void *p = std::malloc(size);
        if (p == nullptr) {
            throw std::bad_alloc{};
        }
#ifdef RAGE_PROFILING_TRACY
        TracyAlloc(p, size);
#endif
        return p;
    }

    void *trackedAllocNoThrow(std::size_t size) noexcept {
        void *p = std::malloc(size);
#ifdef RAGE_PROFILING_TRACY
        if (p != nullptr) {
            TracyAlloc(p, size);
        }
#endif
        return p;
    }

    void trackedFree(void *p) noexcept {
        if (p == nullptr) {
            return;
        }
#ifdef RAGE_PROFILING_TRACY
        TracyFree(p);
#endif
        std::free(p);
    }
}

void *operator new(std::size_t size) { return trackedAlloc(size); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    return trackedAllocNoThrow(size);
}
void operator delete(void *p) noexcept { trackedFree(p); }
void operator delete(void *p, std::size_t) noexcept { trackedFree(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { trackedFree(p); }

void *operator new[](std::size_t size) { return trackedAlloc(size); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
    return trackedAllocNoThrow(size);
}
void operator delete[](void *p) noexcept { trackedFree(p); }
void operator delete[](void *p, std::size_t) noexcept { trackedFree(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { trackedFree(p); }
