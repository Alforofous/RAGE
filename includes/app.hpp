#pragma once
#include <memory>
#include "window.hpp"
#include "renderer.hpp"
#include "vulkan_utils.hpp"
#include "voxel3D.hpp"

class App {
public:
    App();
    ~App();

    void run();

private:
    std::unique_ptr<Window> window;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Scene> scene;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<Voxel3D> voxel;  // Keep voxel alive with the app
    VulkanContext context;
};