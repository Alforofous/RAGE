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
     * Running it
     * ==========
     * Production is the default build. Profiling is linked only when the `-Dev` / `--dev`
     * flag is passed to the build scripts (or `-DRAGE_DEV_BUILD=ON` direct to CMake):
     *
     *     ./scripts/build.ps1 -Dev       # PowerShell
     *     ./scripts/build.sh --dev       # Bash
     *     cmake -B build -DRAGE_DEV_BUILD=ON
     *
     * Dev build implies Tracy client + the dev-only UI. Production builds (no flag) drop
     * both — clean binary, no Tracy threads, no debug panels.
     *
     * CMake auto-downloads `tracy-profiler.exe` under `libraries/tracy-server/` on Windows
     * (~12 MB one-time fetch from the upstream release).
     *
     * To attach a profiler GUI at runtime: click the **"Launch Tracy"** button in the in-app
     * debug panel. The button is only rendered in dev builds where `Profiler::isLinked()`
     * is true.
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

        /**
         * Label the **calling thread** so its row in the profiler timeline shows this
         * name instead of an auto-generated id. Call from inside the worker exactly
         * once, near its entry. Name pointer needs to outlive the run; string literals
         * are fine. No-op on production builds.
         */
        void setThreadName(const char *name);

        /**
         * Open / close a profiling zone on the **calling thread**. Useful for app-level
         * instrumentation outside the engine's standard phase callbacks — e.g. wrapping
         * a worker-thread asset load so its duration shows up on that thread's Tracy
         * timeline. Pair begin/end on the same thread; nesting is allowed and tracked
         * per-thread.
         *
         * No-ops when this build doesn't link Tracy. The `name` string must remain valid
         * until `endZone()` returns — string literals are fine.
         */
        void beginZone(const char *name);
        void endZone();

        /**
         * RAII bracket around `beginZone()` / `endZone()`. Construct at the start of the
         * region you want timed; the zone auto-closes at scope exit.
         *
         *     {
         *         App::Profiler::Zone z(profiler, "VoxelLoad");
         *         expensiveWork();
         *     }
         */
        class Zone {
        public:
            Zone(Profiler &profiler, const char *name)
                : profiler_(profiler) {
                profiler_.beginZone(name);
            }
            ~Zone() { profiler_.endZone(); }
            Zone(const Zone &) = delete;
            Zone &operator=(const Zone &) = delete;
            Zone(Zone &&) = delete;
            Zone &operator=(Zone &&) = delete;

        private:
            Profiler &profiler_;
        };

        /**
         * True when this build links the profiler client (i.e. configured with
         * -DRAGE_ENABLE_PROFILING=ON). Production builds return false; the app uses this to
         * decide whether to show a "Launch Tracy" button or other dev-only UI. No runtime
         * profiler initialisation is required — this is a build-config probe.
         */
        static bool isLinked();

        /**
         * Spawn the external Tracy profiler GUI as a detached child process, connecting it
         * to this app's profiler client on 127.0.0.1. No-op + warning to stderr when the
         * build isn't linked against Tracy or when a child is already running (use
         * `isProfilerGuiRunning()` to gate UI). The handle is closed automatically when the
         * child exits, so calling this again after the user closes Tracy spawns a fresh one.
         */
        void launchProfilerGui();

        /** True if a previously-spawned Tracy GUI child is still alive. */
        bool isProfilerGuiRunning();

        /**
         * True when the profiler client has an active connection to a server. With Tracy's
         * on-demand mode (RAGE_DEV_BUILD default) zones aren't recorded until a server is
         * connected — call this before kicking off short-lived work you want captured.
         * Returns false on production builds (no client linked).
         */
        bool isConnected() const;

    private:
        // GPU-context handle owned by the wrapper. Kept opaque so we don't leak Tracy types.
        void *gpuContext_ = nullptr;
        // Spawned tracy-profiler.exe child handle (HANDLE on Windows). Opaque void* so this
        // header stays Win32-free. nullptr when no child is alive.
        void *profilerGuiProcess_ = nullptr;
    };
}
