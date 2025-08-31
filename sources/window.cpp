#include "../includes/window.hpp"
#include <stdexcept>

Window::Window(int width, int height, std::string title, bool resizable)
    : window(nullptr),
    width(width),
    height(height),
    title(std::move(title)),
    resizable(resizable) {
    // Set window creation hints for Vulkan
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // Vulkan instead of OpenGL
    glfwWindowHint(GLFW_RESIZABLE, this->resizable ? GLFW_TRUE : GLFW_FALSE);
    
    // Additional hints for better Vulkan support
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);  // Hide window until Vulkan is ready
    
    this->window = glfwCreateWindow(this->width, this->height, this->title.c_str(), nullptr, nullptr);
    if (this->window == nullptr) {
        throw std::runtime_error("Failed to create GLFW window");
    }
    
    // Now show the window
    glfwShowWindow(this->window);
}

Window::~Window() {
    if (this->window != nullptr) {
        glfwDestroyWindow(this->window);
        this->window = nullptr;
    }
    glfwTerminate();
}

bool Window::shouldClose() const { return glfwWindowShouldClose(this->window) != 0; }

void Window::pollEvents() { glfwPollEvents(); }

void Window::setResizable(bool resizable) {
    this->resizable = resizable;
    if (this->window != nullptr) {
        glfwSetWindowAttrib(this->window, GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);
    }
}

void Window::setTitle(const std::string &title) {
    this->title = title;
    if (this->window != nullptr) {
        glfwSetWindowTitle(this->window, title.c_str());
    }
}

void Window::setSize(int width, int height) {
    this->width = width;
    this->height = height;
    if (this->window != nullptr) {
        glfwSetWindowSize(this->window, width, height);
    }
}