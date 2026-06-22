#pragma once

#include <memory>
#include <utility>
#include <vulkan/vulkan.h>
#include "engine/materials/material.hpp"
#include "engine/rendering/frame_context.hpp"
#include "engine/scene/node3d.hpp"
#include "gpu/gpu_shader_module.hpp"

namespace RAGE {
    class VulkanDescriptorWriter;

    /**
     * A scene-graph node the renderer should draw.
     *
     * Inherits Node3D (transform + hierarchy) and adds a shared `Material<TShader>` plus a
     * visibility flag. Templated on the shader-module concept so the layer stays backend-agnostic
     * up to the `prepareFrame` hook (which is necessarily backend-coupled — it speaks the
     * renderer's binding language).
     *
     * Subclasses (Voxel3D, etc.) override prepareFrame to contribute their per-frame, per-instance
     * state to a dispatch: extra descriptor bindings beyond the implicit render target, and a
     * push-constant struct built from frame context + their own transform.
     *
     * Multiple renderables can share the same Material — the shader and resolved pipeline are
     * shared via PipelineCache, but each renderable's prepareFrame runs independently with its
     * own descriptor set and push-constant buffer.
     *
     * @param TShader Shader-module type satisfying the GpuShaderModule concept.
     */
    template <GpuShaderModule TShader> class RenderableNode3D : public Node3D {
    public:
        RenderableNode3D() = default;
        explicit RenderableNode3D(std::shared_ptr<Material<TShader>> material)
            : material_(std::move(material)) {}

        const std::shared_ptr<Material<TShader>> &material() const { return material_; }
        void setMaterial(std::shared_ptr<Material<TShader>> material) { material_ = std::move(material); }
        bool hasMaterial() const { return material_ != nullptr; }

        bool visible() const { return visible_; }
        void setVisible(bool v) { visible_ = v; }

        /**
         * Renderer-invoked hook to contribute per-frame, per-instance state to this dispatch.
         *
         * Called after the renderer has allocated the descriptor set and bound the implicit
         * render-target image at binding 0 (subclasses must not overwrite that). Subclasses may:
         *   - call writer.writeStorageBuffer / writeStorageImage / writeUniformBuffer at
         *     additional bindings (1, 2, …) to wire per-object resources;
         *   - call pc.write(MyPushConstantStruct{…}) to populate push constants for this dispatch.
         *
         * Both contributions are optional — the default implementation does nothing, which is
         * correct for a material that only consumes the implicit render-target binding.
         */
        virtual void prepareFrame(VulkanDescriptorWriter &writer, VkDescriptorSet set, PushConstantBuilder &pc,
                                  const FrameContext &frame);

    private:
        std::shared_ptr<Material<TShader>> material_;
        bool visible_ = true;
    };

    template <GpuShaderModule TShader>
    void RenderableNode3D<TShader>::prepareFrame(VulkanDescriptorWriter & /*writer*/, VkDescriptorSet /*set*/,
                                                 PushConstantBuilder & /*pc*/, const FrameContext & /*frame*/) {}
}
