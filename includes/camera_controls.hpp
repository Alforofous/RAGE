#pragma once
#include "camera.hpp"
#include "vector3.hpp"

/**
 * Simple camera controls for moving the camera around
 */
class CameraControls {
public:
    CameraControls(Camera *camera);
    ~CameraControls() = default;

    // Simple movement controls
    void moveForward(float distance);
    void moveBackward(float distance);
    void moveLeft(float distance);
    void moveRight(float distance);
    void moveUp(float distance);
    void moveDown(float distance);

    // Direct position control
    void setPosition(const Vector3 &position);
    void translate(const Vector3 &offset);

    // Look at target
    void lookAt(const Vector3 &target);

    // Get current position
    Vector3 getPosition() const;

private:
    Camera *camera;

    void updateCameraView();
};