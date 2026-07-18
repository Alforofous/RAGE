#include "window.hpp"
#include <stdexcept>
#include <utility>
#include <GLFW/glfw3.h>

namespace {
    uint32_t g_windowCount = 0;

    void ensureGlfwInitialized() {
        if (g_windowCount == 0) {
            if (glfwInit() != GLFW_TRUE) {
                throw std::runtime_error("Failed to initialise GLFW");
            }
        }
        g_windowCount++;
    }

    void releaseGlfw() {
        if (g_windowCount == 0) {
            return;
        }
        g_windowCount--;
        if (g_windowCount == 0) {
            glfwTerminate();
        }
    }
}

namespace RAGE::App {
    Window::Window(uint32_t width, uint32_t height, const std::string &title) {
        ensureGlfwInitialized();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
        if (window_ == nullptr) {
            releaseGlfw();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, &Window::framebufferResizeCallback);
    }

    Window::~Window() {
        destroy();
    }

    Window::Window(Window &&other) noexcept
        : window_(std::exchange(other.window_, nullptr))
        , resized_(other.resized_) {
        if (window_ != nullptr) {
            glfwSetWindowUserPointer(window_, this);
        }
    }

    Window &Window::operator=(Window &&other) noexcept {
        if (this != &other) {
            destroy();
            window_ = std::exchange(other.window_, nullptr);
            resized_ = other.resized_;
            if (window_ != nullptr) {
                glfwSetWindowUserPointer(window_, this);
            }
        }

        return *this;
    }

    void Window::destroy() {
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
            releaseGlfw();
        }
    }

    void Window::pollEvents() {
        glfwPollEvents();
    }

    bool Window::shouldClose() const {
        return glfwWindowShouldClose(window_) != 0;
    }

    void Window::setTitle(const std::string &title) {
        if (window_ != nullptr) {
            glfwSetWindowTitle(window_, title.c_str());
        }
    }

    std::pair<uint32_t, uint32_t> Window::framebufferExtent() const {
        int w = 0;
        int h = 0;
        glfwGetFramebufferSize(window_, &w, &h);

        return { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    }

    Toolkit::WindowSurfaceSource Window::vulkanSurfaceSource() const {
        GLFWwindow *handle = window_;
        uint32_t extCount = 0;
        const char **exts = glfwGetRequiredInstanceExtensions(&extCount);

        Toolkit::WindowSurfaceSource source;
        source.instanceExtensions.assign(exts, exts + extCount);
        source.createSurface = [handle](VkInstance instance) {
            VkSurfaceKHR surface = VK_NULL_HANDLE;
            if (glfwCreateWindowSurface(instance, handle, nullptr, &surface) != VK_SUCCESS) {
                throw std::runtime_error("glfwCreateWindowSurface failed");
            }
            return surface;
        };
        source.framebufferExtent = [handle]() {
            int w = 0;
            int h = 0;
            glfwGetFramebufferSize(handle, &w, &h);
            return std::pair{ static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
        };
        return source;
    }

    void Window::framebufferResizeCallback(GLFWwindow *window, int /*width*/, int /*height*/) {
        auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window));
        if (self != nullptr) {
            self->resized_ = true;
        }
    }
}
