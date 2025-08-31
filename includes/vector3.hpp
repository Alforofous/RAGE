#pragma once
#include <glm/glm.hpp>

/**
 * A 3D vector.
 */
class Vector3 {
public:
    Vector3() = default;
    Vector3(float value) : vector(value) {}
    Vector3(float x, float y, float z) : vector(x, y, z) {}
    Vector3(const Vector3& other) = default;

    void setX(float x) { this->vector.x = x; }
    void setY(float y) { this->vector.y = y; }
    void setZ(float z) { this->vector.z = z; }

    float getX() const { return this->vector.x; }
    float getY() const { return this->vector.y; }
    float getZ() const { return this->vector.z; }

    float length() const { return glm::length(this->vector); }
    void normalize() { this->vector = glm::normalize(this->vector); }

    Vector3 operator+(const Vector3& other) const { return this->vector + other.vector; }
    Vector3 operator-(const Vector3& other) const { return this->vector - other.vector; }
    Vector3 operator*(float scalar) const { return this->vector * scalar; }
    Vector3 operator/(float scalar) const { return this->vector / scalar; }
private:
    Vector3(glm::vec3 vec) : vector(vec) {}
    glm::vec3 vector;
};