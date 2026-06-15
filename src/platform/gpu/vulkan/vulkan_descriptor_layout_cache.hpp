#pragma once

#include <cstddef>
#include <span>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
#include "vulkan_shader_module.hpp"

namespace RAGE {
    /**
     * Caches VkDescriptorSetLayout objects keyed by their binding layout.
     *
     * Pipelines query the cache with a span of ShaderDescriptorBinding entries (typically derived
     * from reflection in VulkanShaderModule); the cache returns the existing layout for that
     * binding shape or creates and stores a new one. Bindings are normalised by sorting on the
     * binding index before hashing, so two equivalent layouts that differ only in input order
     * still hit the same cache entry.
     *
     * The cache owns every layout it creates and destroys them on its own destruction. Cached
     * layouts must not outlive the cache. Single-threaded by contract.
     */
    class VulkanDescriptorSetLayoutCache {
    public:
        VulkanDescriptorSetLayoutCache() = delete;
        explicit VulkanDescriptorSetLayoutCache(VkDevice device);
        ~VulkanDescriptorSetLayoutCache();

        VulkanDescriptorSetLayoutCache(const VulkanDescriptorSetLayoutCache &) = delete;
        VulkanDescriptorSetLayoutCache &operator=(const VulkanDescriptorSetLayoutCache &) = delete;
        VulkanDescriptorSetLayoutCache(VulkanDescriptorSetLayoutCache &&) = delete;
        VulkanDescriptorSetLayoutCache &operator=(VulkanDescriptorSetLayoutCache &&) = delete;

        VkDescriptorSetLayout getOrCreate(std::span<const ShaderDescriptorBinding> bindings);

        size_t size() const { return cache_.size(); }

    private:
        struct Key {
            std::vector<ShaderDescriptorBinding> bindings;
            bool operator==(const Key &other) const;
        };

        struct KeyHash {
            size_t operator()(const Key &k) const noexcept;
        };

        VkDevice device_ = VK_NULL_HANDLE;
        std::unordered_map<Key, VkDescriptorSetLayout, KeyHash> cache_;
    };
}
