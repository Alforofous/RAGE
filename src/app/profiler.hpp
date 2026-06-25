#pragma once

#include <string>
#include <vulkan/vulkan.h>

namespace RAGE {
    class Renderer;
}

namespace RAGE::App {
    /**
     * Profiler is the single bridge between the engine and an external profiling library.
     *
     * Design rules
     * ============
     *   1. The Profiler class is the **only** place in the codebase that knows about the
     *      profiling library (Tracy today; a custom backend tomorrow). The engine source
     *      contains zero profiling-library text — `grep -r tracy src/engine/` returns
     *      nothing. Swap the library by rewriting this one wrapper.
     *   2. The Profiler is wired up at the topmost level (main.cpp). Engine components don't
     *      depend on it; they expose **observer callbacks**, and Profiler subscribes to those
     *      callbacks from outside.
     *   3. Adding more granular profiling means drilling a new callback **down** through the
     *      engine to where you want a labelled span — never reaching **up** with a profiler
     *      include.
     *
     * Sampling vs. instrumentation
     * ============================
     * Profiling runs in two complementary modes:
     *
     *   • **Sampling (default).** Tracy's sampling profiler interrupts the program at
     *     ~10 kHz, captures the CPU call stack, resolves it with debug symbols. Every C++
     *     function the CPU spent meaningful time in appears in the flame chart by its real
     *     name, with no code changes. This is the "Valgrind-style" mode — agnostic and
     *     code-free. Sufficient for the great majority of performance work.
     *
     *   • **Zone instrumentation (opt-in).** When sampling isn't enough — a function is too
     *     short, or you want a precisely-labelled span with custom metadata — you add a
     *     callback at the function boundary in the engine and subscribe a Profiler::zone*
     *     method to it from main.cpp. The engine still has no profiler include; it just has
     *     one more observer hook. This is the "drill-down" path described above.
     *
     * GPU profiling
     * =============
     * GPU spans require the active VkCommandBuffer (Tracy issues vkCmdWriteTimestamp queries
     * into it). The engine emits onBeforeGpuPass / onAfterGpuPass callbacks with the
     * command-buffer handle; Profiler subscribes and translates those into GPU zones. Engine
     * still has no Tracy text — it merely hands out a VkCommandBuffer that anyone could use.
     *
     * Wiring at the top
     * =================
     *     App::Profiler prof;            // initialises the profiling library
     *     prof.attach(renderer);         // subscribes Profiler to all engine callbacks
     *     // ... main loop ...
     *     // Tracy server connects, flame chart appears. Or, if RAGE_PROFILING_TRACY is
     *     // unset at build time, attach() is a no-op and there's zero runtime cost.
     *
     * Swapping the library
     * ====================
     * Replace this file's implementation. The header API (attach, plot, frameMark) is what
     * the rest of the codebase sees. Anything Tracy-specific must stay behind the .cpp file.
     */
    class Profiler {
    public:
        Profiler();
        ~Profiler();

        Profiler(const Profiler &) = delete;
        Profiler &operator=(const Profiler &) = delete;
        Profiler(Profiler &&) = delete;
        Profiler &operator=(Profiler &&) = delete;

        // Subscribe the profiler to a Renderer's observer callbacks: frame end + GPU pass
        // boundaries. Idempotent if called once per renderer. Any future engine callbacks
        // that profiling cares about should be subscribed here.
        void attach(Renderer &renderer);

        // Manual frame marker, in case some component is responsible for frame boundaries
        // other than Renderer. Most callers should not need this — attach() takes care of it.
        void frameMark();

        // Plot a scalar value on the timeline (FPS, voxel count, anything). Useful for
        // overlaying numeric series alongside the flame chart.
        void plot(const char *name, double value);

        // Surface a one-line message that appears as a marker on the timeline. Useful for
        // tagging events like "scene-changed", "shader-recompiled".
        void message(const std::string &text);

    private:
        // GPU-context handle owned by the wrapper. Kept opaque so we don't leak Tracy types.
        void *gpuContext_ = nullptr;
    };
}
