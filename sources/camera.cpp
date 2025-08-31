#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera() {
    view = Matrix4(1.0f);
    projection = Matrix4(1.0f);
}

void Camera::setView(const Matrix4& view) {
    this->view = view;
}

PerspectiveCamera::PerspectiveCamera(float fov, float aspect, float near, float far)
    : fov(fov), aspect(aspect), nearPlane(near), farPlane(far) {
    updateProjection();
}

void PerspectiveCamera::setFov(float fov) {
    this->fov = fov;
    updateProjection();
}

void PerspectiveCamera::setAspect(float aspect) {
    this->aspect = aspect;
    updateProjection();
}

void PerspectiveCamera::setNearPlane(float near) {
    this->nearPlane = near;
    updateProjection();
}

void PerspectiveCamera::setFarPlane(float far) {
    this->farPlane = far;
    updateProjection();
}

void PerspectiveCamera::updateProjection() {
    projection.setPerspective(
        this->fov,
        this->aspect,
        this->nearPlane,
        this->farPlane
        );
}

OrthographicCamera::OrthographicCamera(float left, float right, float bottom, float top, float near, float far)
    : left(left), right(right), bottom(bottom), top(top), nearPlane(near), farPlane(far) {
    updateProjection();
}

void OrthographicCamera::setLeft(float left) {
    this->left = left;
    updateProjection();
}

void OrthographicCamera::setRight(float right) {
    this->right = right;
    updateProjection();
}

void OrthographicCamera::setBottom(float bottom) {
    this->bottom = bottom;
    updateProjection();
}

void OrthographicCamera::setTop(float top) {
    this->top = top;
    updateProjection();
}

void OrthographicCamera::setNearPlane(float near) {
    this->nearPlane = near;
    updateProjection();
}

void OrthographicCamera::setFarPlane(float far) {
    this->farPlane = far;
    updateProjection();
}

void OrthographicCamera::updateProjection() {
    projection.setOrthographic(
        this->left,
        this->right,
        this->bottom,
        this->top,
        this->nearPlane,
        this->farPlane
        );
}