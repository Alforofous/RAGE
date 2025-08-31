#pragma once
#include <GLFW/glfw3.h>
#include "camera_controls.hpp"

/**
 * Simple input handler for camera movement
 */
class InputHandler {
public:
    InputHandler(GLFWwindow *window, CameraControls *cameraControls);
    ~InputHandler() = default;

    // Update camera based on current input state
    void update(float deltaTime);

    // GLFW key callback
    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

private:
    GLFWwindow *window;
    CameraControls *cameraControls;

    // Movement speed
    float moveSpeed = 5.0f; // units per second

    // Check if key is currently pressed
    bool isKeyPressed(int key);
};