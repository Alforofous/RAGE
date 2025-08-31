#pragma once
#include "node3D.hpp"
#include <glm/glm.hpp>

#define DEFAULT_FOV (45.0f)
#define DEFAULT_ASPECT (16.0f / 9.0f)
#define DEFAULT_NEAR (0.1f)
#define DEFAULT_FAR (1000.0f)

class Camera : public Node3D {
public:
    Camera();
    virtual ~Camera() = default;

    void setView(const Matrix4& view);
    const Matrix4& getProjection() const { return projection; }
    const Matrix4& getView() const { return view; }

    virtual void updateProjection() = 0;

protected:
    Matrix4 projection;
    Matrix4 view;
};

class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(float fov = DEFAULT_FOV, float aspect = DEFAULT_ASPECT,
                      float near = DEFAULT_NEAR, float far = DEFAULT_FAR);

    void setFov(float fov);
    void setAspect(float aspect);
    void setNearPlane(float near);
    void setFarPlane(float far);

    float getFov() const { return fov; }
    float getAspect() const { return aspect; }
    float getNearPlane() const { return nearPlane; }
    float getFarPlane() const { return farPlane; }

    void updateProjection() override;

private:
    float fov;
    float aspect;
    float nearPlane;
    float farPlane;
};

#define DEFAULT_LEFT (-10.0f)
#define DEFAULT_RIGHT (10.0f)
#define DEFAULT_BOTTOM (-10.0f)
#define DEFAULT_TOP (10.0f)

class OrthographicCamera : public Camera {
public:
    OrthographicCamera(float left = DEFAULT_LEFT, float right = DEFAULT_RIGHT,
                       float bottom = DEFAULT_BOTTOM, float top = DEFAULT_TOP,
                       float near = DEFAULT_NEAR, float far = DEFAULT_FAR);

    void setLeft(float left);
    void setRight(float right);
    void setBottom(float bottom);
    void setTop(float top);
    void setNearPlane(float near);
    void setFarPlane(float far);

    float getLeft() const { return left; }
    float getRight() const { return right; }
    float getBottom() const { return bottom; }
    float getTop() const { return top; }
    float getNearPlane() const { return nearPlane; }
    float getFarPlane() const { return farPlane; }

    void updateProjection() override;

private:
    float left, right;
    float bottom, top;
    float nearPlane, farPlane;
};