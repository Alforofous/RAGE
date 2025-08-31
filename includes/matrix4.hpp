#pragma once
#include <glm/glm.hpp>
#include "vector3.hpp"

/**
 * A 4x4 matrix.
 */
class Matrix4 {
public:
    Matrix4() = default;
    Matrix4(float value) : matrix(value) {}
    Matrix4(float v1, float v2, float v3, float v4,
            float v5, float v6, float v7, float v8,
            float v9, float v10, float v11, float v12,
            float v13, float v14, float v15, float v16);

    Matrix4 operator+(const Matrix4& other) const;
    Matrix4 operator-(const Matrix4& other) const;
    Matrix4 operator*(const Matrix4& other) const;
    Matrix4 operator*(float scalar) const;
    Matrix4 operator/(float scalar) const;

    void setIdentity();
    void setPerspective(float fov, float aspectRatio, float near, float far);
    void setOrthographic(
        float left,
        float right,
        float bottom,
        float top,
        float near,
        float far
        );
    void setTranslation(const Vector3& translation);
    void setRotation(const Vector3& rotation);
    void setScale(const Vector3& scale);
    void setProjection(const Vector3& projection);
private:
    Matrix4(glm::mat4 matrix) : matrix(matrix) {}
    glm::mat4 matrix = glm::mat4(1.0f);
};