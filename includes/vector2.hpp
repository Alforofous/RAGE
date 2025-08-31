#pragma once
#include <glm/glm.hpp>

class Vector2 {
public:
    Vector2() = default;
    Vector2(float x, float y) : vec(x, y) {}
    Vector2(const Vector2& other) = default;

    float x() const { return vec.x; }
    float y() const { return vec.y; }

    void setX(float x) { vec.x = x; }
    void setY(float y) { vec.y = y; }

    float length() const { return glm::length(vec); }
    void normalize() { vec = glm::normalize(vec); }

    Vector2 operator+(const Vector2& other) const { return vec + other.vec; }
    Vector2 operator-(const Vector2& other) const { return vec - other.vec; }
    Vector2 operator*(float scalar) const { return vec * scalar; }
    Vector2 operator/(float scalar) const { return vec / scalar; }
private:
    Vector2(glm::vec2 vec) : vec(vec) {}
    glm::vec2 vec;
};