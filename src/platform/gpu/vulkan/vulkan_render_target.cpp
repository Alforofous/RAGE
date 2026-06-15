#include "vulkan_render_target.hpp"
#include "vulkan_allocator.hpp"

namespace RAGE {
    VulkanImage VulkanRenderTarget::createBackingImage(VulkanAllocator &allocator,
                                                       const VulkanRenderTargetCreateInfo &info) {
        ImageCreateInfo ici{};
        ici.width = info.width;
        ici.height = info.height;
        ici.depth = 1;
        ici.format = info.format;
        ici.usage = info.usage | ImageUsage::Storage;
        ici.memory = MemoryLocation::GpuOnly;

        return allocator.createImage(ici);
    }

    VulkanRenderTarget::VulkanRenderTarget(VulkanAllocator &allocator, const VulkanRenderTargetCreateInfo &info)
        : image_(createBackingImage(allocator, info))
        , view_(image_.createView({}))
        , width_(info.width)
        , height_(info.height)
        , format_(info.format) {}
}
