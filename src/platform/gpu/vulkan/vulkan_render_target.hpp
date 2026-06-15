#pragma once

#include <cstdint>
#include "gpu_types.hpp"
#include "vulkan_image.hpp"

namespace RAGE {
    class VulkanAllocator;

    struct VulkanRenderTargetCreateInfo {
        uint32_t width = 0;
        uint32_t height = 0;
        ImageFormat format = ImageFormat::RGBA8_UNORM;
        ImageUsage usage = ImageUsage::Storage;
    };

    /**
     * A renderable image plus its default view, with caller-tracked Vulkan layout.
     *
     * Wraps a VulkanImage (allocated through VulkanAllocator) and a single 2D image view covering
     * the full subresource range. The lifetime category — per-frame versus persistent — is the
     * owner's responsibility, not the render target's: the renderer recreates per-frame targets
     * on resize and drops them at frame teardown; persistent targets (accumulation buffers, etc.)
     * are held by scene objects and survive across frames, resetting only on scene change.
     *
     * currentLayout() / setCurrentLayout() track the most recent layout the caller asked Vulkan
     * to transition this image to. It is a record-keeping convenience for inserting subsequent
     * pipeline barriers; the GPU's real layout is whatever the most recent barrier said it is.
     *
     * Move-only. The underlying VulkanImage and view are destroyed when this object goes out of
     * scope.
     *
     * @note The image is created with MemoryLocation::GpuOnly. ImageUsage::Storage is included
     *       implicitly so compute shaders can write to the target.
     */
    class VulkanRenderTarget {
    public:
        VulkanRenderTarget() = delete;
        VulkanRenderTarget(VulkanAllocator &allocator, const VulkanRenderTargetCreateInfo &info);

        VulkanRenderTarget(const VulkanRenderTarget &) = delete;
        VulkanRenderTarget &operator=(const VulkanRenderTarget &) = delete;
        VulkanRenderTarget(VulkanRenderTarget &&) noexcept = default;
        VulkanRenderTarget &operator=(VulkanRenderTarget &&) noexcept = default;

        const VulkanImage &image() const { return image_; }
        VulkanImage &image() { return image_; }
        const VulkanImageView &view() const { return view_; }

        uint32_t width() const { return width_; }
        uint32_t height() const { return height_; }
        ImageFormat format() const { return format_; }

        ImageLayout currentLayout() const { return currentLayout_; }
        void setCurrentLayout(ImageLayout layout) { currentLayout_ = layout; }

    private:
        static VulkanImage createBackingImage(VulkanAllocator &allocator, const VulkanRenderTargetCreateInfo &info);

        VulkanImage image_;
        VulkanImageView view_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
        ImageFormat format_ = ImageFormat::RGBA8_UNORM;
        ImageLayout currentLayout_ = ImageLayout::Undefined;
    };
}
