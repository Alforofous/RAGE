#pragma once

#include <functional>
#include <memory>
#include <span>

struct GLFWwindow;

namespace RAGE {
    class Renderer;
    class VulkanContext;
}

namespace RAGE::App {
    /**
     * DebugUi is the single bridge between the engine and Dear ImGui.
     *
     * Design mirrors `App::Profiler`: this is the **only** translation unit that includes
     * imgui.h. The engine is ImGui-agnostic; it exposes two observer callbacks
     * (`onSwapchainRebuilt`, `onUiRender`) that this wrapper subscribes to from main.cpp.
     * Swap the library — replace this file's implementation; the public header stays.
     *
     * Phase A scope
     * =============
     *   - Initialise ImGui (context, fonts) and the GLFW + Vulkan backends.
     *   - Build a `VkRenderPass` + per-swapchain-image `VkFramebuffer` lazily on the first
     *     swapchain-rebuilt callback, and again on every subsequent rebuild (resize).
     *   - During the per-frame UI-render callback, run NewFrame -> builder() -> Render and
     *     submit the resulting ImGui draws into the engine's command buffer inside the UI
     *     render pass.
     *   - Provide a single immediate-mode helper for the demo panel.
     *
     * Phase B (planned)
     * =================
     *   - Widget surface: beginPanel / endPanel / checkbox / radio / text(fmt, ...).
     *   - `wantsMouse() / wantsKeyboard()` accessors so input controllers can yield.
     *
     * Wiring
     * ======
     *     App::DebugUi ui(ctx, window);
     *     ui.attach(renderer);
     *     ui.setBuilder([&] {
     *         // app code uses ui.beginPanel / ui.checkbox / ui.text here in Phase B
     *     });
     */
    class DebugUi {
    public:
        DebugUi(VulkanContext &ctx, GLFWwindow *window);
        ~DebugUi();

        DebugUi(const DebugUi &) = delete;
        DebugUi &operator=(const DebugUi &) = delete;
        DebugUi(DebugUi &&) = delete;
        DebugUi &operator=(DebugUi &&) = delete;

        // Subscribe the UI to a renderer's swapchain-rebuilt and per-frame UI-render hooks.
        // Call exactly once per renderer.
        void attach(Renderer &renderer);

        // Builder lambda runs inside the engine's per-frame UI-render callback, between
        // NewFrame() and Render(). The lambda uses the wrapper's widget API (Phase B); for
        // Phase A it is invoked but typically only contains a stub "Hello" panel.
        using BuilderFn = std::function<void()>;
        void setBuilder(BuilderFn fn);

        // Phase A helper: draw a minimal sanity panel ("RAGE Debug UI") so the integration
        // can be smoke-tested before any wrapper widgets exist. Calls inside a builder fn.
        void demoPanel(const char *title);

        // Immediate-mode widget surface. Each pair of beginPanel/endPanel brackets a draggable
        // window; widgets in between render inside it. Returns from interactive widgets are
        // `true` iff the value changed this frame. These primitives are deliberately tiny —
        // expand as new debug needs arise rather than mirroring all of ImGui.
        void beginPanel(const char *title);
        void endPanel();
        bool button(const char *label);
        bool checkbox(const char *label, bool *value);
        bool radio(const char *label, int *current, std::span<const char *const> options);
        bool sliderInt(const char *label, int *value, int min, int max);
        bool sliderFloat(const char *label, float *value, float min, float max);
        /**
         * @brief Collapsible section header; returns true while expanded. For labels that
         *        change per frame (live values), append a "###id" suffix so the widget ID
         *        stays stable across relabels.
         */
        bool collapsingHeader(const char *label, bool defaultOpen = false);
        void separatorText(const char *label);
        void separator();
        void text(const char *fmt, ...);

        /**
         * @brief Time-series bar plot. `(samples, bufferLength, count, offset)` is the
         * shape `Core::Histogram` exposes via `data()` / `capacity()` / `size()` /
         * `oldestOffset()`. `overlayFmt` is a printf format applied to the latest
         * sample for the value tagline; null to omit. `(yMin, yMax) = (0, FLT_MAX)`
         * auto-scales.
         */
        void plot(const char *label, const float *samples, size_t bufferLength, size_t count,
                  size_t offset, const char *overlayFmt, float yMin, float yMax);

        /**
         * Mouse-wheel delta this frame, in ImGui's units (+1 per tick up, -1 down). Always
         * the raw wheel value — callers gate based on their own state (e.g. mouse-look
         * capture). ImGui consumes wheel events for its own widgets independently of this
         * accessor, so reading it here does not steal from UI scrolling.
         */
        float scrollDelta() const;

        // True when ImGui has the mouse / keyboard focus this frame (cursor over a panel,
        // text-input widget active). Input controllers consult these to yield gracefully so
        // panel clicks don't also grab the camera, etc. Engine code never calls these — the
        // engine has no notion of UI focus.
        bool wantsMouse() const;
        bool wantsKeyboard() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}
