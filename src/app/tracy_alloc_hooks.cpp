// Global operator new / operator delete overrides that report every host-side
// heap allocation to Tracy's memory profiler. Linking this TU turns Tracy's
// "Memory" tab into a live ledger of outstanding allocations across the whole
// process (engine + libraries + ImGui + GLFW + STL containers + ...), which is
// the right diagnostic surface for slow cumulative leaks where per-frame growth
// is too small to spot by inspection.
//
// Gated on RAGE_PROFILING_TRACY (same flag the profiler uses). In production
// builds (flag undefined) the operators forward to plain `malloc` / `free` and
// add no overhead vs the default global new/delete.
//
// Usage from the Tracy GUI:
//   1. Build dev (-DRAGE_DEV_BUILD=ON) so RAGE_PROFILING_TRACY is defined.
//   2. Run the app with --profile, connect Tracy.
//   3. Memory tab → see every allocation grouped by call-site. Sort by "Total
//      allocations" to find leaks (large outstanding count after a few minutes
//      of steady-state means something allocated and didn't free).
//
// Caveats:
//  - All allocations are tracked, including warm-up. Filter for *outstanding*
//    after the first ~5s.
//  - Adds ~5-10% overhead in alloc-heavy code. Fine for diagnostic runs.
//  - Tracy must be compiled with TRACY_ENABLE (the cmake target sets this when
//    RAGE_ENABLE_PROFILING is on).
//  - The CRT's own bootstrap allocations (before main) happen before Tracy is
//    initialised and won't be tracked — that's expected.

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

// Single-object new / delete.
void *operator new(std::size_t size) { return trackedAlloc(size); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    return trackedAllocNoThrow(size);
}
void operator delete(void *p) noexcept { trackedFree(p); }
void operator delete(void *p, std::size_t) noexcept { trackedFree(p); }
void operator delete(void *p, const std::nothrow_t &) noexcept { trackedFree(p); }

// Array new / delete (used by e.g. `new T[N]`).
void *operator new[](std::size_t size) { return trackedAlloc(size); }
void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
    return trackedAllocNoThrow(size);
}
void operator delete[](void *p) noexcept { trackedFree(p); }
void operator delete[](void *p, std::size_t) noexcept { trackedFree(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { trackedFree(p); }
