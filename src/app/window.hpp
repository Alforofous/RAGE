#pragma once

#include <cstdint>
#include <string>
#include <utility>

struct GLFWwindow;

namespace RAGE::App {
    /**
     * A platform-native window backed by GLFW, used by the test harness application.
     *
     * This class is part of the app layer, not the engine. The engine never depends on a window
     * type: callers wishing to render through RAGE construct a VkSurfaceKHR via their own
     * windowing system and pass the handle into the engine (e.g. into a Swapchain).
     *
     * Window owns the underlying GLFWwindow, tracks resize events, and exposes the raw
     * GLFWwindow* so callers can build a VkSurfaceKHR with glfwCreateWindowSurface. Move-only.
     *
     * @note GLFW is initialised lazily on the first Window construction and terminated when the
     *       last Window is destroyed. Construct windows only from the main thread.
     */
    class Window {
    public:
        Window() = delete;
        Window(uint32_t width, uint32_t height, const std::string &title);
        ~Window();

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;
        Window(Window &&other) noexcept;
        Window &operator=(Window &&other) noexcept;

        void pollEvents();
        bool shouldClose() const;
        void setTitle(const std::string &title);

        std::pair<uint32_t, uint32_t> framebufferExtent() const;

        bool wasResized() const { return resized_; }
        void clearResized() { resized_ = false; }

        GLFWwindow *glfwHandle() const { return window_; }

    private:
        void destroy();

        static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

        GLFWwindow *window_ = nullptr;
        bool resized_ = false;
    };
}
