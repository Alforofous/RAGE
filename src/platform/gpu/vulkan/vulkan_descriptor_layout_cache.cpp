#include "vulkan_descriptor_layout_cache.hpp"
#include <algorithm>
#include <functional>
#include <stdexcept>

namespace RAGE {
    bool VulkanDescriptorSetLayoutCache::Key::operator==(const Key &other) const {
        if (bindings.size() != other.bindings.size()) {
            return false;
        }
        for (size_t i = 0; i < bindings.size(); ++i) {
            const auto &a = bindings[i];
            const auto &b = other.bindings[i];
            if (a.set != b.set || a.binding != b.binding || a.type != b.type || a.count != b.count ||
                a.stages != b.stages) {
                return false;
            }
        }

        return true;
    }

    size_t VulkanDescriptorSetLayoutCache::KeyHash::operator()(const Key &k) const noexcept {
        size_t h = 0;
        const std::hash<uint32_t> hasher;
        for (const auto &b : k.bindings) {
            h ^= hasher(b.set) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= hasher(b.binding) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= hasher(static_cast<uint32_t>(b.type)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= hasher(b.count) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= hasher(b.stages) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }

        return h;
    }

    VulkanDescriptorSetLayoutCache::VulkanDescriptorSetLayoutCache(VkDevice device)
        : device_(device) {
        if (device_ == VK_NULL_HANDLE) {
            throw std::runtime_error("VulkanDescriptorSetLayoutCache: null device");
        }
    }

    VulkanDescriptorSetLayoutCache::~VulkanDescriptorSetLayoutCache() {
        for (const auto &[key, layout] : cache_) {
            vkDestroyDescriptorSetLayout(device_, layout, nullptr);
        }
    }

    VkDescriptorSetLayout
    VulkanDescriptorSetLayoutCache::getOrCreate(std::span<const ShaderDescriptorBinding> bindings) {
        Key key;
        key.bindings.assign(bindings.begin(), bindings.end());
        std::ranges::sort(key.bindings, [](const ShaderDescriptorBinding &a, const ShaderDescriptorBinding &b) {
            return a.binding < b.binding;
        });

        const auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }

        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        vkBindings.reserve(key.bindings.size());
        for (const auto &b : key.bindings) {
            VkDescriptorSetLayoutBinding vb{};
            vb.binding = b.binding;
            vb.descriptorType = b.type;
            vb.descriptorCount = b.count;
            vb.stageFlags = b.stages;
            vkBindings.push_back(vb);
        }

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<uint32_t>(vkBindings.size());
        ci.pBindings = vkBindings.data();

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(device_, &ci, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error("VulkanDescriptorSetLayoutCache: vkCreateDescriptorSetLayout failed");
        }

        cache_.emplace(std::move(key), layout);

        return layout;
    }
}
