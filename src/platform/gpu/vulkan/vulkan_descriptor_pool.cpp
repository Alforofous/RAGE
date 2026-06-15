#include "vulkan_descriptor_pool.hpp"
#include <array>
#include <stdexcept>

namespace RAGE {
    VulkanDescriptorPool::VulkanDescriptorPool(VkDevice device, const VulkanDescriptorPoolSizes &sizes)
        : device_(device) {
        if (device_ == VK_NULL_HANDLE) {
            throw std::runtime_error("VulkanDescriptorPool: null device");
        }

        const std::array<VkDescriptorPoolSize, 6> poolSizes{ {
            { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = sizes.uniformBuffers },
            { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = sizes.storageBuffers },
            { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = sizes.storageImages },
            { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = sizes.combinedImageSamplers },
            { .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = sizes.sampledImages },
            { .type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = sizes.samplers },
        } };

        VkDescriptorPoolCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        ci.maxSets = sizes.maxSets;
        ci.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        ci.pPoolSizes = poolSizes.data();

        if (vkCreateDescriptorPool(device_, &ci, nullptr, &pool_) != VK_SUCCESS) {
            throw std::runtime_error("VulkanDescriptorPool: vkCreateDescriptorPool failed");
        }
    }

    VulkanDescriptorPool::~VulkanDescriptorPool() {
        if (pool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, pool_, nullptr);
        }
    }

    VkDescriptorSet VulkanDescriptorPool::allocate(VkDescriptorSetLayout layout) {
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = pool_;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &layout;

        VkDescriptorSet set = VK_NULL_HANDLE;
        const VkResult r = vkAllocateDescriptorSets(device_, &ai, &set);
        if (r == VK_ERROR_OUT_OF_POOL_MEMORY || r == VK_ERROR_FRAGMENTED_POOL) {
            throw std::runtime_error("VulkanDescriptorPool: pool exhausted; increase pool sizes");
        }
        if (r != VK_SUCCESS) {
            throw std::runtime_error("VulkanDescriptorPool: vkAllocateDescriptorSets failed");
        }

        allocatedCount_++;

        return set;
    }

    void VulkanDescriptorPool::reset() {
        if (vkResetDescriptorPool(device_, pool_, 0) != VK_SUCCESS) {
            throw std::runtime_error("VulkanDescriptorPool: vkResetDescriptorPool failed");
        }
        allocatedCount_ = 0;
    }
}
