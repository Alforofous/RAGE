#pragma once
#include <memory>
#include "window.hpp"
#include "renderer.hpp"
#include "vulkan_utils.hpp"
#include "voxel3D.hpp"
#include "camera_controls.hpp"
#include "input_handler.hpp"

class App {
public:
    App();
    ~App();

    void run();

    // Public access to camera controls for easy manipulation
    CameraControls *getCameraControls() { return this->cameraControls.get(); }

private:
    std::unique_ptr<Window> window;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Scene> scene;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<CameraControls> cameraControls;
    std::unique_ptr<InputHandler> inputHandler;
    std::unique_ptr<Voxel3D> voxel;  // Keep voxel alive with the app
    VulkanContext context;
};