#include "profiler.hpp"
#include <cstdio>
#include "engine/rendering/renderer.hpp"

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
    #include <tracy/Tracy.hpp>
    #include <tracy/TracyVulkan.hpp>
#endif

namespace RAGE::App {
    Profiler::Profiler() {
#ifdef RAGE_PROFILING_TRACY
        std::fprintf(stdout, "[profiler] Tracy client v0.13.1 linked, on-demand. Connect Tracy.exe to attach.\n");
        std::fflush(stdout);
#else
        std::fprintf(stdout, "[profiler] disabled (configured with -DRAGE_ENABLE_PROFILING=OFF; default is ON).\n");
        std::fflush(stdout);
#endif
    }

    Profiler::~Profiler() {
#ifdef RAGE_PROFILING_TRACY
        if (gpuContext_ != nullptr) {
            TracyVkDestroy(static_cast<TracyVkCtx>(gpuContext_));
            gpuContext_ = nullptr;
        }
#endif
    }

    void Profiler::attach(Renderer &renderer) {
        renderer.onFrameEnd([this] { frameMark(); });

        renderer.onBeforeGpuPass([this](const char *passName, VkCommandBuffer cmd) {
#ifdef RAGE_PROFILING_TRACY
            if (gpuContext_ != nullptr) {
                TracyVkZoneTransient(static_cast<TracyVkCtx>(gpuContext_), _gpuZone, cmd, passName, true);
            }
#else
            (void)passName;
            (void)cmd;
#endif
        });

        renderer.onAfterGpuPass([](const char *passName, VkCommandBuffer cmd) {
            (void)passName;
            (void)cmd;
        });

        renderer.onFrameImage([](const void *rgbaBytes, uint16_t width, uint16_t height) {
#ifdef RAGE_PROFILING_TRACY
            // offset = 0: the image is for the most recently signaled frame (the one whose
            // FrameMark just fired at the end of the previous render() call). The pixels come
            // from that frame's render — we waited for its GPU completion via drainInFlight
            // before reading the staging buffer.
            FrameImage(rgbaBytes, width, height, 0, false);
#else
            (void)rgbaBytes;
            (void)width;
            (void)height;
#endif
        });
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
}
