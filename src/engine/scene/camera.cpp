#include "camera.hpp"

namespace RAGE {
    Camera::Camera(float fovRadians, float aspect, float zNear, float zFar)
        : fovRadians_(fovRadians)
        , aspect_(aspect)
        , zNear_(zNear)
        , zFar_(zFar) {}

    void Camera::setProjection(float fovRadians, float aspect, float zNear, float zFar) {
        fovRadians_ = fovRadians;
        aspect_ = aspect;
        zNear_ = zNear;
        zFar_ = zFar;
        markProjectionDirty();
    }

    void Camera::setFov(float fovRadians) {
        fovRadians_ = fovRadians;
        markProjectionDirty();
    }

    void Camera::setAspect(float aspect) {
        aspect_ = aspect;
        markProjectionDirty();
    }

    void Camera::setZNear(float zNear) {
        zNear_ = zNear;
        markProjectionDirty();
    }

    void Camera::setZFar(float zFar) {
        zFar_ = zFar;
        markProjectionDirty();
    }

    const Mat4 &Camera::projectionMatrix() const {
        if (projectionDirty_) {
            projectionMatrix_ = Mat4::perspective(fovRadians_, aspect_, zNear_, zFar_);
            projectionDirty_ = false;
        }

        return projectionMatrix_;
    }

    Mat4 Camera::viewMatrix() const {
        return worldMatrix().inverted();
    }

    Mat4 Camera::viewProjectionMatrix() const {
        return projectionMatrix() * viewMatrix();
    }
}
