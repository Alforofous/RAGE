#include "renderable_node3D.hpp"

RenderableNode3D::RenderableNode3D(Material *material) : material(material) {}

RenderableNode3D::RenderableNode3D(const RenderableNode3D &other) = default;

RenderableNode3D::~RenderableNode3D() = default;

void RenderableNode3D::render(Renderer &renderer) const {
    // Material properties are now handled via push constants in the renderer
    // This method is called during scene traversal in recordCommandBuffer
    // No need to call renderFrame again as we're already in the render process
}