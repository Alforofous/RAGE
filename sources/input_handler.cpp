#include "input_handler.hpp"
#include <iostream>

InputHandler::InputHandler(GLFWwindow *window, CameraControls *cameraControls)
    : window(window), cameraControls(cameraControls) {
    // Set up GLFW key callback
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, InputHandler::keyCallback);
}

void InputHandler::update(float deltaTime) {
    float moveDistance = this->moveSpeed * deltaTime;

    // WASD movement (horizontal plane)
    if (this->isKeyPressed(GLFW_KEY_W)) {
        this->cameraControls->moveForward(moveDistance);
    }
    if (this->isKeyPressed(GLFW_KEY_S)) {
        this->cameraControls->moveBackward(moveDistance);
    }
    if (this->isKeyPressed(GLFW_KEY_A)) {
        this->cameraControls->moveLeft(moveDistance);
    }
    if (this->isKeyPressed(GLFW_KEY_D)) {
        this->cameraControls->moveRight(moveDistance);
    }

    // EQ movement (vertical)
    if (this->isKeyPressed(GLFW_KEY_Q)) {
        this->cameraControls->moveUp(moveDistance);
    }
    if (this->isKeyPressed(GLFW_KEY_E)) {
        this->cameraControls->moveDown(moveDistance);
    }
}

void InputHandler::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)scancode; // Unused parameter
    (void)mods;     // Unused parameter

    // Handle ESC to close window
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // Print key presses for debugging
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_W: std::cout << "W pressed - Move Forward" << std::endl; break;
        case GLFW_KEY_S: std::cout << "S pressed - Move Backward" << std::endl; break;
        case GLFW_KEY_A: std::cout << "A pressed - Move Left" << std::endl; break;
        case GLFW_KEY_D: std::cout << "D pressed - Move Right" << std::endl; break;
        case GLFW_KEY_Q: std::cout << "Q pressed - Move Up" << std::endl; break;
        case GLFW_KEY_E: std::cout << "E pressed - Move Down" << std::endl; break;
        }
    }
}

bool InputHandler::isKeyPressed(int key) {
    return glfwGetKey(this->window, key) == GLFW_PRESS;
}