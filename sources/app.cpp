#include "app.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include "voxel3D.hpp"
#include <iostream>

#define HEIGHT 720
#define WIDTH 1280

App::App() {
    std::cout << "Initializing GLFW..." << std::endl;
    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    std::cout << "Creating window..." << std::endl;
    this->window = std::make_unique<Window>(WIDTH, HEIGHT, "RAGE Engine");
    if (!this->window->getWindow()) {
        throw std::runtime_error("Window creation failed - null window handle");
    }

    std::cout << "Creating Vulkan surface..." << std::endl;
    this->context = createVulkanGLFWSurface(this->window->getWindow());
    std::cout << "Vulkan surface created successfully" << std::endl;

    std::cout << "Creating scene..." << std::endl;
    this->scene = std::make_unique<Scene>();
    auto voxel = std::make_unique<Voxel3D>();
    voxel->setPosition(IntVector3(0, 0, -5));  // Put voxel 5 units in front of camera
    voxel->setSize(IntVector3(2));             // Make it bigger (2x2x2) so it's easier to see
    voxel->setColor(IntVector3(0, 0, 255));    // Blue color
    this->scene->addChild(voxel.get());
    this->voxel = std::move(voxel);            // Keep voxel alive

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
    // Wait for all operations to complete before destroying anything
    if (this->context.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(this->context.device);
    }

    // First destroy the renderer as it uses the Vulkan context
    if (this->renderer) {
        this->renderer.reset();
    }

    // Then clean up Vulkan resources
    if (this->context.device != VK_NULL_HANDLE) {
        // First destroy swapchain resources
        for (auto imageView : this->context.swapchainImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(this->context.device, imageView, nullptr);
            }
        }
        if (this->context.swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(this->context.device, this->context.swapchain, nullptr);
            this->context.swapchain = VK_NULL_HANDLE;
        }

        // Then destroy the device
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

    // Clean up other resources
    this->scene.reset();
    this->camera.reset();
    this->voxel.reset();

    // Window must be destroyed last as it contains the GLFW window
    this->window.reset();
}

void App::run() {
    std::cout << "Starting main loop..." << std::endl;
    std::cout << "Controls: WASD to move, E/Q for up/down, ESC to exit" << std::endl;

    this->window->show(); // Make sure window is visible

    // Timing for smooth movement
    double lastTime = glfwGetTime();

    while (!this->window->shouldClose()) {
        // Calculate delta time
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        // Poll events
        Window::pollEvents();

        // Update input and camera
        this->inputHandler->update(deltaTime);

        // Render frame
        this->renderer->renderFrame();
    }
    std::cout << "Main loop ended" << std::endl;
}