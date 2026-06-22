#pragma once

#include "engine/scene/node3d.hpp"
#include "math/mat.hpp"

namespace RAGE {
    /**
     * A perspective camera attached to the scene graph.
     *
     * Camera inherits from Node3D so it is transform-driven the same way as any other node: place
     * it with setPosition / setRotation, parent it to another node to follow that node, etc. The
     * view matrix is the inverse of the camera's world transform; the projection matrix is
     * constructed from (fov, aspect, near, far) using a Vulkan-aware perspective (the Y axis is
     * flipped internally so the engine renders right-side-up without callers caring).
     *
     * Projection parameters change rarely (typically only on resize), so the projection matrix is
     * cached with a dirty flag. The view matrix is recomputed on every call from the world
     * transform — cheap enough at one camera per frame; cache later if profiling demands it.
     *
     * @note The fov is in radians. A camera with identity rotation looks down -Z, matching the
     *       Vulkan / right-handed -Z-forward convention used by Mat4::lookAt.
     */
    class Camera : public Node3D {
    public:
        Camera(float fovRadians, float aspect, float zNear, float zFar);

        void setProjection(float fovRadians, float aspect, float zNear, float zFar);
        void setFov(float fovRadians);
        void setAspect(float aspect);
        void setZNear(float zNear);
        void setZFar(float zFar);

        float fov() const { return fovRadians_; }
        float aspect() const { return aspect_; }
        float zNear() const { return zNear_; }
        float zFar() const { return zFar_; }

        const Mat4 &projectionMatrix() const;
        Mat4 viewMatrix() const;
        Mat4 viewProjectionMatrix() const;

    private:
        void markProjectionDirty() noexcept { projectionDirty_ = true; }

        float fovRadians_ = 0.0f;
        float aspect_ = 1.0f;
        float zNear_ = 0.1f;
        float zFar_ = 100.0f;

        mutable Mat4 projectionMatrix_ = Mat4::identity();
        mutable bool projectionDirty_ = true;
    };
}
