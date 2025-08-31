#pragma once
#include <glm/glm.hpp>
#include <cstdlib>  // for std::abs

/**
 * A 3D integer vector.
 */
class IntVector3 {
public:
    IntVector3() = default;
    IntVector3(int value) : vector(value) {}
    IntVector3(int x, int y, int z) : vector(x, y, z) {}
    IntVector3(const IntVector3& other) = default;

    void setX(int x) { this->vector.x = x; }
    void setY(int y) { this->vector.y = y; }
    void setZ(int z) { this->vector.z = z; }

    int getX() const { return this->vector.x; }
    int getY() const { return this->vector.y; }
    int getZ() const { return this->vector.z; }

    /** Manhattan distance (sum of absolute values) */
    int manhattanDistance() const {
        return std::abs(this->vector.x) + std::abs(this->vector.y) + std::abs(this->vector.z);
    }

    IntVector3 operator+(const IntVector3& other) const { return this->vector + other.vector; }
    IntVector3 operator-(const IntVector3& other) const { return this->vector - other.vector; }
    IntVector3 operator*(int scalar) const { return this->vector * scalar; }
    IntVector3 operator/(int scalar) const { return this->vector / scalar; }
private:
    IntVector3(glm::ivec3 vec) : vector(vec) {}
    glm::ivec3 vector;
};