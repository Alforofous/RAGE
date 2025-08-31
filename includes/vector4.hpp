#pragma once
#include <glm/glm.hpp>

class Vector4 {
public:
    Vector4() = default;
    Vector4(float x, float y, float z, float w) : vec(x, y, z, w) {}
    Vector4(float value) : vec(value) {}
    Vector4(const Vector4& other) = default;

    float x() const { return vec.x; }
    float y() const { return vec.y; }
    float z() const { return vec.z; }
    float w() const { return vec.w; }

    void setX(float x) { vec.x = x; }
    void setY(float y) { vec.y = y; }
    void setZ(float z) { vec.z = z; }
    void setW(float w) { vec.w = w; }

    float length() const { return glm::length(vec); }
    void normalize() { vec = glm::normalize(vec); }

    Vector4 operator+(const Vector4& other) const { return vec + other.vec; }
    Vector4 operator-(const Vector4& other) const { return vec - other.vec; }
    Vector4 operator*(float scalar) const { return vec * scalar; }
    Vector4 operator/(float scalar) const { return vec / scalar; }
private:
    Vector4(glm::vec4 vec) : vec(vec) {}
    glm::vec4 vec;
};