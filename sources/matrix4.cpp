#include "matrix4.hpp"
#include <glm/gtc/matrix_transform.hpp>

Matrix4::Matrix4(
    float v1, float v2, float v3, float v4,
    float v5, float v6, float v7, float v8,
    float v9, float v10, float v11, float v12,
    float v13, float v14, float v15, float v16)
    : matrix(
        v1, v2, v3, v4,
        v5, v6, v7, v8,
        v9, v10, v11, v12,
        v13, v14, v15, v16)
{}

Matrix4 Matrix4::operator+(const Matrix4& other) const {
    return { this->matrix + other.matrix };
}

void Matrix4::setIdentity() {
    this->matrix = glm::mat4(1.0f);
}

void Matrix4::setPerspective(float fov, float aspectRatio, float near, float far) {
    this->matrix = glm::perspective(fov, aspectRatio, near, far);
}

void Matrix4::setOrthographic(float left, float right, float bottom, float top, float near, float far) {
    this->matrix = glm::ortho(left, right, bottom, top, near, far);
}

void Matrix4::setTranslation(const Vector3& translation) {
    glm::vec3 vec(translation.getX(), translation.getY(), translation.getZ());
    this->matrix = glm::translate(this->matrix, vec);
}

void Matrix4::setRotation(const Vector3& rotation) {
    glm::vec3 xAxis(1.0f, 0.0f, 0.0f);
    glm::vec3 yAxis(0.0f, 1.0f, 0.0f);
    glm::vec3 zAxis(0.0f, 0.0f, 1.0f);
    this->matrix = glm::rotate(this->matrix, rotation.getX(), xAxis);
    this->matrix = glm::rotate(this->matrix, rotation.getY(), yAxis);
    this->matrix = glm::rotate(this->matrix, rotation.getZ(), zAxis);
}

void Matrix4::setScale(const Vector3& scale) {
    glm::vec3 vec(scale.getX(), scale.getY(), scale.getZ());
    this->matrix = glm::scale(this->matrix, vec);
}