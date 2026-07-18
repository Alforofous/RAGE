#pragma once

#include <cstdint>

namespace RAGE::Core {
    /**
     * @brief Engine-side profiling trampoline. The engine never links a profiler —
     *        it emits zones/plots through these function pointers, which stay null
     *        (every call a no-op) until the application installs a backend
     *        (see `src/app/profiler.cpp`, the only Tracy-aware translation unit).
     *
     * Same philosophy as the renderer's PhaseHook callbacks, generalized so any
     * engine component — including worker threads — can instrument itself without
     * knowing who listens. All strings must be literals or otherwise outlive the
     * zone (the backend may keep the pointer).
     */
    struct ProfileHooks {
        void (*zoneBegin)(const char *name) = nullptr;
        void (*zoneEnd)() = nullptr;
        void (*threadName)(const char *name) = nullptr;
        void (*plot)(const char *name, double value) = nullptr;
        /// End-of-frame marker (frame boundaries in the profiler timeline).
        void (*frameMark)() = nullptr;
        /// Paired GPU-pass zone around command recording. `commandBuffer` is the
        /// backend's raw handle (VkCommandBuffer), opaque here by design.
        void (*gpuPassBegin)(const char *name, void *commandBuffer) = nullptr;
        void (*gpuPassEnd)() = nullptr;
        /// Frame thumbnail (RGBA8) for the profiler's frame browser. Emitters must
        /// skip the (expensive) capture entirely while this is null.
        void (*frameImage)(const void *rgbaPixels, uint16_t width, uint16_t height) = nullptr;
    };

    inline ProfileHooks gProfileHooks{};

    /// Name the calling thread in the profiler timeline (no-op without a backend).
    inline void profileThreadName(const char *name) {
        if (gProfileHooks.threadName != nullptr) {
            gProfileHooks.threadName(name);
        }
    }

    /// Emit a named sample onto a profiler plot (no-op without a backend).
    inline void profilePlot(const char *name, double value) {
        if (gProfileHooks.plot != nullptr) {
            gProfileHooks.plot(name, value);
        }
    }

    /// Mark a frame boundary (no-op without a backend).
    inline void profileFrameMark() {
        if (gProfileHooks.frameMark != nullptr) {
            gProfileHooks.frameMark();
        }
    }

    /// Begin/end a GPU-pass zone around command recording (no-ops without a backend).
    inline void profileGpuPassBegin(const char *name, void *commandBuffer) {
        if (gProfileHooks.gpuPassBegin != nullptr) {
            gProfileHooks.gpuPassBegin(name, commandBuffer);
        }
    }
    inline void profileGpuPassEnd() {
        if (gProfileHooks.gpuPassEnd != nullptr) {
            gProfileHooks.gpuPassEnd();
        }
    }

    /// True while a backend wants frame thumbnails — check BEFORE capturing.
    inline bool profileFrameImageWanted() { return gProfileHooks.frameImage != nullptr; }

    /// Deliver a captured frame thumbnail (no-op without a backend).
    inline void profileFrameImage(const void *rgbaPixels, uint16_t width, uint16_t height) {
        if (gProfileHooks.frameImage != nullptr) {
            gProfileHooks.frameImage(rgbaPixels, width, height);
        }
    }

    /// RAII profiling zone; nests per thread. `name` must be a string literal.
    class ProfileZone {
    public:
        explicit ProfileZone(const char *name) {
            if (gProfileHooks.zoneBegin != nullptr) {
                gProfileHooks.zoneBegin(name);
                active_ = true;
            }
        }
        ~ProfileZone() {
            if (active_ && gProfileHooks.zoneEnd != nullptr) {
                gProfileHooks.zoneEnd();
            }
        }
        ProfileZone(const ProfileZone &) = delete;
        ProfileZone &operator=(const ProfileZone &) = delete;
        ProfileZone(ProfileZone &&) = delete;
        ProfileZone &operator=(ProfileZone &&) = delete;

    private:
        bool active_ = false;
    };
}
