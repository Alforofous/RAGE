#include "vulkan_sampler.hpp"

#include <stdexcept>

namespace RAGE {
    VulkanSampler VulkanSampler::createNearestClamp(VkDevice device) {
        if (device == VK_NULL_HANDLE) {
            throw std::runtime_error("VulkanSampler: null device");
        }

        VkSamplerCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        ci.magFilter = VK_FILTER_NEAREST;
        ci.minFilter = VK_FILTER_NEAREST;
        ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        ci.maxLod = 0.0f;

        VkSampler sampler = VK_NULL_HANDLE;
        if (vkCreateSampler(device, &ci, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("VulkanSampler: vkCreateSampler failed");
        }
        return VulkanSampler(device, sampler);
    }
}
