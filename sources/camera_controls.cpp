#include "camera_controls.hpp"
#include "matrix4.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CameraControls::CameraControls(Camera *camera) : camera(camera) {
}

void CameraControls::moveForward(float distance) {
    Vector3 currentPos = this->camera->getPosition();
    // Simple forward movement along -Z axis (standard camera forward)
    this->camera->setPosition(Vector3(currentPos.getX(), currentPos.getY(), currentPos.getZ() - distance));
    this->updateCameraView();
}

void CameraControls::moveBackward(float distance) {
    Vector3 currentPos = this->camera->getPosition();
    this->camera->setPosition(Vector3(currentPos.getX(), currentPos.getY(), currentPos.getZ() + distance));
    this->updateCameraView();
}

void CameraControls::moveLeft(float distance) {
    Vector3 currentPos = this->camera->getPosition();
    this->camera->setPosition(Vector3(currentPos.getX() - distance, currentPos.getY(), currentPos.getZ()));
    this->updateCameraView();
}

void CameraControls::moveRight(float distance) {
    Vector3 currentPos = this->camera->getPosition();
    this->camera->setPosition(Vector3(currentPos.getX() + distance, currentPos.getY(), currentPos.getZ()));
    this->updateCameraView();
}

void CameraControls::moveUp(float distance) {
    Vector3 currentPos = this->camera->getPosition();
    this->camera->setPosition(Vector3(currentPos.getX(), currentPos.getY() + distance, currentPos.getZ()));
    this->updateCameraView();
}

void CameraControls::moveDown(float distance) {
    Vector3 currentPos = this->camera->getPosition();
    this->camera->setPosition(Vector3(currentPos.getX(), currentPos.getY() - distance, currentPos.getZ()));
    this->updateCameraView();
}

void CameraControls::setPosition(const Vector3 &position) {
    this->camera->setPosition(position);
    this->updateCameraView();
}

void CameraControls::translate(const Vector3 &offset) {
    Vector3 currentPos = this->camera->getPosition();
    Vector3 newPos = Vector3(
        currentPos.getX() + offset.getX(),
        currentPos.getY() + offset.getY(),
        currentPos.getZ() + offset.getZ()
    );
    this->camera->setPosition(newPos);
    this->updateCameraView();
}

void CameraControls::lookAt(const Vector3 &target) {
    Vector3 cameraPos = this->camera->getPosition();

    // Calculate direction vector from camera to target
    Vector3 direction = Vector3(
        target.getX() - cameraPos.getX(),
        target.getY() - cameraPos.getY(),
        target.getZ() - cameraPos.getZ()
    );

    // Calculate rotation angles (simplified - just basic pitch and yaw)
    float distance = std::sqrt(direction.getX() * direction.getX() +
                               direction.getY() * direction.getY() +
                               direction.getZ() * direction.getZ());

    if (distance > 0.001f) {
        // Calculate pitch (rotation around X-axis)
        float pitch = std::asin(-direction.getY() / distance) * 180.0f / M_PI;

        // Calculate yaw (rotation around Y-axis)
        float yaw = std::atan2(direction.getX(), -direction.getZ()) * 180.0f / M_PI;

        this->camera->setRotation(Vector3(pitch, yaw, 0.0f));
        this->updateCameraView();
    }
}

Vector3 CameraControls::getPosition() const {
    return this->camera->getPosition();
}

void CameraControls::updateCameraView() {
    // For now, just use a simple identity matrix
    // The camera position is already set, and the shader will handle the ray generation
    Matrix4 viewMatrix;
    viewMatrix.setIdentity();

    this->camera->setView(viewMatrix);
}