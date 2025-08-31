#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <vulkan/vulkan.h>

class Window {
public:
    Window(int width, int height, std::string title, bool resizable = true);
    ~Window();

    bool shouldClose() const;
    static void pollEvents();
    GLFWwindow *getWindow() const { return this->window; }

    int getWidth() const { return this->width; }
    int getHeight() const { return this->height; }
    std::string getTitle() const { return this->title; }

    void setResizable(bool resizable);
    void setTitle(const std::string &title);
    void setSize(int width, int height);
    void show() { glfwShowWindow(window); }
private:
    GLFWwindow *window;
    int width;
    int height;
    std::string title;
    bool resizable;
};