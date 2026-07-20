#include "profiler.hpp"
#include <chrono>
#include <cstdio>
#include <thread>
#include <cstdlib>
#include <string>
#include "app/build_paths.hpp"
#include "shared/profiling.hpp"

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

// =============================================================================================
// Tracy integration lives entirely inside this translation unit. To enable Tracy:
//   1. Vendor the library under libraries/tracy (single-header public/ + TracyClient.cpp).
//   2. Add TracyClient.cpp to the engine sources in CMakeLists.txt.
//   3. Define RAGE_PROFILING_TRACY when compiling this file (target_compile_definitions on
//      the RAGE executable target, gated by an option(RAGE_ENABLE_PROFILING ...) flag).
//   4. Run Tracy.exe alongside RAGE.exe and click "Connect".
//
// When RAGE_PROFILING_TRACY is undefined (the default), every method below is a no-op and
// the binary does not link against Tracy.
// =============================================================================================

#ifdef RAGE_PROFILING_TRACY
    #include <cstring>
    #include <memory>
    #include <vector>
    #include <vulkan/vulkan.h>
    #include <tracy/Tracy.hpp>
    #include <tracy/TracyC.h>
    #include <tracy/TracyVulkan.hpp>
#endif

namespace {
#ifdef RAGE_PROFILING_TRACY
    // Per-thread nesting stack of Tracy zone contexts. PhaseScope (engine-side) fires paired
    // begin/end callbacks; we push the Tracy ctx on begin and pop+end on the matching end.
    // Thread-local because zones nest only within a single thread's call stack.
    thread_local std::vector<TracyCZoneCtx> g_zoneStack;

    // GPU zone stack for the paired gpuPassBegin / gpuPassEnd hooks. VkCtxScope
    // writes its begin timestamp into the command buffer at construction and the end
    // timestamp at destruction, so the scope must stay alive across the pass recording —
    // heap-held here, destroyed on the matching end hook.
    std::vector<std::unique_ptr<tracy::VkCtxScope>> g_gpuZoneStack;

    // Tracy GPU context (TracyVkCtx). File-scope because the trampoline backends are free
    // functions. Currently never created (GPU zones are latent until a context exists);
    // Profiler's destructor releases it if some future path creates one.
    void *g_gpuContext = nullptr;

    // Backend for the engine's Core::ProfileHooks trampoline (shared/profiling.hpp):
    // same emitters as Profiler::beginZone/endZone, callable from any engine thread.
    void coreZoneBegin(const char *name) {
        const size_t nameLen = std::strlen(name);
        auto srcLoc =
            ___tracy_alloc_srcloc_name(0, nullptr, 0, nullptr, 0, name, nameLen, 0xffaaaaee);
        g_zoneStack.push_back(___tracy_emit_zone_begin_alloc(srcLoc, 1));
    }
    void coreZoneEnd() {
        if (!g_zoneStack.empty()) {
            ___tracy_emit_zone_end(g_zoneStack.back());
            g_zoneStack.pop_back();
        }
    }
    void coreThreadName(const char *name) { ___tracy_set_thread_name(name); }
    void corePlot(const char *name, double value) { TracyPlot(name, value); }
    void coreFrameMark() { FrameMark; }
    void coreGpuPassBegin(const char *name, void *commandBuffer) {
        if (g_gpuContext != nullptr) {
            g_gpuZoneStack.push_back(std::make_unique<tracy::VkCtxScope>(
                static_cast<TracyVkCtx>(g_gpuContext), TracyLine, TracyFile, strlen(TracyFile),
                TracyFunction, strlen(TracyFunction), name, strlen(name),
                static_cast<VkCommandBuffer>(commandBuffer), true));
        }
    }
    void coreGpuPassEnd() {
        if (g_gpuContext != nullptr && !g_gpuZoneStack.empty()) {
            g_gpuZoneStack.pop_back();
        }
    }
    void coreFrameImage(const void *rgbaPixels, uint16_t width, uint16_t height) {
        // offset = 0: the image is for the most recently signaled frame (the one whose
        // FrameMark just fired at the end of the previous render() call). The pixels come
        // from that frame's render — the renderer waited for GPU completion via
        // drainInFlight before reading the staging buffer.
        FrameImage(rgbaPixels, width, height, 0, false);
    }
#endif
}

namespace RAGE::App {
    Profiler::Profiler() {
#ifdef RAGE_PROFILING_TRACY
        Core::gProfileHooks = Core::ProfileHooks{
            .zoneBegin = &coreZoneBegin,
            .zoneEnd = &coreZoneEnd,
            .threadName = &coreThreadName,
            .plot = &corePlot,
            .frameMark = &coreFrameMark,
            .gpuPassBegin = &coreGpuPassBegin,
            .gpuPassEnd = &coreGpuPassEnd,
            .frameImage = &coreFrameImage,
        };
        std::fprintf(stdout, "[profiler] Tracy client v0.13.1 linked, on-demand. Connect Tracy.exe to attach.\n");
        std::fflush(stdout);
#else
        std::fprintf(stdout, "[profiler] disabled (rebuild with `-Dev` / `--dev`, "
                             "or pass -DRAGE_DEV_BUILD=ON to cmake).\n");
        std::fflush(stdout);
#endif
    }

    Profiler::~Profiler() {
#ifdef RAGE_PROFILING_TRACY
        if (g_gpuContext != nullptr) {
            TracyVkDestroy(static_cast<TracyVkCtx>(g_gpuContext));
            g_gpuContext = nullptr;
        }
#endif
#ifdef _WIN32
        if (profilerGuiProcess_ != nullptr) {
            CloseHandle(static_cast<HANDLE>(profilerGuiProcess_));
            profilerGuiProcess_ = nullptr;
        }
#endif
    }

    void Profiler::frameMark() {
#ifdef RAGE_PROFILING_TRACY
        FrameMark;
#endif
    }

    void Profiler::plot(const char *name, double value) {
#ifdef RAGE_PROFILING_TRACY
        TracyPlot(name, value);
#else
        (void)name;
        (void)value;
#endif
    }

    void Profiler::message(const std::string &text) {
#ifdef RAGE_PROFILING_TRACY
        TracyMessage(text.c_str(), text.size());
#else
        (void)text;
#endif
    }

    void Profiler::setThreadName(const char *name) {
#ifdef RAGE_PROFILING_TRACY
        ___tracy_set_thread_name(name);
#else
        (void)name;
#endif
    }

    void Profiler::beginZone(const char *name) {
#ifdef RAGE_PROFILING_TRACY
        const size_t nameLen = std::strlen(name);
        auto srcLoc =
            ___tracy_alloc_srcloc_name(0, nullptr, 0, nullptr, 0, name, nameLen, 0xffaaaaee);
        const auto ctx = ___tracy_emit_zone_begin_alloc(srcLoc, 1);
        g_zoneStack.push_back(ctx);
#else
        (void)name;
#endif
    }

    void Profiler::endZone() {
#ifdef RAGE_PROFILING_TRACY
        if (!g_zoneStack.empty()) {
            ___tracy_emit_zone_end(g_zoneStack.back());
            g_zoneStack.pop_back();
        }
#endif
    }

    bool Profiler::isConnected() const {
#ifdef RAGE_PROFILING_TRACY
        return ___tracy_connected() != 0;
#else
        return false;
#endif
    }

    bool Profiler::isLinked() {
#ifdef RAGE_PROFILING_TRACY
        return true;
#else
        return false;
#endif
    }

    void Profiler::launchProfilerGui() {
#if defined(RAGE_PROFILING_TRACY) && defined(_WIN32)
        if (isProfilerGuiRunning()) {
            return;
        }
        const char *tracyExe = RAGE::App::kTracyServerExePath;
        if (tracyExe == nullptr || tracyExe[0] == '\0') {
            std::fprintf(stderr, "[profiler] tracy-profiler.exe path not configured.\n");
            return;
        }
        std::string cmd = "\"";
        cmd += tracyExe;
        cmd += "\" -a 127.0.0.1";

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr,
                            nullptr, &si, &pi)) {
            std::fprintf(stderr, "[profiler] CreateProcess failed for %s (err=%lu)\n", tracyExe, GetLastError());
            return;
        }
        CloseHandle(pi.hThread);
        profilerGuiProcess_ = pi.hProcess;
        std::fprintf(stdout, "[profiler] launched %s (pid %lu)\n", tracyExe, pi.dwProcessId);
        std::fflush(stdout);
#else
        std::fprintf(stderr, "[profiler] launchProfilerGui: not supported on this build/platform.\n");
#endif
    }

    void Profiler::launchGuiAndWait(int timeoutMs) {
        if (!isLinked()) {
            return;
        }
        launchProfilerGui();
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!isConnected() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    bool Profiler::isProfilerGuiRunning() {
#if defined(RAGE_PROFILING_TRACY) && defined(_WIN32)
        if (profilerGuiProcess_ == nullptr) {
            return false;
        }
        const DWORD wait = WaitForSingleObject(static_cast<HANDLE>(profilerGuiProcess_), 0);
        if (wait == WAIT_TIMEOUT) {
            return true;
        }
        CloseHandle(static_cast<HANDLE>(profilerGuiProcess_));
        profilerGuiProcess_ = nullptr;
        return false;
#else
        return false;
#endif
    }
}
