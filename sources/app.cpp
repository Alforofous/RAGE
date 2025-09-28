#include "app.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include "voxel3D.hpp"
#include <iostream>

#define HEIGHT 720
#define WIDTH 1280

App::App() {
    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    this->window = std::make_unique<Window>(WIDTH, HEIGHT, "RAGE Engine");
    if (this->window->getWindow() == nullptr) {
        throw std::runtime_error("Window creation failed - null window handle");
    }

    this->context = createVulkanGLFWSurface(this->window->getWindow());

    this->scene = Scene::create();
    auto voxel1 = Voxel3D::create();
    auto voxel2 = Voxel3D::create();
    voxel1->setPosition(Vector3(2, 0, -5));  // Put voxel 5 units in front of camera
    voxel1->setScale(Vector3(0.5));             // Make it bigger (2x2x2) so it's easier to see
    voxel1->setColor(Vector3(1.0, 0.0, 0.0));    // Red color
    this->scene->addChild(voxel1);
    voxel2->setPosition(Vector3(-2, 0, -5));
    voxel2->setScale(Vector3(0.5));
    voxel2->setColor(Vector3(0.0, 1.0, 0.0));
    this->scene->addChild(voxel2);

    this->camera = std::make_unique<PerspectiveCamera>();
    this->camera->setPosition(Vector3(0.0f, 0.0f, 0.0f));   // Camera at origin
    this->camera->setRotation(Vector3(0.0f, 0.0f, 0.0f));   // Looking straight forward

    // Create camera controls
    this->cameraControls = std::make_unique<CameraControls>(this->camera.get());

    // Create input handler
    this->inputHandler = std::make_unique<InputHandler>(this->window->getWindow(), this->cameraControls.get());

    this->renderer = std::make_unique<Renderer>(&this->context, this->scene.get(), this->camera.get());
}

App::~App() {
    if (this->context.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(this->context.device);
    }
    if (this->renderer) {
        this->renderer.reset();
    }

    if (this->context.device != VK_NULL_HANDLE) {
        for (auto *imageView : this->context.swapchainImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(this->context.device, imageView, nullptr);
            }
        }
        if (this->context.swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(this->context.device, this->context.swapchain, nullptr);
            this->context.swapchain = VK_NULL_HANDLE;
        }
        vkDestroyDevice(this->context.device, nullptr);
        this->context.device = VK_NULL_HANDLE;
    }

    if (this->context.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(this->context.instance, this->context.surface, nullptr);
        this->context.surface = VK_NULL_HANDLE;
    }

    if (this->context.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(this->context.instance, nullptr);
        this->context.instance = VK_NULL_HANDLE;
    }
    this->scene.reset();
    this->camera.reset();
    this->window.reset();
}

void App::run() {
    this->window->show();
    double lastTime = glfwGetTime();
    while (!this->window->shouldClose()) {
        double currentTime = glfwGetTime();
        auto deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        Window::pollEvents();
        this->inputHandler->update(deltaTime);
        this->renderer->renderFrame();
    }
    std::cout << "Main loop ended" << std::endl;
}