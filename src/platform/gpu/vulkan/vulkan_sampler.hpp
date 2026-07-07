#pragma once

#include <utility>
#include <vulkan/vulkan.h>

namespace RAGE {
    class VulkanDescriptorWriter;

    /**
     * @brief RAII wrapper for a `VkSampler`. Currently exposes the one config the
     *        engine needs — nearest filtering with clamp-to-edge addressing, the
     *        correct mode for `texelFetch`-style integer-texture lookups. Grow the
     *        factory surface as new sampling needs appear rather than mirroring the
     *        full `VkSamplerCreateInfo`.
     */
    class VulkanSampler {
    public:
        VulkanSampler() = default;
        ~VulkanSampler() { destroy(); }

        VulkanSampler(const VulkanSampler &) = delete;
        VulkanSampler &operator=(const VulkanSampler &) = delete;
        VulkanSampler(VulkanSampler &&other) noexcept { swap(other); }
        VulkanSampler &operator=(VulkanSampler &&other) noexcept {
            if (this != &other) {
                destroy();
                swap(other);
            }
            return *this;
        }

        /// Nearest min/mag filter, clamp-to-edge on all axes, no mips/anisotropy.
        static VulkanSampler createNearestClamp(VkDevice device);

        bool valid() const { return sampler_ != VK_NULL_HANDLE; }

    private:
        friend class VulkanDescriptorWriter;

        VulkanSampler(VkDevice device, VkSampler sampler)
            : device_(device)
            , sampler_(sampler) {}

        void destroy() noexcept {
            if (sampler_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
                vkDestroySampler(device_, sampler_, nullptr);
                sampler_ = VK_NULL_HANDLE;
            }
        }

        void swap(VulkanSampler &other) noexcept {
            std::swap(device_, other.device_);
            std::swap(sampler_, other.sampler_);
        }

        VkDevice device_ = VK_NULL_HANDLE;
        VkSampler sampler_ = VK_NULL_HANDLE;
    };
}
